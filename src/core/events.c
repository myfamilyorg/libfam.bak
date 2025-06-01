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

#define STATIC_ASSERT(condition, message) \
	typedef char static_assert_##message[(condition) ? 1 : -1]

#include <error.h>
#include <sys.h>
#include <types.h>
#ifdef __linux__
#include <fcntl.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <unistd.h>
STATIC_ASSERT(sizeof(Event) == sizeof(struct epoll_event), event_match);
#endif /* __linux__ */
#ifdef __APPLE__
#include <errno.h>
#include <sys/event.h>
#include <time.h>
STATIC_ASSERT(sizeof(Event) == sizeof(struct kevent), event_match);
#endif /* __APPLE__ */

int multiplex(void) {
#ifdef __linux__
	return epoll_create1(0);
#elif defined(__APPLE__)
	int ret = kqueue();
	if (ret == -1) err = errno;
	return ret;
#endif
}

int mregister(int multiplex, int fd, int flags, void *attach) {
#ifdef __linux__
	struct epoll_event ev;
	int event_flags = 0;

	if (flags & MULTIPLEX_FLAG_READ) {
		event_flags |= EPOLLIN;
	}

	if (flags & MULTIPLEX_FLAG_WRITE) {
		event_flags |= EPOLLOUT;
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
#elif defined(__APPLE__)
	struct kevent change_event[2];
	int event_count = 0, res;

	if (flags & MULTIPLEX_FLAG_READ) {
		EV_SET(&change_event[event_count], fd, EVFILT_READ,
		       EV_ADD | EV_ENABLE | EV_CLEAR, 0, 0, attach);
		event_count++;
	}

	if (flags & MULTIPLEX_FLAG_WRITE) {
		EV_SET(&change_event[event_count], fd, EVFILT_WRITE,
		       EV_ADD | EV_ENABLE | EV_CLEAR, 0, 0, attach);
		event_count++;
	}

	res = kevent(multiplex, change_event, event_count, NULL, 0, NULL);
	if (res < 0) err = errno;
	if (res < 0) return -1;
	return 0;
#endif
}

int mwait(int multiplex, Event *events, int max_events,
	  int64_t timeout_millis) {
#ifdef __linux__
	int timeout = (timeout_millis >= 0) ? (int)timeout_millis : -1;

	return epoll_wait(multiplex, (struct epoll_event *)events, max_events,
			  timeout);
#elif defined(__APPLE__)
	struct timespec ts;
	struct timespec *timeout_ptr = NULL;
	int ret;

	if (timeout_millis >= 0) {
		ts.tv_sec = timeout_millis / 1000;
		ts.tv_nsec = (timeout_millis % 1000) * 1000000;
		timeout_ptr = &ts;
	}

	ret = kevent(multiplex, NULL, 0, (struct kevent *)events, max_events,
		     timeout_ptr);
	if (ret == -1) err = errno;
	return ret;
#endif
}

int event_is_read(Event event) {
#ifdef __linux__
	struct epoll_event *epoll_ev = (struct epoll_event *)&event;
	return epoll_ev->events & EPOLLIN;
#elif defined(__APPLE__)
	struct kevent *kv = (struct kevent *)&event;
	return kv->filter == EVFILT_READ;
#endif
}

int event_is_write(Event event) {
#ifdef __linux__
	struct epoll_event *epoll_ev = (struct epoll_event *)&event;
	return epoll_ev->events & EPOLLOUT;
#elif defined(__APPLE__)
	struct kevent *kv = (struct kevent *)&event;
	return kv->filter == EVFILT_WRITE;
#endif
}

void *event_attachment(Event event) {
#ifdef __linux__
	struct epoll_event *epoll_ev = (struct epoll_event *)&event;
	return epoll_ev->data.ptr;
#elif defined(__APPLE__)
	struct kevent *kv = (struct kevent *)&event;
	return kv->udata;
#endif
}

