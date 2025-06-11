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

#ifndef _EVH_H
#define _EVH_H

#include <channel.h>
#include <lock.h>
#include <types.h>

typedef struct Connection Connection;

typedef int (*OnRecvFn)(void *ctx, Connection *conn, size_t rlen);
typedef int (*OnAcceptFn)(void *ctx, Connection *conn);
typedef int (*OnCloseFn)(void *ctx, Connection *conn);

typedef enum { Acceptor, Inbound, Outbound } ConnectionType;

typedef struct {
	OnRecvFn on_recv;
	OnAcceptFn on_accept;
	OnCloseFn on_close;
	uint16_t port;
} AcceptorData;

typedef struct {
	int mplex;
	OnRecvFn on_recv;
	OnCloseFn on_close;
	Lock lock;
	bool is_closed;
	uint8_t *rbuf;
	size_t rbuf_capacity;
	size_t rbuf_offset;
	uint8_t *wbuf;
	size_t wbuf_capacity;
	size_t wbuf_offset;
} InboundData;

struct Connection {
	ConnectionType conn_type;
	int socket;
	union {
		AcceptorData acceptor;
		InboundData inbound;
	} data;
};

typedef struct {
	int wakeup;
	int mplex;
} Evh;

int evh_register(Evh *evh, Connection *connection);
int evh_start(Evh *evh, void *ctx);
int evh_stop(Evh *evh);
int evh_wpend(Evh *evh, Connection *connection);

Connection *evh_acceptor(uint8_t addr[4], uint16_t port, uint16_t backlog,
			 OnRecvFn on_recv, OnAcceptFn on_accept,
			 OnCloseFn on_close);
Connection *evh_client(Evh *evh, uint8_t addr[4], uint16_t port,
		       OnRecvFn on_recv, OnCloseFn on_close);
uint16_t evh_acceptor_port(Connection *conn);
int connection_close(Connection *connection);
int connection_write(Connection *connection, const uint8_t *buf, size_t len);

#endif /* _EVH_H */
