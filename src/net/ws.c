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
#include <evh.H>
#include <format.H>
#include <ws.H>

#define CONNECTION_SIZE 104

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
	u8 reserved[CONNECTION_SIZE];
	u64 id;
	bool handshake_complete;
};

STATIC void __attribute__((constructor)) _check_connection_size(void) {
	if (connection_size() != CONNECTION_SIZE)
		panic("expected connection size {}. actual {}.",
		      CONNECTION_SIZE, connection_size());
}

STATIC void ws_on_accept_proc(void *ctx, Connection *conn) {
	if (ctx || conn) {
	}
}
STATIC void ws_on_recv_proc(void *ctx, Connection *conn, u64 rlen) {
	if (ctx || conn || rlen) {
	}
}
STATIC void ws_on_close_proc(void *ctx, Connection *conn) {
	if (ctx || conn) {
	}
}

Ws *ws_init(WsConfig *config, OnMessage on_message, OnOpen on_open,
	    OnClose on_close) {
	Ws *ret = alloc(sizeof(Ws));
	if (ret == NULL) return NULL;
	ret->workers = config->workers;
	ret->config = *config;
	ret->on_message = on_message;
	ret->on_open = on_open;
	ret->on_close = on_close;
	ret->next_id = 0;
	ret->acceptor =
	    connection_acceptor(config->addr, config->port, config->backlog,
			 ws_on_recv_proc, ws_on_accept_proc, ws_on_close_proc,
			 sizeof(WsConnection) - CONNECTION_SIZE);
	return ret;
}

i32 ws_start(Ws *ws) {
	ws->evh = evh_start(NULL);
	return 0;
}
i32 ws_stop(Ws *ws) { return evh_stop(ws->evh); }
void ws_destroy(Ws *ws) {
	if (ws) {
	}
}

i32 ws_send(u64 id, WsMessage *msg) {
	if (id || msg) {
	}
	return 0;
}

u64 ws_conn_id(WsConnection *conn) {
	if (conn) {
	}
	return 0;
}

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

