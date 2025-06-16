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
#include <error.H>
#include <evh.H>
#include <misc.H>
#include <sha1.H>
#include <ws.H>

#define MAX_URI_LEN 24

struct Ws {
	Evh *evh;
	u64 workers;
	OnMessage on_message;
	OnOpen on_open;
	OnClose on_close;
	WsConfig config;
	u64 next_id;
	Connection *acceptor;
};

struct WsConnection {
	Connection connection;
	u64 id;
	bool handshake_complete;
	u8 uri[MAX_URI_LEN + 1];
};

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

STATIC int ws_proc_handshake(WsConnection *wsconn) {
	u8 *rbuf = (u8 *)wsconn->connection.data.inbound.rbuf;
	u64 rbuf_offset = wsconn->connection.data.inbound.rbuf_offset;
	u8 *end;
	u8 key[24];
	SHA1_CTX sha1;
	u8 accept[32];
	u8 hash[20];
	u8 decoded_key[16];
	int lendec;
	const u8 *guid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

	if ((end = substrn(rbuf, "\r\n\r\n", rbuf_offset))) {
		u64 len = end - rbuf, i;
		u8 *sec_websocket_key = NULL;
		sec_websocket_key = substrn(rbuf, "Sec-WebSocket-Key: ", len);
		if (sec_websocket_key == NULL) {
			err = EPROTO;
			return -1;
		}
		sec_websocket_key += 19;
		if ((u64)(sec_websocket_key - rbuf) + 24 > len) {
			err = EOVERFLOW;
			return -1;
		}
		memcpy(key, sec_websocket_key, 24);

		if (substrn(rbuf, "GET ", len) != rbuf) {
			err = EPROTO;
			return -1;
		}
		for (i = 4; i < len; i++)
			if (rbuf[i] == ' ' || rbuf[i] == '\r' ||
			    rbuf[i] == '\n')
				break;
		if (i - 4 >= MAX_URI_LEN) {
			err = EOVERFLOW;
			return -1;
		}
		memcpy(wsconn->uri, rbuf + 4, i - 4);
		wsconn->uri[i - 4] = 0;

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

		connection_write(&wsconn->connection, SWITCHING_PROTOS,
				 strlen(SWITCHING_PROTOS));
		connection_write(&wsconn->connection, accept, strlen(accept));
		connection_write(&wsconn->connection, "\r\n\r\n", 4);
		connection_clear_rbuf_through(&wsconn->connection, len + 4);

		return 0;
	} else {
		err = EAGAIN;
		return -1;
	}
}

STATIC int proc_message_single(Ws *ws, WsConnection *wsconn, u64 offset,
			       u64 len) {
	WsMessage msg;
	msg.buffer = wsconn->connection.data.inbound.rbuf + offset;
	msg.len = len;
	ws->on_message(wsconn, &msg);

	return 0;
}

STATIC int ws_proc_frames(Ws *ws, WsConnection *wsconn) {
	u64 rbuf_offset = wsconn->connection.data.inbound.rbuf_offset;
	u8 *rbuf = wsconn->connection.data.inbound.rbuf;
	bool fin, mask;
	u8 op;
	u64 len;
	u64 data_start;
	u8 masking_key[4] = {0};

	if (rbuf_offset < 2) {
		err = EAGAIN;
		return -1;
	}

	fin = (rbuf[0] & 0x80) != 0;
	op = rbuf[0] & 0xF;
	mask = (rbuf[1] & 0x80) != 0;

	len = rbuf[1] & 0x7F;
	if (len == 126) {
		if (rbuf_offset < 4) {
			err = EAGAIN;
			return -1;
		}
		len = (rbuf[2] << 8) | rbuf[3];
		data_start = mask ? 8 : 4;
	} else if (len == 127) {
		if (rbuf_offset < 10) {
			err = EAGAIN;
			return -1;
		}
		len = ((u64)rbuf[2] << 56) | ((u64)rbuf[3] << 48) |
		      ((u64)rbuf[4] << 40) | ((u64)rbuf[5] << 32) |
		      ((u64)rbuf[6] << 24) | ((u64)rbuf[7] << 16) |
		      ((u64)rbuf[8] << 8) | rbuf[9];
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
		masking_key[0] = rbuf[data_start - 4];
		masking_key[1] = rbuf[data_start - 3];
		masking_key[2] = rbuf[data_start - 2];
		masking_key[3] = rbuf[data_start - 1];

		payload = rbuf + data_start;
		for (i = 0; i < len; i++) {
			payload[i] ^= masking_key[i % 4];
		}
	}

	if (fin && data_start + len <= rbuf_offset) {
		proc_message_single(ws, wsconn, data_start, len);
		connection_clear_rbuf_through(&wsconn->connection,
					      data_start + len);
		return 0;
	} else if (!fin && data_start + len <= rbuf_offset) {
		/* TODO: implement fragmentation handling */
		return 0;
	} else {
		err = EAGAIN;
		return -1;
	}
}

