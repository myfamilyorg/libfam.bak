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

#include <libfam/alloc.H>
#include <libfam/atomic.H>
#include <libfam/connection.H>
#include <libfam/error.H>
#include <libfam/evh.H>
#include <libfam/format.H>
#include <libfam/lock.H>
#include <libfam/rbtree.H>
#include <libfam/sha1.H>
#include <libfam/ws.H>

#define CONNECTION_SIZE 56

static const u8 *CLIENT_INIT_PREFIX =
    "GET / HTTP/1.1\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n\r\n";

static const u8 *BAD_REQUEST =
    "HTTP/1.1 400 Bad Request\r\n\
Connection: close\r\n\
Content-Length: 0\r\n\
\r\n";

static const u8 *SWITCHING_PROTOS =
    "HTTP/1.1 101 Switching Protocols\r\n\
Upgrade: websocket\r\n\
Connection: Upgrade\r\n\
Sec-WebSocket-Accept: ";

typedef struct {
	u16 id;
	Ws *ws;
	Evh *evh;
	RbTree connections;
	Lock lock;
} WsContext;

struct Ws {
	WsContext *ctxs;
	WsConfig config;
	u64 next_id;
	Connection *acceptor;
};

struct WsConnection {
	u8 reserved[CONNECTION_SIZE];
	u64 id;
};

STATIC void __attribute__((constructor)) _check_connection_size(void) {
	if (connection_size() != CONNECTION_SIZE)
		panic("expected connection size {}. actual {}.",
		      CONNECTION_SIZE, connection_size());
	if (sizeof(WsConnection) > 64)
		panic("WsConnection > 64 bytes {}!", sizeof(WsConnection));
}

STATIC i32 ws_rbtree_search(RbTreeNode *cur, const RbTreeNode *value,
			    RbTreeNodePair *retval) {
	while (cur) {
		u64 v1 = ((WsConnection *)cur)->id;
		u64 v2 = ((WsConnection *)value)->id;
		if (v1 == v2) {
			retval->self = cur;
			break;
		} else if (v1 < v2) {
			retval->parent = cur;
			retval->is_right = 1;
			cur = cur->right;
		} else {
			retval->parent = cur;
			retval->is_right = 0;
			cur = cur->left;
		}
		retval->self = cur;
	}
	return 0;
}

STATIC i32 proc_message_single(Ws *ws, WsConnection *wsconn, u64 offset,
			       u64 len, u8 op, bool fin) {
	WsMessage msg;
	Vec *rbuf = connection_rbuf((Connection *)wsconn);
	u8 *data = vec_data(rbuf);
	msg.buffer = data + offset;
	msg.len = len;
	msg.op = op;
	msg.fin = fin;
	ws->config.on_message(ws, wsconn, &msg);
	return 0;
}

STATIC i32 ws_proc_handshake_client(WsConnection *wsconn) {
	Vec *rbuf = connection_rbuf((Connection *)wsconn);
	u64 rbuf_offset = vec_elements(rbuf);
	u8 *data = vec_data(rbuf);
	u8 *end;

	if ((end = substrn(data, "\r\n\r\n", rbuf_offset))) {
		u64 len = (u64)end - (u64)data;
		if (substrn(data, "Upgrade: websocket", len)) {
			if (len + 4 < rbuf_offset)
				memorymove(data, (u8 *)((u64)data + len + 4),
					   rbuf_offset - (len + 4));
			vec_truncate(rbuf, rbuf_offset - (len + 4));
			return 0;
		} else {
			err = EPROTO;
			return -1;
		}
	} else {
		err = EAGAIN;
		return -1;
	}
}

