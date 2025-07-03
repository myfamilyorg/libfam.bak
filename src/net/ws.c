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
#include <lock.H>
#include <rbtree.H>
#include <sha1.H>
#include <ws.H>

#define CONNECTION_SIZE 56

typedef struct {
	u16 id;
	Ws *ws;
	Evh *evh;
	RbTree connections;
	Lock lock;
} WsContext;

struct Ws {
	WsContext *ctxs;
	WsConfig config;
	u64 next_id;
	Connection *acceptor;
};

struct WsConnection {
	u8 reserved[CONNECTION_SIZE];
	u64 id;
};

STATIC void __attribute__((constructor)) _check_connection_size(void) {
	if (connection_size() != CONNECTION_SIZE)
		panic("expected connection size {}. actual {}.",
		      CONNECTION_SIZE, connection_size());
	if (sizeof(WsConnection) > 64)
		panic("WsConnection > 64 bytes {}!", sizeof(WsConnection));
}

STATIC i32 ws_rbtree_search(RbTreeNode *cur, const RbTreeNode *value,
			    RbTreeNodePair *retval) {
	while (cur) {
		u64 v1 = ((WsConnection *)cur)->id;
		u64 v2 = ((WsConnection *)value)->id;
		if (v1 == v2) {
			retval->self = cur;
			break;
		} else if (v1 < v2) {
			retval->parent = cur;
			retval->is_right = 1;
			cur = cur->right;
		} else {
			retval->parent = cur;
			retval->is_right = 0;
			cur = cur->left;
		}
		retval->self = cur;
	}
	return 0;
}

STATIC void ws_on_accept_proc(void *ctx, Connection *conn) {
	WsContext *ws_ctx = (WsContext *)ctx;
	Ws *ws = ws_ctx->ws;
	WsConnection *wsconn = (WsConnection *)conn;
	wsconn->id = __add64(&ws->next_id, 1);
	connection_set_flag(conn, CONN_FLAG_USR1, false);
	connection_set_flag_upper_bits(conn, ws_ctx->id);
	{
		LockGuard lg = wlock(&ws_ctx->lock);
		rbtree_put(&ws_ctx->connections, (RbTreeNode *)wsconn,
			   ws_rbtree_search);
	}
	ws->config.on_open(ws, wsconn);
}

STATIC void ws_on_connect_proc(void *ctx, Connection *conn, int error) {
	if (ctx || conn || error) {
	}
}

STATIC void ws_on_recv_proc(void *ctx, Connection *conn,
			    u64 rlen __attribute__((unused))) {
	if (ctx || conn) {
	}
}

STATIC void ws_on_close_proc(void *ctx, Connection *conn) {
	WsContext *ws_ctx = (WsContext *)ctx;
	Ws *ws = ws_ctx->ws;
	WsConnection *wsconn = (WsConnection *)conn;

	ws->config.on_close(ws, wsconn);
	{
		LockGuard lg = wlock(&ws_ctx->lock);
		rbtree_remove(&ws_ctx->connections, (RbTreeNode *)wsconn,
			      ws_rbtree_search);
	}
}

STATIC void ws_on_message_nop(Ws *ws __attribute__((unused)),
			      WsConnection *conn __attribute__((unused)),
			      WsMessage *msg __attribute__((unused))) {}
STATIC void ws_on_connect_nop(Ws *ws __attribute__((unused)),
			      WsConnection *conn __attribute__((unused)),
			      i32 error __attribute__((unused))) {}
STATIC void ws_on_open_nop(Ws *ws __attribute__((unused)),
			   WsConnection *conn __attribute__((unused))) {}

STATIC void ws_on_close_nop(Ws *ws __attribute__((unused)),
			    WsConnection *conn __attribute__((unused))) {}

Ws *ws_init(const WsConfig *config) {
	u16 i;
	Ws *ret;
	u16 backlog;
	u16 workers;

	if (!config) {
		err = EINVAL;
		return NULL;
	}

	backlog = config->backlog == 0 ? 16 : config->backlog;
	workers = config->workers == 0 ? 1 : config->workers;

	ret = alloc(sizeof(Ws));
	if (ret == NULL) return NULL;
	ret->ctxs = alloc(sizeof(WsContext) * workers);
	if (!ret->ctxs) {
		release(ret);
		return NULL;
	}

	ret->acceptor =
	    connection_acceptor(config->addr, config->port, backlog,
				sizeof(WsConnection) - CONNECTION_SIZE);
	if (!ret->acceptor) {
		release(ret->ctxs);
		release(ret);
		return NULL;
	}
	for (i = 0; i < workers; i++) {
		EvhConfig config = {NULL, ws_on_recv_proc, ws_on_accept_proc,
				    ws_on_connect_proc, ws_on_close_proc};
		ret->ctxs[i].ws = ret;
		ret->ctxs[i].id = i;
		ret->ctxs[i].lock = LOCK_INIT;
		ret->ctxs[i].connections = RBTREE_INIT;
		config.ctx = &ret->ctxs[i];
		ret->ctxs[i].evh = evh_init(&config);
	}
	ret->config = *config;
	ret->config.backlog = backlog;
	ret->config.workers = workers;
	ret->next_id = 0;
	ret->config.on_message = !ret->config.on_message
				     ? ws_on_message_nop
				     : ret->config.on_message;
	ret->config.on_connect = !ret->config.on_connect
				     ? ws_on_connect_nop
				     : ret->config.on_connect;
	ret->config.on_open =
	    !ret->config.on_open ? ws_on_open_nop : ret->config.on_open;
	ret->config.on_close =
	    !ret->config.on_close ? ws_on_close_nop : ret->config.on_close;
	return ret;
}

i32 ws_start(Ws *ws) {
	u64 i;
	for (i = 0; i < ws->config.workers; i++) {
		if (!evh_start(ws->ctxs[i].evh))
			evh_register(ws->ctxs[i].evh, ws->acceptor);
		else {
			while (i--) evh_stop(ws->ctxs[i].evh);
			return -1;
		}
	}
	return 0;
}

i32 ws_stop(Ws *ws) {
	u64 i;
	i32 ret = 0;
	for (i = 0; i < ws->config.workers; i++)
		ret |= evh_stop(ws->ctxs[i].evh);
	return ret;
}

void ws_destroy(Ws *ws) {
	u64 i;
	connection_release(ws->acceptor);
	for (i = 0; i < ws->config.workers; i++) evh_destroy(ws->ctxs[i].evh);
	release(ws->ctxs);
	release(ws);
}

