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

#ifndef _CONNECTION_H
#define _CONNECTION_H

#include <types.H>

typedef enum { Acceptor, Inbound, Outbound } ConnectionType;

typedef struct Connection Connection;

typedef i32 (*OnRecvFn)(void *ctx, Connection *conn, u64 rlen);
typedef i32 (*OnAcceptFn)(void *ctx, Connection *conn);
typedef i32 (*OnCloseFn)(void *ctx, Connection *conn);
typedef i32 (*OnConnectFn)(void *ctx, Connection *conn);
typedef i32 (*OnConnectErrorFn)(void *ctx, Connection *conn);

Connection *evh_acceptor(const u8 addr[4], u16 port, u16 backlog,
			 OnRecvFn on_recv, OnAcceptFn on_accept,
			 OnCloseFn on_close, u64 connection_alloc_overhead);
Connection *evh_client(const u8 addr[4], u16 port, OnRecvFn on_recv,
		       OnConnectFn on_connect,
		       OnConnectErrorFn on_connect_error_fn, OnCloseFn on_close,
		       u64 connection_alloc_overhead);
Connection *evh_accepted(i32 fd, OnRecvFn on_recv, OnCloseFn on_close,
			 u64 connection_alloc_overhead);
u16 evh_acceptor_port(const Connection *conn);
i32 connection_close(Connection *connection);
i32 connection_write(Connection *connection, const void *buf, u64 len);
i32 connection_write_complete(Connection *connection);
u8 *connection_rbuf(Connection *conn);
u64 connection_rbuf_offset(Connection *conn);
i32 connection_set_rbuf_offset(Connection *conn, u64 noffset);
u64 connection_rbuf_capacity(Connection *conn);
void connection_clear_rbuf_through(Connection *conn, u64 off);
void connection_clear_rbuf(Connection *conn);
ConnectionType connection_type(Connection *conn);
i32 connection_set_mplex(Connection *conn, i32 mplex);
i32 connection_socket(Connection *conn);
OnAcceptFn connection_on_accept(Connection *conn);
OnRecvFn connection_on_recv(Connection *conn);
OnCloseFn connection_on_close(Connection *conn);
OnConnectFn connection_on_connect(Connection *conn);
OnConnectErrorFn connection_on_connect_error(Connection *conn);
i64 connection_alloc_overhead(Connection *conn);
i32 connection_check_capacity(Connection *conn);
void connection_release(Connection *conn);
bool connection_is_connected(Connection *conn);
void connection_set_is_connected(Connection *conn);

#endif /* _CONNECTION_H */