STATIC i32 ws_proc_handshake_server(WsConnection *wsconn) {
	Vec *rbuf = connection_rbuf((Connection *)wsconn);
	u64 rbuf_offset = vec_elements(rbuf);
	u8 *data = vec_data(rbuf);
	u8 *end;
	u8 key[24];
	SHA1_CTX sha1;
	u8 accept[32];
	u8 hash[20];
	u8 decoded_key[16];
	i32 lendec;
	const u8 *guid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

	if ((end = substrn(data, "\r\n\r\n", rbuf_offset))) {
		u64 len = end - data, i;
		u8 *sec_websocket_key = NULL;
		sec_websocket_key = substrn(data, "Sec-WebSocket-Key: ", len);
		if (sec_websocket_key == NULL) {
			err = EPROTO;
			return -1;
		}
		sec_websocket_key += 19;
		if ((u64)(sec_websocket_key - data) + 24 > len) {
			err = EOVERFLOW;
			return -1;
		}
		memcpy(key, sec_websocket_key, 24);

		if (substrn(data, "GET ", len) != data) {
			err = EPROTO;
			return -1;
		}
		for (i = 4; i < len; i++)
			if (data[i] == ' ' || data[i] == '\r' ||
			    data[i] == '\n')
				break;

		lendec = b64_decode((u8 *)key, 24, decoded_key, 16);
		if (lendec != 16) {
			err = EINVAL;
			return -1;
		}

		sha1_init(&sha1);
		sha1_update(&sha1, (u8 *)key, 24);
		sha1_update(&sha1, (u8 *)guid, strlen(guid));
		sha1_final(&sha1, hash);

		if (!b64_encode(hash, 20, (u8 *)accept, sizeof(accept))) {
			err = EINVAL;
			return -1;
		}

		connection_write((Connection *)wsconn, SWITCHING_PROTOS,
				 strlen(SWITCHING_PROTOS));
		connection_write((Connection *)wsconn, accept, strlen(accept));
		connection_write((Connection *)wsconn, "\r\n\r\n", 4);
		if (len + 4 < rbuf_offset) {
			memorymove(data, (u8 *)((u64)data + len + 4),
				   rbuf_offset - (len + 4));
		}
		vec_truncate(rbuf, rbuf_offset - (len + 4));

		return 0;
	} else {
		err = EAGAIN;
		return -1;
	}
}

STATIC i32 ws_proc_frames(Ws *ws, WsConnection *wsconn) {
	Vec *rbuf = connection_rbuf((Connection *)wsconn);
	u8 *data = vec_data(rbuf);
	u64 rbuf_offset = vec_elements(rbuf);
	bool fin, mask;
	u8 op;
	u64 len;
	u64 data_start;
	u8 masking_key[4] = {0};

	if (rbuf_offset < 2) {
		err = EAGAIN;
		return -1;
	}

	fin = (data[0] & 0x80) != 0;
	op = data[0] & 0xF;
	mask = (data[1] & 0x80) != 0;

	len = data[1] & 0x7F;
	if (len == 126) {
		if (rbuf_offset < 4) {
			err = EAGAIN;
			return -1;
		}
		len = (data[2] << 8) | data[3];
		data_start = mask ? 8 : 4;
	} else if (len == 127) {
		if (rbuf_offset < 10) {
			err = EAGAIN;
			return -1;
		}
		len = ((u64)data[2] << 56) | ((u64)data[3] << 48) |
		      ((u64)data[4] << 40) | ((u64)data[5] << 32) |
		      ((u64)data[6] << 24) | ((u64)data[7] << 16) |
		      ((u64)data[8] << 8) | data[9];
		data_start = mask ? 14 : 10;
	} else
		data_start = mask ? 6 : 2;

	/* Accept continuation, binary, close, and ping frames only */
	if (op != 0 && op != 1 && op != 2 && op != 8 && op != 9) {
		err = EPROTO;
		return -1;
	}
	if (mask) {
		u64 i;
		u8 *payload;
		masking_key[0] = data[data_start - 4];
		masking_key[1] = data[data_start - 3];
		masking_key[2] = data[data_start - 2];
		masking_key[3] = data[data_start - 1];

		payload = data + data_start;
		for (i = 0; i < len; i++) {
			payload[i] ^= masking_key[i % 4];
		}
	}

	if (data_start + len <= rbuf_offset) {
		proc_message_single(ws, wsconn, data_start, len, op, fin);
		if (len + data_start < rbuf_offset)
			memorymove(data, (u8 *)((u64)data + len + data_start),
				   rbuf_offset - (len + data_start));
		vec_truncate(rbuf, rbuf_offset - (len + data_start));
		return 0;
	} else {
		err = EAGAIN;
		return -1;
	}
}

STATIC void ws_on_accept_proc(void *ctx, Connection *conn) {
	WsContext *ws_ctx = (WsContext *)ctx;
	Ws *ws = ws_ctx->ws;
	WsConnection *wsconn = (WsConnection *)conn;
	wsconn->id = __add64(&ws->next_id, 1);
	connection_set_flag(conn, CONN_FLAG_USR1, false);
	connection_set_flag_upper_bits(conn, ws_ctx->id);
	{
		LockGuard lg = wlock(&ws_ctx->lock);
		rbtree_put(&ws_ctx->connections, (RbTreeNode *)wsconn,
			   ws_rbtree_search);
	}
	ws->config.on_open(ws, wsconn);
}

STATIC void ws_on_connect_proc(void *ctx, Connection *conn, i32 error) {
	WsContext *ws_ctx = (WsContext *)ctx;
	Ws *ws = ws_ctx->ws;
	WsConnection *wsconn = (WsConnection *)conn;

	wsconn->id = __add64(&ws->next_id, 1);
	connection_set_flag(conn, CONN_FLAG_USR1, false);
	connection_set_flag_upper_bits(conn, ws_ctx->id);
	{
		LockGuard lg = wlock(&ws_ctx->lock);
		rbtree_put(&ws_ctx->connections, (RbTreeNode *)wsconn,
			   ws_rbtree_search);
	}
	ws->config.on_connect(ws, wsconn, error);
}

