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

#ifndef _WS_H
#define _WS_H

#define WS_CONNECTION_SIZE 88
#define WS_SIZE 16

typedef struct {
	uint8_t *buffer;
	size_t len;
} WsMessage;

typedef struct {
	uint8_t opaque[WS_CONNECTION_SIZE];
} WsConnection;

typedef struct {
	uint16_t port;
	uint8_t addr[4];
	uint16_t backlog;
} WsConfig;

typedef struct {
	uint8_t opaque[WS_SIZE];
} Ws;

typedef void (*OnOpen)(void *ctx, WsConnection *conn);
typedef void (*OnClose)(void *ctx, WsConnection *conn, int code,
			const char *reason);
typedef int (*OnMessage)(void *ctx, WsConnection *conn, WsMessage *msg);
int send_ws_message(WsConnection *conn, WsMessage *msg);

int init_ws(Ws *ws, WsConfig *config, void *ctx, OnMessage on_message,
	    OnOpen on_open, OnClose on_close);
int start_ws(Ws *ws);
int stop_ws(Ws *ws);

uint64_t connection_id(WsConnection *connection);
int connect_ws(Ws *ws, WsConnection *conn, const char *url, void *ctx);
int close_ws_connection(WsConnection *conn, int code, const char *reason);

#endif /* _WS_H */