STATIC int ws_on_recv_proc(void *ctx, Connection *conn,
			   u64 rlen __attribute__((unused))) {
	Ws *ws = ctx;
	WsConnection *wsconn = (WsConnection *)conn;

	while (true) {
		err = 0;
		if (!wsconn->handshake_complete) {
			int res = ws_proc_handshake(wsconn);
			if (res == 0) {
				wsconn->handshake_complete = true;
				continue;
			} else if (err != EAGAIN) {
				connection_write(&wsconn->connection,
						 BAD_REQUEST,
						 strlen(BAD_REQUEST));
				connection_close(&wsconn->connection);
			}
			return 0;
		} else {
			if (ws_proc_frames(ws, wsconn) == 0) continue;
			if (err != EAGAIN) {
				connection_close(&wsconn->connection);
			}
			return 0;
		}
		break;
	}
}

STATIC int ws_on_accept_proc(void *ctx, Connection *conn) {
	Ws *ws = ctx;
	WsConnection *wsconn = (WsConnection *)conn;
	wsconn->id = __add64(&ws->next_id, 1);
	wsconn->handshake_complete = false;
	wsconn->uri[0] = 0;
	ws->on_open(wsconn);
	return 0;
}
STATIC int ws_on_close_proc(void *ctx, Connection *conn) {
	Ws *ws = ctx;
	WsConnection *wsconn = (WsConnection *)conn;
	ws->on_close(wsconn);
	return 0;
}

Ws *ws_init(WsConfig *config, OnMessage on_message, OnOpen on_open,
	    OnClose on_close) {
	Ws *ret = alloc(sizeof(Ws));
	if (ret == NULL) return NULL;
	ret->evh = alloc(sizeof(Evh) * config->workers);
	if (ret->evh == NULL) {
		release(ret);
		return NULL;
	}
	ret->workers = config->workers;
	ret->config = *config;
	ret->on_message = on_message;
	ret->on_open = on_open;
	ret->on_close = on_close;
	ret->next_id = 0;
	return ret;
}

int ws_start(Ws *ws) {
	u64 i;
	ws->acceptor =
	    evh_acceptor(ws->config.addr, ws->config.port, ws->config.backlog,
			 ws_on_recv_proc, ws_on_accept_proc, ws_on_close_proc);

	if (ws->acceptor == NULL) return -1;

	for (i = 0; i < ws->workers; i++) {
		/* TODO: cleanup properly on errors */
		ws->evh[i].id = i;
		if (evh_start(&ws->evh[i], ws,
			      sizeof(WsConnection) - sizeof(Connection)) ==
		    -1) {
			release(ws->acceptor);
			return -1;
		}

		if (evh_register(&ws->evh[i], ws->acceptor) == -1) {
			release(ws->acceptor);
			evh_stop(&ws->evh[i]);
			return -1;
		}
	}

	return 0;
}

int ws_stop(Ws *ws) {
	u64 i;
	for (i = 0; i < ws->workers; i++) {
		evh_stop(&ws->evh[i]);
	}
	connection_close(ws->acceptor);
	release(ws->acceptor);
	release(ws->evh);
	release(ws);
	return 0;
}

u64 ws_connection_id(WsConnection *conn) { return conn->id; }
WsConnection *ws_connect(Ws *ws, const u8 *url);

int ws_connection_close(WsConnection *conn, int code __attribute__((unused)),
			const u8 *reason __attribute__((unused))) {
	connection_close(&conn->connection);
	return 0;
}

int ws_send(WsConnection *conn, WsMessage *msg) {
	u8 buf[10];
	u64 header_len;

	buf[0] = 0x82;

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

	if (connection_write(&conn->connection, buf, header_len) < 0) {
		return -1;
	}

	return connection_write(&conn->connection, msg->buffer, msg->len);
}

const u8 *ws_connection_uri(WsConnection *conn) { return conn->uri; }

u16 ws_port(Ws *ws) { return evh_acceptor_port(ws->acceptor); }