STATIC void ws_on_recv_proc(void *ctx, Connection *conn,
			    u64 rlen __attribute__((unused))) {
	WsContext *ws_ctx = (WsContext *)ctx;
	Ws *ws = ws_ctx->ws;
	WsConnection *wsconn = (WsConnection *)conn;

	while (true) {
		err = 0;
		if (!connection_get_flag(conn, CONN_FLAG_USR1)) {
			ConnectionType ctype =
			    connection_type((Connection *)wsconn);
			i32 res;
			if (ctype == Inbound)
				res = ws_proc_handshake_server(wsconn);
			else
				res = ws_proc_handshake_client(wsconn);
			if (res == 0) {
				connection_set_flag(conn, CONN_FLAG_USR1, true);
				continue;
			} else if (err != EAGAIN) {
				connection_write((Connection *)wsconn,
						 BAD_REQUEST,
						 strlen(BAD_REQUEST));
				connection_close((Connection *)wsconn);
			}
		} else {
			if (ws_proc_frames(ws, wsconn) == 0) continue;
			if (err != EAGAIN) {
				connection_close((Connection *)wsconn);
			}
		}
		break;
	}
}

STATIC void ws_on_close_proc(void *ctx, Connection *conn) {
	WsContext *ws_ctx = (WsContext *)ctx;
	Ws *ws = ws_ctx->ws;
	WsConnection *wsconn = (WsConnection *)conn;

	ws->config.on_close(ws, wsconn);
	{
		LockGuard lg = wlock(&ws_ctx->lock);
		rbtree_remove(&ws_ctx->connections, (RbTreeNode *)wsconn,
			      ws_rbtree_search);
	}
}

STATIC void ws_on_message_nop(Ws *ws __attribute__((unused)),
			      WsConnection *conn __attribute__((unused)),
			      WsMessage *msg __attribute__((unused))) {}
STATIC void ws_on_connect_nop(Ws *ws __attribute__((unused)),
			      WsConnection *conn __attribute__((unused)),
			      i32 error __attribute__((unused))) {}
STATIC void ws_on_open_nop(Ws *ws __attribute__((unused)),
			   WsConnection *conn __attribute__((unused))) {}

STATIC void ws_on_close_nop(Ws *ws __attribute__((unused)),
			    WsConnection *conn __attribute__((unused))) {}

Ws *ws_init(const WsConfig *config) {
	u16 i;
	Ws *ret;
	u16 backlog;
	u16 workers;

	if (!config) {
		err = EINVAL;
		return NULL;
	}

	backlog = config->backlog == 0 ? 16 : config->backlog;
	workers = config->workers == 0 ? 1 : config->workers;

	ret = alloc(sizeof(Ws));
	if (ret == NULL) return NULL;
	ret->ctxs = alloc(sizeof(WsContext) * workers);
	if (!ret->ctxs) {
		release(ret);
		return NULL;
	}

	ret->acceptor =
	    connection_acceptor(config->addr, config->port, backlog,
				sizeof(WsConnection) - CONNECTION_SIZE);
	if (!ret->acceptor) {
		release(ret->ctxs);
		release(ret);
		return NULL;
	}
	for (i = 0; i < workers; i++) {
		EvhConfig config = {NULL, ws_on_recv_proc, ws_on_accept_proc,
				    ws_on_connect_proc, ws_on_close_proc};
		ret->ctxs[i].ws = ret;
		ret->ctxs[i].id = i;
		ret->ctxs[i].lock = LOCK_INIT;
		ret->ctxs[i].connections = RBTREE_INIT;
		config.ctx = &ret->ctxs[i];
		ret->ctxs[i].evh = evh_init(&config);
	}
	ret->config = *config;
	ret->config.backlog = backlog;
	ret->config.workers = workers;
	ret->next_id = 0;
	ret->config.on_message = !ret->config.on_message
				     ? ws_on_message_nop
				     : ret->config.on_message;
	ret->config.on_connect = !ret->config.on_connect
				     ? ws_on_connect_nop
				     : ret->config.on_connect;
	ret->config.on_open =
	    !ret->config.on_open ? ws_on_open_nop : ret->config.on_open;
	ret->config.on_close =
	    !ret->config.on_close ? ws_on_close_nop : ret->config.on_close;
	return ret;
}

