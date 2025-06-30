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

#include <error.H>
#include <event.H>
#include <syscall.H>
#include <syscall_const.H>
#include <types.H>

#define STATIC_ASSERT(condition, message) \
	typedef u8 static_assert_##message[(condition) ? 1 : -1]

STATIC_ASSERT(sizeof(Event) == sizeof(struct epoll_event), event_match);

i32 multiplex(void) { return epoll_create1(0); }

i32 mregister(i32 multiplex, i32 fd, i32 flags, void *attach) {
	struct epoll_event ev = {0};
	i32 event_flags = 0;
	i32 ret;

	if (multiplex < 0 || fd < 0) {
		err = EINVAL;
		return -1;
	}

	if (flags & MULTIPLEX_FLAG_READ)
		event_flags |= (EPOLLIN | EPOLLET | EPOLLRDHUP);
	if (flags & MULTIPLEX_FLAG_ACCEPT) event_flags |= (EPOLLIN | EPOLLET);
	if (flags & MULTIPLEX_FLAG_WRITE) event_flags |= (EPOLLOUT | EPOLLET);

	ev.events = event_flags;
	ev.data.ptr = attach;

	ret = epoll_ctl(multiplex, EPOLL_CTL_ADD, fd, &ev);
	if (ret < 0 && err == EEXIST)
		ret = epoll_ctl(multiplex, EPOLL_CTL_MOD, fd, &ev);

	return ret;
}
i32 mwait(i32 multiplex, Event *events, i32 max_events, i32 timeout) {
	if (multiplex < 0 || max_events <= 0 || !events) {
		err = EINVAL;
		return -1;
	}

	return epoll_pwait(multiplex, (struct epoll_event *)events, max_events,
			   timeout, NULL, 0);
}

i32 event_is_read(Event event) {
	struct epoll_event *epoll_ev = (struct epoll_event *)&event;
	return epoll_ev->events & EPOLLIN;
}

i32 event_is_write(Event event) {
	struct epoll_event *epoll_ev = (struct epoll_event *)&event;
	return epoll_ev->events & EPOLLOUT;
}

void *event_attachment(Event event) {
	struct epoll_event *epoll_ev = (struct epoll_event *)&event;
	return epoll_ev->data.ptr;
}

