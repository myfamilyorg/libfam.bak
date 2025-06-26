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
#include <connection.H>
#include <error.H>
#include <event.H>
#include <format.H>
#include <lock.H>
#include <misc.H>
#include <socket.H>
#include <syscall_const.H>

#define MIN_EXCESS 1024
#define MIN_RESIZE (MIN_EXCESS * 2)

bool debug_force_write_buffer = false;

typedef struct {
	OnRecvFn on_recv;
	OnAcceptFn on_accept;
	OnCloseFn on_close;
	u16 port;
	u64 connection_alloc_overhead;
} AcceptorData;

typedef struct {
	OnRecvFn on_recv;
	OnCloseFn on_close;
	i32 mplex;
	Lock lock;
	bool is_closed;
	u8 *rbuf;
	u64 rbuf_capacity;
	u64 rbuf_offset;
	u8 *wbuf;
	u64 wbuf_capacity;
	u64 wbuf_offset;

} ConnectionCommon;

typedef struct {
	ConnectionCommon common;
} InboundData;

typedef struct {
	OnConnectFn on_connect;
	OnConnectErrorFn on_connect_error;
	ConnectionCommon common;
} OutboundData;

struct Connection {
	ConnectionType conn_type;
	i32 socket;
	union {
		AcceptorData acceptor;
		InboundData inbound;
		OutboundData outbound;
	} data;
};

STATIC ConnectionCommon *connection_common(Connection *connection) {
	if (connection->conn_type != Inbound &&
	    connection->conn_type != Outbound) {
		err = EINVAL;
		return NULL;
	}
	return connection->conn_type == Inbound
		   ? &connection->data.inbound.common
		   : &connection->data.outbound.common;
}

Connection *evh_acceptor(const u8 addr[4], u16 port, u16 backlog,
			 OnRecvFn on_recv, OnAcceptFn on_accept,
			 OnCloseFn on_close, u64 connection_alloc_overhead) {
	i32 pval;
	Connection *conn = alloc(sizeof(Connection));
	if (conn == NULL) return NULL;
	conn->conn_type = Acceptor;
	conn->data.acceptor.on_accept = on_accept;
	conn->data.acceptor.on_recv = on_recv;
	conn->data.acceptor.on_close = on_close;
	conn->data.acceptor.connection_alloc_overhead =
	    connection_alloc_overhead;
	pval = socket_listen(&conn->socket, addr, port, backlog);
	if (pval < 0) {
		release(conn);
		return NULL;
	}
	conn->data.acceptor.port = pval;
	return conn;
}

u16 evh_acceptor_port(const Connection *conn) {
	if (conn->conn_type == Acceptor) return conn->data.acceptor.port;
	err = EINVAL;
	return 0;
}

Connection *evh_client(const u8 addr[4], u16 port, OnRecvFn on_recv,
		       OnConnectFn on_connect,
		       OnConnectErrorFn on_connect_error, OnCloseFn on_close,
		       u64 connection_alloc_overhead) {
	i32 ret;
	Connection *client =
	    alloc(sizeof(Connection) + connection_alloc_overhead);
	if (client == NULL) return NULL;
	client->conn_type = Outbound;
	client->data.outbound.common.on_recv = on_recv;
	client->data.outbound.common.on_close = on_close;
	client->data.outbound.common.lock = LOCK_INIT;
	client->data.outbound.common.is_closed = false;
	client->data.outbound.common.rbuf = NULL;
	client->data.outbound.common.rbuf_capacity = 0;
	client->data.outbound.common.rbuf_offset = 0;
	client->data.outbound.common.wbuf = NULL;
	client->data.outbound.common.wbuf_capacity = 0;
	client->data.outbound.common.wbuf_offset = 0;
	client->data.outbound.on_connect = on_connect;
	client->data.outbound.on_connect_error = on_connect_error;
	ret = socket_connect(&client->socket, addr, port);
	if (ret < 0 && err != EINPROGRESS) {
		release(client);
		return NULL;
	}
	return client;
}

