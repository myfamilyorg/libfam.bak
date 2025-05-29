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

#ifndef _EVH_H__
#define _EVH_H__

#include <channel.h>
#include <lock.h>
#include <types.h>

struct Connection;
struct AcceptorData;

typedef int (*OnRecvFn)(void *ctx, struct Connection *conn, const uint8_t *data,
			size_t len);
typedef int (*OnAcceptFn)(void *ctx, struct Connection *conn);
typedef int (*OnCloseFn)(void *ctx, struct Connection *conn);

typedef enum { Acceptor, Inbound, Outbound } ConnectionType;

typedef struct {
	OnRecvFn on_recv;
	OnAcceptFn on_accept;
	OnCloseFn on_close;
} AcceptorData;

typedef struct {
	Lock lock;
	bool is_closed;
} InboundData;

typedef struct {
	OnRecvFn on_recv;
	OnCloseFn on_close;
	Lock lock;
	bool is_closed;
} OutboundData;

typedef struct {
	ConnectionType conn_type;
	int socket;
	union {
		AcceptorData acceptor;
		InboundData inbound;
		OutboundData outbound;
	} data;
} Connection;

typedef struct {
	int wakeup;
	Channel *regqueue;
} Evh;

int evh_register(Evh *evh, Connection *connection);
int evh_start(Evh *evh);
int evh_stop(Evh *evh);

#endif /* _EVH_H__ */
