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

#include <alloc.h>
#include <atomic.h>
#include <error.h>
#include <evh.h>
#include <ws.h>

int printf(const char *, ...);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"

struct Ws {
	Evh *evh;
	uint64_t workers;
	OnMessage on_message;
	OnOpen on_open;
	OnClose on_close;
	WsConfig config;
	uint64_t next_id;
};

struct WsConnection {
	Connection connection;
	uint64_t id;
};

STATIC int ws_on_recv_proc(void *ctx, Connection *conn, size_t rlen) {
	Ws *ws = ctx;
	WsConnection *wsconn = (WsConnection *)conn;
	/*printf("on recv %ld: len=%lu\n", wsconn->id, rlen);*/
	return 0;
}

STATIC int ws_on_accept_proc(void *ctx, Connection *conn) {
	Ws *ws = ctx;
	WsConnection *wsconn = (WsConnection *)conn;
	wsconn->id = __add64(&ws->next_id, 1);
	ws->on_open(wsconn);
	return 0;
}
STATIC int ws_on_close_proc(void *ctx, Connection *conn) {
	Ws *ws = ctx;
	WsConnection *wsconn = (WsConnection *)conn;
	ws->on_close(wsconn);
	return 0;
}

Ws *init_ws(WsConfig *config, OnMessage on_message, OnOpen on_open,
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

int start_ws(Ws *ws) {
	uint64_t i;
	Connection *acceptor =
	    evh_acceptor(ws->config.addr, ws->config.port, ws->config.backlog,
			 ws_on_recv_proc, ws_on_accept_proc, ws_on_close_proc);

	if (acceptor == NULL) return -1;

	for (i = 0; i < ws->workers; i++) {
		ws->evh[i].id = i;
		if (evh_start(&ws->evh[i], ws,
			      sizeof(WsConnection) - sizeof(Connection)) ==
		    -1) {
			release(acceptor);
			return -1;
		}

		/*printf("register acceptor %i with %i\n", acceptor->socket,
		       ws->evh[i].mplex);*/
		if (evh_register(&ws->evh[i], acceptor) == -1) {
			release(acceptor);
			evh_stop(&ws->evh[i]);
			return -1;
		}
	}

	return 0;
}

int stop_ws(Ws *ws) {
	if (ws == NULL) {
		err = EINVAL;
		return -1;
	}
	return 0;
}

uint64_t connection_id(WsConnection *conn) { return conn->id; }
WsConnection *connect_ws(Ws *ws, const char *url);
int close_ws_connection(WsConnection *conn, int code, const char *reason);
int send_ws_message(WsConnection *conn, WsMessage *msg);

#pragma GCC diagnostic pop

