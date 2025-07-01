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
#include <connection.H>
#include <error.H>
#include <evh.H>
#include <format.H>
#include <sha1.H>
#include <ws.H>

#define CONNECTION_SIZE 104

struct Ws {
	Evh **evh;
	u64 workers;
	OnMessage on_message;
	OnOpen on_open;
	OnClose on_close;
	OnConnect on_connect;
	WsConfig config;
	u64 next_id;
	Connection *acceptor;
};

struct WsConnection {
	u8 reserved[CONNECTION_SIZE];
	u64 id;
	bool handshake_complete;
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

STATIC void __attribute__((constructor)) _check_connection_size(void) {
	if (connection_size() != CONNECTION_SIZE)
		panic("expected connection size {}. actual {}.",
		      CONNECTION_SIZE, connection_size());
}

STATIC i32 ws_proc_handshake(WsConnection *wsconn) {
	u8 *rbuf = connection_rbuf((Connection *)wsconn);
	u64 rbuf_offset = connection_rbuf_offset((Connection *)wsconn);
	u8 *end;
	u8 key[24];
	SHA1_CTX sha1;
	u8 accept[32];
	u8 hash[20];
	u8 decoded_key[16];
	i32 lendec;
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
		connection_clear_rbuf_through((Connection *)wsconn, len + 4);

		return 0;
	} else {
		err = EAGAIN;
		return -1;
	}
}

STATIC i32 proc_message_single(Ws *ws, WsConnection *wsconn, u64 offset,
			       u64 len) {
	WsMessage msg;
	u8 *rbuf = connection_rbuf((Connection *)wsconn);
	msg.buffer = rbuf + offset;
	msg.len = len;
	ws->on_message(wsconn, &msg);
	return 0;
}

STATIC i32 ws_proc_frames(Ws *ws, WsConnection *wsconn) {
	u8 *rbuf = connection_rbuf((Connection *)wsconn);
	u64 rbuf_offset = connection_rbuf_offset((Connection *)wsconn);
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
		connection_clear_rbuf_through((Connection *)wsconn,
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

STATIC void ws_on_accept_proc(void *ctx, Connection *conn) {
	Ws *ws = (Ws *)ctx;
	WsConnection *wsconn = (WsConnection *)conn;
	wsconn->id = __add64(&ws->next_id, 1);
	wsconn->handshake_complete = false;
	ws->on_open(wsconn);
}

STATIC void ws_on_recv_proc(void *ctx, Connection *conn,
			    u64 rlen __attribute__((unused))) {
	Ws *ws = ctx;
	WsConnection *wsconn = (WsConnection *)conn;

	while (true) {
		err = 0;
		if (!wsconn->handshake_complete) {
			i32 res = ws_proc_handshake(wsconn);
			if (res == 0) {
				wsconn->handshake_complete = true;
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
	Ws *ws = ctx;
	WsConnection *wsconn = (WsConnection *)conn;
	ws->on_close(wsconn);
}

Ws *ws_init(WsConfig *config, OnMessage on_message, OnOpen on_open,
	    OnClose on_close, OnConnect on_connect) {
	Ws *ret = alloc(sizeof(Ws));
	if (ret == NULL) return NULL;
	ret->evh = alloc(sizeof(Evh *) * config->workers);
	if (!ret->evh) {
		release(ret);
		return NULL;
	}

	ret->acceptor = connection_acceptor(
	    config->addr, config->port, config->backlog, ws_on_recv_proc,
	    ws_on_accept_proc, ws_on_close_proc,
	    sizeof(WsConnection) - CONNECTION_SIZE);
	if (!ret->acceptor) {
		release(ret->evh);
		release(ret);
		return NULL;
	}
	ret->workers = config->workers;
	ret->config = *config;
	ret->on_message = on_message;
	ret->on_connect = on_connect;
	ret->on_open = on_open;
	ret->on_close = on_close;
	ret->next_id = 0;
	return ret;
}

i32 ws_start(Ws *ws) {
	u64 i;
	for (i = 0; i < ws->workers; i++) {
		ws->evh[i] = evh_start(ws);
		if (ws->evh[i])
			evh_register(ws->evh[i], ws->acceptor);
		else {
			while (i--) evh_stop(ws->evh[i]);
			return -1;
		}
	}
	return 0;
}

i32 ws_stop(Ws *ws) {
	u64 i;
	i32 ret = 0;
	for (i = 0; i < ws->workers; i++) ret |= evh_stop(ws->evh[i]);
	return ret;
}

void ws_destroy(Ws *ws) {
	connection_close(ws->acceptor);
	connection_release(ws->acceptor);
	release(ws->evh);
	release(ws);
}

i32 ws_send(u64 id, WsMessage *msg) {
	if (id || msg) {
	}
	return 0;
}

u64 ws_conn_id(WsConnection *conn) { return conn->id; }

WsConnection *ws_connect(Ws *ws, u8 addr[4], u16 port) {
	if (ws || addr || port) {
	}
	return NULL;
}
i32 ws_close(u64 id, i32 code, const u8 *reason) {
	if (id || code || reason) {
	}
	return 0;
}

u16 ws_port(Ws *ws) {
	if (ws) {
	}
	return 0;
}