i32 ws_start(Ws *ws) {
	u64 i;
	for (i = 0; i < ws->config.workers; i++) {
		if (!evh_start(ws->ctxs[i].evh))
			evh_register(ws->ctxs[i].evh, ws->acceptor);
		else {
			while (i--) evh_stop(ws->ctxs[i].evh);
			return -1;
		}
	}
	return 0;
}

i32 ws_stop(Ws *ws) {
	u64 i;
	i32 ret = 0;
	for (i = 0; i < ws->config.workers; i++)
		ret |= evh_stop(ws->ctxs[i].evh);
	return ret;
}

void ws_destroy(Ws *ws) {
	u64 i;
	connection_release(ws->acceptor);
	for (i = 0; i < ws->config.workers; i++) evh_destroy(ws->ctxs[i].evh);
	release(ws->ctxs);
	release(ws);
}

i32 ws_send(Ws *ws, WsConnection *conn, WsMessage *msg) {
	u8 buf[10];
	u64 header_len;
	RbTreeNodePair retval = {0};
	RbTreeNode *root;
	u16 index = connection_get_flag_upper_bits((Connection *)conn);

	buf[0] = 0x80 | msg->op;
	root = ws->ctxs[index].connections.root;

	if (msg->len <= 125) {
		buf[1] = (u8)msg->len;
		header_len = 2;
	} else if (msg->len <= 65535) {
		buf[1] = 126;
		buf[2] = (msg->len >> 8) & 0xFF;
		buf[3] = msg->len & 0xFF;
		header_len = 4;
	} else {
		buf[1] = 127;
		buf[2] = (msg->len >> 56) & 0xFF;
		buf[3] = (msg->len >> 48) & 0xFF;
		buf[4] = (msg->len >> 40) & 0xFF;
		buf[5] = (msg->len >> 32) & 0xFF;
		buf[6] = (msg->len >> 24) & 0xFF;
		buf[7] = (msg->len >> 16) & 0xFF;
		buf[8] = (msg->len >> 8) & 0xFF;
		buf[9] = msg->len & 0xFF;
		header_len = 10;
	}

	{
		LockGuard lg = wlock(&ws->ctxs[index].lock);
		Connection *sres;
		ws_rbtree_search(root, (RbTreeNode *)conn, &retval);
		sres = (Connection *)retval.self;
		if (!sres) {
			return -1;
		}
		if (connection_write(sres, buf, header_len) < 0) {
			return -1;
		}
		return connection_write(sres, msg->buffer, msg->len);
	}
}

i32 ws_close(Ws *ws, WsConnection *conn, i32 code, const u8 *reason) {
	u16 index = connection_get_flag_upper_bits((Connection *)conn);
	RbTreeNode *root = ws->ctxs[index].connections.root;
	RbTreeNodePair retval = {0};
	WsMessage msg = {0};
	u8 close_frame[125 + 2] = {0};
	u64 reason_len = reason ? strlen((const char *)reason) : 0;
	u16 payload_len = 0;
	i32 send_result;

	if (code != 0 &&
	    !((code >= 1000 && code <= 1015 && code != 1004 && code != 1006) ||
	      (code >= 3000 && code <= 4999))) {
		err = EINVAL;
		return -1;
	}

	if (reason_len > 123) {
		reason_len = 123;
	}

	if (code > 0) {
		close_frame[0] = (code >> 8) & 0xFF;
		close_frame[1] = code & 0xFF;
		payload_len = 2;

		if (reason_len > 0) {
			memcpy(&close_frame[2], reason, reason_len);
			payload_len += reason_len;
		}
	}

	msg.buffer = close_frame;
	msg.len = payload_len;
	msg.op = 0x08;

	send_result = ws_send(ws, conn, &msg);
	if (send_result < 0) return send_result;

	{
		LockGuard lg = wlock(&ws->ctxs[index].lock);
		Connection *sres;
		ws_rbtree_search(root, (RbTreeNode *)conn, &retval);
		sres = (Connection *)retval.self;
		if (!sres) return -1;
		connection_close(sres);
	}

	return 0;
}

WsConnection *ws_connect(Ws *ws, u8 addr[4], u16 port) {
	WsConnection *client = (WsConnection *)connection_client(
	    addr, port, sizeof(WsConnection) - CONNECTION_SIZE);
	if (!client) return NULL;
	if (evh_register(ws->ctxs[0].evh, (Connection *)client) < 0) {
		close(connection_socket((Connection *)client));
		connection_release((Connection *)client);
	}
	connection_write((Connection *)client, CLIENT_INIT_PREFIX,
			 strlen(CLIENT_INIT_PREFIX));
	return client;
}

u16 ws_port(Ws *ws) { return connection_acceptor_port(ws->acceptor); }

u64 ws_conn_id(WsConnection *conn) { return conn->id; }

WsConnection ws_conn_copy(WsConnection *connection) { return *connection; }
