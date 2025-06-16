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

#include <channel.H>
#include <lock.H>
#include <types.H>

typedef struct Connection Connection;

typedef int (*OnRecvFn)(void *ctx, Connection *conn, u64 rlen);
typedef int (*OnAcceptFn)(void *ctx, Connection *conn);
typedef int (*OnCloseFn)(void *ctx, Connection *conn);

typedef enum { Acceptor, Inbound, Outbound } ConnectionType;

typedef struct {
	OnRecvFn on_recv;
	OnAcceptFn on_accept;
	OnCloseFn on_close;
	u16 port;
} AcceptorData;

typedef struct {
	OnRecvFn on_recv;
	OnCloseFn on_close;
	int mplex;
	Lock lock;
	bool is_closed;
	u8 *rbuf;
	u64 rbuf_capacity;
	u64 rbuf_offset;
	u8 *wbuf;
	u64 wbuf_capacity;
	u64 wbuf_offset;
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
	u64 *stopped;
	u64 id;
	u64 connection_alloc_overhead;
} Evh;

int evh_register(Evh *evh, Connection *connection);
int evh_start(Evh *evh, void *ctx, u64 connection_alloc_overhead);
int evh_stop(Evh *evh);

Connection *evh_acceptor(u8 addr[4], u16 port, u16 backlog,
			 OnRecvFn on_recv, OnAcceptFn on_accept,
			 OnCloseFn on_close);
Connection *evh_client(u8 addr[4], u16 port, OnRecvFn on_recv,
		       OnCloseFn on_close, u64 connection_alloc_overhead);
u16 evh_acceptor_port(Connection *conn);
int connection_close(Connection *connection);
int connection_write(Connection *connection, const void *buf, u64 len);
void connection_clear_rbuf_through(Connection *conn, u64 off);
void connection_clear_rbuf(Connection *conn);

#if TEST == 1
extern bool debug_force_write_buffer;
#endif /* TEST */

#endif /* _EVH_H */
