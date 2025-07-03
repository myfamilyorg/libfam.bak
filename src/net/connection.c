/********************************************************************************
 * MIT License
 *
 * Copyright (c) 2025 Christopher Gilliard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *******************************************************************************/

#include <alloc.H>
#include <atomic.H>
#include <connection_internal.H>
#include <error.H>
#include <event.H>
#include <format.H>
#include <lock.H>
#include <misc.H>
#include <rbtree.H>
#include <socket.H>
#include <syscall_const.H>
#include <vec.H>

#define MIN_EXCESS 1024
#define MIN_RESIZE (MIN_EXCESS * 2)

bool _debug_force_write_buffer = false;
bool _debug_force_write_error = false;
i32 _debug_write_error_code = EIO;
u64 _debug_connection_wmax = 0;

typedef struct {
	u16 port;
	u32 connection_alloc_overhead;
} AcceptorData;

typedef struct {
	i32 mplex;
	Lock lock;
	Vec *rbuf;
	Vec *wbuf;
} ConnectionData;

struct Connection {
	u8 reserved[sizeof(RbTreeNode)]; /* Reserved for Red-black tree data */
	u32 flags;
	i32 socket;
	union {
		AcceptorData acceptor_data;
		ConnectionData conn_data;
	} data;
};

Connection *connection_acceptor(const u8 addr[4], u16 port, u16 backlog,
				u32 connection_alloc_overhead) {
	i32 pval;
	Connection *conn = alloc(sizeof(Connection));
	if (conn == NULL) return NULL;
	conn->flags = CONN_FLAG_ACCEPTOR;
	conn->data.acceptor_data.connection_alloc_overhead =
	    connection_alloc_overhead;
	pval = socket_listen(&conn->socket, addr, port, backlog);
	if (pval < 0) {
		release(conn);
		return NULL;
	}
	conn->data.acceptor_data.port = pval;
	return conn;
}

i32 connection_acceptor_port(const Connection *conn) {
	if ((conn->flags & CONN_FLAG_ACCEPTOR) == 0) {
		err = EINVAL;
		return -1;
	}
	return conn->data.acceptor_data.port;
}

Connection *connection_client(const u8 addr[4], u16 port,
			      u32 connection_alloc_overhead) {
	i32 ret;
	Connection *client =
	    alloc(sizeof(Connection) + connection_alloc_overhead);
	if (client == NULL) return NULL;
	client->flags = CONN_FLAG_OUTBOUND;
	client->data.conn_data.wbuf = client->data.conn_data.rbuf = NULL;
	client->data.conn_data.mplex = -1;
	client->data.conn_data.lock = LOCK_INIT;
	ret = socket_connect(&client->socket, addr, port);
	if (ret < 0 && err != EINPROGRESS) {
		release(client);
		return NULL;
	}
	return client;
}

Connection *connection_accepted(i32 fd, i32 mplex,
				u32 connection_alloc_overhead) {
	Connection *nconn =
	    alloc(sizeof(Connection) + connection_alloc_overhead);
	if (!nconn) return NULL;
	nconn->flags = CONN_FLAG_INBOUND;
	nconn->socket = fd;
	nconn->data.conn_data.wbuf = nconn->data.conn_data.rbuf = NULL;
	nconn->data.conn_data.mplex = mplex;
	nconn->data.conn_data.lock = LOCK_INIT;
	return nconn;
}

i32 connection_write(Connection *conn, const void *buf, u64 len) {
	i64 wlen = 0;
	u64 capacity, elements;
	ConnectionData *conn_data = &conn->data.conn_data;

	if (conn->flags & CONN_FLAG_ACCEPTOR) {
		err = EINVAL;
		return -1;
	}
	{
		LockGuard lg = wlock(&conn_data->lock);
		if (conn->flags & CONN_FLAG_CLOSED) {
			err = EIO;
			return -1;
		}
		if (!conn_data->wbuf) {
		write_block:
			if (_debug_force_write_error) {
				wlen = -1;
				err = _debug_write_error_code;
			} else if (_debug_force_write_buffer)
				wlen = 0;
			else
				wlen = write(conn->socket, buf, len);
			if (wlen < 0 && err == EINTR) {
				_debug_force_write_error = false;
				goto write_block;
			} else if (wlen < 0 && err == EAGAIN)
				wlen = 0;
			else if (wlen < 0 && err) {
				shutdown(conn->socket, SHUT_RD);
				conn->flags |= CONN_FLAG_CLOSED;
				return -1;
			}
			if ((u64)wlen == len) return 0;
			if (mregister(
				conn_data->mplex, conn->socket,
				MULTIPLEX_FLAG_READ | MULTIPLEX_FLAG_WRITE,
				conn) == -1) {
				shutdown(conn->socket, SHUT_RD);
				conn->flags |= CONN_FLAG_CLOSED;
				return -1;
			}
		}
		capacity = vec_capacity(conn_data->wbuf);
		elements = vec_elements(conn_data->wbuf);
		if (capacity < (len + elements) - wlen) {
			Vec *tmp = vec_resize(conn_data->wbuf, len + elements);
			if (!tmp) {
				shutdown(conn->socket, SHUT_RD);
				conn->flags |= CONN_FLAG_CLOSED;
				return -1;
			}
			conn_data->wbuf = tmp;
			vec_extend(conn_data->wbuf, (u8 *)buf + wlen,
				   len - wlen);
		}
	}
	return 0;
}

