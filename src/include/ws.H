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

typedef struct {
	u8 *buffer;
	u64 len;
} WsMessage;

typedef struct {
	u16 port;
	u8 addr[4];
	u16 backlog;
	u16 workers;
} WsConfig;

typedef struct WsConnection WsConnection;
typedef struct Ws Ws;

typedef void (*OnOpen)(WsConnection *conn);
typedef void (*OnClose)(WsConnection *conn);
typedef int (*OnMessage)(WsConnection *conn, WsMessage *msg);

i32 ws_send(WsConnection *conn, WsMessage *msg);
Ws *ws_init(WsConfig *config, OnMessage on_message, OnOpen on_open,
	    OnClose on_close);
i32 ws_start(Ws *ws);
i32 ws_stop(Ws *ws);

u64 ws_connection_id(WsConnection *connection);
WsConnection *ws_connect(Ws *ws, const u8 *url);
i32 ws_connection_close(WsConnection *conn, int code, const u8 *reason);
const u8 *ws_connection_uri(WsConnection *conn);
u16 ws_port(Ws *ws);

#endif /* _WS_H */