Connection *evh_accepted(i32 fd, OnRecvFn on_recv, OnCloseFn on_close,
			 u64 connection_alloc_overhead) {
	Connection *nconn =
	    alloc(sizeof(Connection) + connection_alloc_overhead);
	if (!nconn) return NULL;
	nconn->conn_type = Inbound;
	nconn->socket = fd;
	nconn->data.inbound.common.on_recv = on_recv;
	nconn->data.inbound.common.on_close = on_close;
	nconn->data.inbound.common.lock = LOCK_INIT;
	nconn->data.inbound.common.is_closed = false;
	nconn->data.inbound.common.rbuf_capacity = 0;
	nconn->data.inbound.common.rbuf_offset = 0;
	nconn->data.inbound.common.wbuf_capacity = 0;
	nconn->data.inbound.common.wbuf_offset = 0;
	nconn->data.inbound.common.wbuf = nconn->data.inbound.common.rbuf =
	    NULL;
	return nconn;
}

i32 connection_write(Connection *conn, const void *buf, u64 len) {
	i64 wlen = 0;
	ConnectionCommon *common = connection_common(conn);
	if (!common) {
		err = EINVAL;
		return -1;
	}
	{
		LockGuard lg = wlock(&common->lock);
		if (common->is_closed) {
			err = EIO;
			return -1;
		}
		if (!common->wbuf_offset) {
			u64 offset = 0;
		write_block:
			if (debug_force_write_buffer)
				wlen = 0;
			else
				wlen = write(conn->socket, (u8 *)buf + offset,
					     len);
			if (wlen < 0 && err == EINTR) {
				if (wlen > 0) offset += wlen;
				goto write_block;
			} else if (wlen < 0 && err == EAGAIN)
				wlen = 0;
			else if (wlen < 0 && err) {
				shutdown(conn->socket, SHUT_RDWR);
				common->is_closed = true;
				return -1;
			}

			if ((u64)wlen == len) return 0;
			if (mregister(
				common->mplex, conn->socket,
				MULTIPLEX_FLAG_READ | MULTIPLEX_FLAG_WRITE,
				conn) == -1) {
				shutdown(conn->socket, SHUT_RDWR);
				common->is_closed = true;
				return -1;
			}
		}

		if (common->wbuf_offset + len - wlen > common->wbuf_capacity) {
			void *tmp = resize(common->wbuf,
					   common->wbuf_offset + len - wlen);
			if (!tmp) {
				shutdown(conn->socket, SHUT_RDWR);
				common->is_closed = true;
				return -1;
			}
			common->wbuf = tmp;
			common->wbuf_capacity =
			    common->wbuf_offset + len - wlen;
		}
		memcpy(common->wbuf + common->wbuf_offset, (u8 *)buf + wlen,
		       len - wlen);
		common->wbuf_offset += len - wlen;
	}

	return 0;
}

i32 connection_write_complete(Connection *connection) {
	ConnectionCommon *common = connection_common(connection);
	i64 wlen;
	u64 cur = 0;
	i32 sock;

	if (!common) {
		err = EINVAL;
		return -1;
	}
	sock = connection->socket;
	{
		LockGuard lg = wlock(&common->lock);
		if (common->is_closed) return -1;

		while (cur < common->wbuf_offset) {
			wlen = write(sock, common->wbuf + cur,
				     common->wbuf_offset - cur);
			if (wlen < 0) {
				shutdown(sock, SHUT_RDWR);
				return -1;
			}
			cur += wlen;
		}

		if (cur == common->wbuf_offset) {
			if (mregister(common->mplex, sock, MULTIPLEX_FLAG_READ,
				      connection) == -1) {
				shutdown(sock, SHUT_RDWR);
				return -1;
			}
			if (common->wbuf_capacity) {
				release(common->wbuf);
				common->wbuf_offset = 0;
				common->wbuf_capacity = 0;
			}
		} else {
			memorymove(common->wbuf, common->wbuf + cur,
				   common->wbuf_offset - cur);
			common->wbuf_offset -= cur;
		}
	}
	return 0;
}

