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

#include <error.h>
#include <event.h>
#include <syscall.h>
#include <syscall_const.h>
#include <types.h>

#define STATIC_ASSERT(condition, message) \
	typedef char static_assert_##message[(condition) ? 1 : -1]

STATIC_ASSERT(sizeof(Event) == sizeof(struct epoll_event), event_match);

int multiplex(void) { return epoll_create1(0); }

int mregister(int multiplex, int fd, int flags, void *attach) {
	struct epoll_event ev;
	int event_flags = 0;

	if (flags & MULTIPLEX_FLAG_READ) {
		event_flags |= (EPOLLIN | EPOLLET | EPOLLRDHUP);
	}

	if (flags & MULTIPLEX_FLAG_ACCEPT) {
		event_flags |= (EPOLLIN | EPOLLEXCLUSIVE | EPOLLET);
	}

	if (flags & MULTIPLEX_FLAG_WRITE) {
		event_flags |= (EPOLLOUT | EPOLLET);
	}

	ev.events = event_flags;
	if (attach == NULL)
		ev.data.fd = fd;
	else
		ev.data.ptr = attach;

	if (epoll_ctl(multiplex, EPOLL_CTL_ADD, fd, &ev) < 0) {
		if (err == EEXIST) {
			if (epoll_ctl(multiplex, EPOLL_CTL_MOD, fd, &ev) < 0) {
				return -1;
			}
		} else {
			return -1;
		}
	}

	return 0;
}
int mwait(int multiplex, Event *events, int max_events,
	  int64_t timeout_millis) {
	int timeout = (timeout_millis >= 0) ? (int)timeout_millis : -1;
	return epoll_pwait(multiplex, (struct epoll_event *)events, max_events,
			   timeout, NULL, 0);
}

int event_is_read(Event event) {
	struct epoll_event *epoll_ev = (struct epoll_event *)&event;
	return epoll_ev->events & EPOLLIN;
}

int event_is_write(Event event) {
	struct epoll_event *epoll_ev = (struct epoll_event *)&event;
	return epoll_ev->events & EPOLLOUT;
}

void *event_attachment(Event event) {
	struct epoll_event *epoll_ev = (struct epoll_event *)&event;
	return epoll_ev->data.ptr;
}

