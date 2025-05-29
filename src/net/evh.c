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
#include <error.h>
#include <evh.h>
#include <misc.h>
#include <stdio.h>
#include <sys.h>

STATIC int evh_proc_wakeup(int mplex, int wakeup, EvhRegisterQueue *regqueue) {
	char buf[1024];
	struct Connection *ptr;
	printf("evh wakeup\n");
	{
		LockGuard lg = lock_write(&regqueue->lock);
		ptr = regqueue->head;
		regqueue->head = regqueue->tail = NULL;
	}
	/*
	while (ptr) {
		printf("adding connection %lu\n", (size_t)ptr);
		ptr = ptr->next;
		printf("after\n");
	}
	*/
	printf("loop complete\n");

	if (read(wakeup, buf, 1024) <= 0)
		return -1;
	else
		return 0;
}

STATIC void evh_loop(int wakeup, EvhRegisterQueue *regqueue) {
	Event events[1024];
	int i, count, mplex;

	if ((mplex = multiplex()) == -1) {
		print_error("multiplex");
		exit(0);
	}
	if (mregister(mplex, wakeup, MULTIPLEX_FLAG_READ, &wakeup) == -1) {
		print_error("mregister");
		exit(0);
	}
	while ((count = mwait(mplex, events, 1024, -1)) > 0) {
		for (i = 0; i < count; i++) {
			if (event_attachment(events[i]) == &wakeup) {
				if (evh_proc_wakeup(mplex, wakeup, regqueue))
					goto breakloop;
			}
		}
	}
	print_error("mwait");
breakloop:
	printf("exit evh_loop\n");
	exit(0);
}

int evh_register(Evh *evh, Connection *connection) {
	Connection *conn = alloc(sizeof(Connection));
	memcpy(conn, connection, sizeof(Connection));
	conn->next = NULL;
	if (conn) {
		LockGuard lg = lock_write(&evh->regqueue->lock);
		if (evh->regqueue->tail) {
			evh->regqueue->tail->next = conn;
			evh->regqueue->tail = conn;
		} else
			evh->regqueue->tail = evh->regqueue->head = conn;
	} else {
		return -1;
	}

	if (write(evh->wakeup, "1", 1) == -1) {
		release(conn);
		return -1;
	}
	return 0;
}

int evh_start(Evh *evh) {
	int fds[2];

	if (pipe(fds) == -1) return -1;

	evh->regqueue = smap(sizeof(EvhRegisterQueue));
	if (evh->regqueue == NULL) {
		close(fds[0]);
		close(fds[1]);
		return -1;
	}

	evh->wakeup = fds[1];
	if (fork() == 0) {
		close(fds[1]);
		evh_loop(fds[0], evh->regqueue);
		exit(0);
	} else {
		close(fds[0]);
	}
	return 0;
}

int evh_stop(Evh *evh) {
	munmap(evh->regqueue, sizeof(EvhRegisterQueue));
	return close(evh->wakeup);
}