i32 connection_close(Connection *connection) {
	if (connection->conn_type == Acceptor) {
		return close(connection->socket);
	} else {
		ConnectionCommon *common = connection_common(connection);
		if (!common) return -1;
		LockGuard lg = wlock(&common->lock);
		if (common->is_closed) return -1;
		common->is_closed = true;
		return shutdown(connection->socket, SHUT_RDWR);
	}
}

void connection_clear_rbuf_through(Connection *conn, u64 off) {
	ConnectionCommon *common = connection_common(conn);
	if (!common || off > common->rbuf_offset) return;
	memorymove(common->rbuf, common->rbuf + off, common->rbuf_offset - off);
	common->rbuf_offset -= off;
}

void connection_clear_rbuf(Connection *conn) {
	ConnectionCommon *common = connection_common(conn);
	if (common) common->rbuf_offset = 0;
}

u8 *connection_rbuf(Connection *conn) {
	ConnectionCommon *common = connection_common(conn);
	if (!common) return NULL;
	return common->rbuf;
}

u64 connection_rbuf_offset(Connection *conn) {
	ConnectionCommon *common = connection_common(conn);
	if (!common) return 0;
	return common->rbuf_offset;
}

i32 connection_set_rbuf_offset(Connection *conn, u64 noffset) {
	ConnectionCommon *common = connection_common(conn);
	if (!common) return -1;
	common->rbuf_offset = noffset;
	return 0;
}

u64 connection_rbuf_capacity(Connection *conn) {
	ConnectionCommon *common = connection_common(conn);
	if (!common) return 0;
	return common->rbuf_capacity;
}

ConnectionType connection_type(Connection *conn) { return conn->conn_type; }

i32 connection_set_mplex(Connection *conn, i32 mplex) {
	ConnectionCommon *common = connection_common(conn);
	if (!common) {
		err = EINVAL;
		return -1;
	}
	common->mplex = mplex;
	return 0;
}

i32 connection_socket(Connection *conn) { return conn->socket; }
i64 connection_alloc_overhead(Connection *conn) {
	if (!conn || conn->conn_type != Acceptor) return -1;
	return conn->data.acceptor.connection_alloc_overhead;
}

OnRecvFn connection_on_recv(Connection *conn) {
	if (conn->conn_type == Acceptor) {
		return conn->data.acceptor.on_recv;
	} else {
		ConnectionCommon *common = connection_common(conn);
		return common->on_recv;
	}
}
OnCloseFn connection_on_close(Connection *conn) {
	if (conn->conn_type == Acceptor) {
		return conn->data.acceptor.on_close;
	} else {
		ConnectionCommon *common = connection_common(conn);
		return common->on_close;
	}
}

OnAcceptFn connection_on_accept(Connection *conn) {
	if (conn->conn_type != Acceptor) {
		err = EINVAL;
		return NULL;
	}
	return conn->data.acceptor.on_accept;
}

i32 connection_check_capacity(Connection *conn) {
	ConnectionCommon *common = connection_common(conn);
	if (!common) return 0;

	if (common->rbuf_offset + MIN_EXCESS > common->rbuf_capacity) {
		void *tmp =
		    resize(common->rbuf, common->rbuf_offset + MIN_RESIZE);
		if (tmp == NULL) return -1;
		common->rbuf = tmp;
		common->rbuf_capacity = common->rbuf_offset + MIN_RESIZE;
	}

	return 0;
}

void connection_release(Connection *conn) {
	ConnectionCommon *common = connection_common(conn);
	if (common) {
		LockGuard lg = wlock(&common->lock);
		if (common->rbuf_capacity) release(common->rbuf);
		common->rbuf_capacity = common->rbuf_offset = 0;
		common->is_closed = true;
		if (common->wbuf_capacity) release(common->wbuf);
		common->wbuf_capacity = common->wbuf_offset = 0;
	}
	close(conn->socket);
	release(conn);
}