i32 connection_write_complete(Connection *conn) {
	ConnectionData *conn_data = &conn->data.conn_data;
	i64 wlen;
	u64 cur = 0;
	i32 sock;

	if (conn->flags & CONN_FLAG_ACCEPTOR) {
		err = EINVAL;
		return -1;
	}
	sock = conn->socket;
	{
		LockGuard lg = wlock(&conn_data->lock);
		u64 elems = vec_elements(conn_data->wbuf);
		if (conn->flags & CONN_FLAG_CLOSED) {
			err = EIO;
			return -1;
		}

		while (cur < elems) {
			if (_debug_force_write_error) {
				wlen = -1;
				err = _debug_write_error_code;
			} else {
				u64 wmax = !_debug_connection_wmax
					       ? elems - cur
					       : _debug_connection_wmax;
				wlen = write(
				    sock, (u8 *)vec_data(conn_data->wbuf) + cur,
				    wmax);
			}
			if (wlen < 0 && err == EAGAIN) break;
			if (wlen < 0 && err == EINTR) {
				_debug_force_write_error = false;
				continue;
			}
			if (wlen < 0) {
				shutdown(sock, SHUT_RD);
				conn->flags |= CONN_FLAG_CLOSED;
				return -1;
			}
			cur += wlen;
			if (_debug_connection_wmax) break;
		}

		if (cur == elems) {
			if (mregister(conn_data->mplex, sock,
				      MULTIPLEX_FLAG_READ, conn) < 0) {
				shutdown(sock, SHUT_RD);
				conn->flags |= CONN_FLAG_CLOSED;
				return -1;
			}
			vec_release(conn_data->wbuf);
			conn_data->wbuf = NULL;
		} else {
			u8 *wbuf = vec_data(conn_data->wbuf);
			memorymove(wbuf, wbuf + cur, elems - cur);
			vec_truncate(conn_data->wbuf, elems - cur);
		}
	}
	return 0;
}

i32 connection_close(Connection *conn) {
	ConnectionType ctype = connection_type(conn);

	if (ctype == Acceptor) {
		u32 flags = ALOAD(&conn->flags);
		u32 expected = flags & ~CONN_FLAG_CLOSED;
		if (__cas32(&conn->flags, &expected,
			    flags | CONN_FLAG_CLOSED)) {
			return close(conn->socket);
		} else {
			err = EALREADY;
			return -1;
		}
	} else {
		ConnectionData *conn_data = &conn->data.conn_data;
		LockGuard lg = wlock(&conn_data->lock);
		if (conn->flags & CONN_FLAG_CLOSED) {
			err = EALREADY;
			return -1;
		}
		conn->flags |= CONN_FLAG_CLOSED;
		return shutdown(conn->socket, SHUT_RD);
	}
}

Vec *connection_rbuf(Connection *conn) {
	if (conn->flags & CONN_FLAG_ACCEPTOR) {
		err = EINVAL;
		return NULL;
	}
	return conn->data.conn_data.rbuf;
}
Vec *connection_wbuf(Connection *conn) {
	if (conn->flags & CONN_FLAG_ACCEPTOR) {
		err = EINVAL;
		return NULL;
	}
	return conn->data.conn_data.wbuf;
}

ConnectionType connection_type(Connection *conn) {
	if (conn->flags & CONN_FLAG_ACCEPTOR)
		return Acceptor;
	else if (conn->flags & CONN_FLAG_INBOUND)
		return Inbound;
	else
		return Outbound;
}

i32 connection_set_mplex(Connection *conn, i32 mplex) {
	if (conn->flags & CONN_FLAG_ACCEPTOR) {
		err = EINVAL;
		return -1;
	}
	conn->data.conn_data.mplex = mplex;
	return 0;
}

i32 connection_socket(Connection *conn) { return conn->socket; }

i64 connection_alloc_overhead(Connection *conn) {
	if ((conn->flags & CONN_FLAG_ACCEPTOR) == 0) {
		err = EINVAL;
		return -1;
	}
	return conn->data.acceptor_data.connection_alloc_overhead;
}

void connection_release(Connection *conn) {
	ConnectionType ctype = connection_type(conn);
	connection_close(conn);
	if (ctype == Inbound || ctype == Outbound) {
		if (conn->data.conn_data.wbuf)
			release(conn->data.conn_data.wbuf);
		if (conn->data.conn_data.rbuf)
			release(conn->data.conn_data.rbuf);
	}
	release(conn);
}

bool connection_is_connected(Connection *conn) {
	return (ALOAD(&conn->flags) & CONN_FLAG_CONNECT_COMPLETE) != 0;
}

void connection_set_is_connected(Connection *conn) {
	u32 flags = ALOAD(&conn->flags);
	if ((flags & CONN_FLAG_ACCEPTOR) ||
	    (flags & CONN_FLAG_CONNECT_COMPLETE))
		return;
	__cas32(&conn->flags, &flags, flags | CONN_FLAG_CONNECT_COMPLETE);
}

void connection_set_rbuf(Connection *conn, Vec *v) {
	ConnectionType ct = connection_type(conn);
	if (ct == Inbound || ct == Outbound) conn->data.conn_data.rbuf = v;
}

u64 connection_size(void) { return sizeof(Connection); }

