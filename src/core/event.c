#include <errno.h>
#include <error.h>
#include <sys.h>
#include <time.h>

#ifdef __linux__
#define DECLARE_SYSCALL(ret_type, name, num, ...) \
	ret_type syscall_##name(__VA_ARGS__);     \
	__asm__(".global syscall_" #name          \
		"\n"                              \
		"syscall_" #name                  \
		":\n"                             \
		"    movq $" #num                 \
		", %rax\n"                        \
		"    syscall\n"                   \
		"    ret\n");

#define IMPL_WRAPPER(ret_type, name, ...)         \
	ret_type v = syscall_##name(__VA_ARGS__); \
	if (v < 0) {                              \
		err = -v;                         \
		return -1;                        \
	}                                         \
	return v;

DECLARE_SYSCALL(int, epoll_create1, 291, int ignore)

DECLARE_SYSCALL(int, epoll_ctl, 233, int epfd, int op, int fd, Event *event)

DECLARE_SYSCALL(int, epoll_wait, 232, int epfd, Event *events, int maxevents,
		int timeout)

int sys_epoll_create1(int ignore) { IMPL_WRAPPER(int, epoll_create1, ignore) }
int sys_epoll_ctl(int epfd, int op, int fd, Event *event) {
	IMPL_WRAPPER(int, epoll_ctl, epfd, op, fd, event)
}
int sys_epoll_wait(int epfd, Event *events, int maxevents, int timeout) {
	IMPL_WRAPPER(int, epoll_wait, epfd, events, maxevents, timeout)
}
#endif /* __linux__ */

int multiplex() {
#ifdef __linux__
	int fd = sys_epoll_create1(0);
#elif defined(__APPLE__)
	int fd = kqueue();
#else
#error Unsupported platform. Supported platforms: __linux__ or __APPLE__
#endif
	return fd;
}
int multiplex_register(int multiplex, int fd, int flags, void *attach) {
#ifdef __linux__
	struct epoll_event ev;
	int event_flags = 0;

	if (flags & REGISTER_READ) {
		event_flags |= EPOLLIN;
	}

	if (flags & REGISTER_WRITE) {
		event_flags |= EPOLLOUT;
	}

	ev.events = event_flags;
	if (attach == NULL)
		ev.data.fd = fd;
	else
		ev.data.ptr = attach;

	if (sys_epoll_ctl(multiplex, EPOLL_CTL_ADD, fd, &ev) < 0) {
		if (errno == EEXIST) {
			if (sys_epoll_ctl(multiplex, EPOLL_CTL_MOD, fd, &ev) <
			    0) {
				err = -errno;
				return -1;
			}
		} else {
			err = -errno;
			return -1;
		}
	}

	return 0;
#elif defined(__APPLE__)
	struct kevent change_event[2];

	int event_count = 0;

	if (flags & REGISTER_READ) {
		EV_SET(&change_event[event_count], fd, EVFILT_READ,
		       EV_ADD | EV_ENABLE | EV_CLEAR, 0, 0, attach);
		event_count++;
	}

	if (flags & REGISTER_WRITE) {
		EV_SET(&change_event[event_count], fd, EVFILT_WRITE,
		       EV_ADD | EV_ENABLE | EV_CLEAR, 0, 0, attach);
		event_count++;
	}

	int res = kevent(multiplex, change_event, event_count, NULL, 0, NULL);
	if (res < 0) err = -errno;
	if (res < 0) return -1;
	return 0;
#else
#error Unsupported platform. Supported platforms: __linux__ or __APPLE__
#endif
}

/*
int multiplex_wait(int multiplex, Event events[], int max_events,
		   int64_t timeout) {
	return 0;
}
*/

int multiplex_wait(int multiplex, Event events[], int max_events,
		   int64_t timeout_millis) {
#ifdef __linux__
	int timeout = (timeout_millis >= 0) ? (int)timeout_millis : -1;

	return sys_epoll_wait(multiplex, (struct epoll_event *)events,
			      max_events, timeout);
#elif defined(__APPLE__)
	struct timespec ts;
	struct timespec *timeout_ptr = NULL;

	if (timeout_millis >= 0) {
		ts.tv_sec = timeout_millis / 1000;
		ts.tv_nsec = (timeout_millis % 1000) * 1000000;
		timeout_ptr = &ts;
	}

	return kevent(multiplex, NULL, 0, (struct kevent *)events, max_events,
		      timeout_ptr);
#else
#error Unsupported platform. Supported platforms: __linux__ or __APPLE__
#endif
}

int event_fd(Event *event) {
#ifdef __linux__
	struct epoll_event *epoll_ev = (struct epoll_event *)event;
	return epoll_ev->data.fd;
#elif defined(__APPLE__)
	struct kevent *kv = (struct kevent *)event;
	return kv->ident;
#else
#error Unsupported platform. Supported platforms: __linux__ or __APPLE__
#endif
}
bool event_is_read(Event *event) {
#ifdef __linux__
	struct epoll_event *epoll_ev = (struct epoll_event *)event;
	return epoll_ev->events & EPOLLIN;
#elif defined(__APPLE__)
	struct kevent *kv = (struct kevent *)event;
	return kv->filter == EVFILT_READ;
#else
#error Unsupported platform. Supported platforms: __linux__ or __APPLE__
#endif
}

bool event_is_write(Event *event) {
#ifdef __linux__
	struct epoll_event *epoll_ev = (struct epoll_event *)event;
	return epoll_ev->events & EPOLLOUT;
#elif defined(__APPLE__)
	struct kevent *kv = (struct kevent *)event;
	return kv->filter == EVFILT_WRITE;
#else
#error Unsupported platform. Supported platforms: __linux__ or __APPLE__
#endif
}

void *event_ptr(void *event) {
#ifdef __linux__
	struct epoll_event *epoll_ev = (struct epoll_event *)event;
	return epoll_ev->data.ptr;
#elif defined(__APPLE__)
	struct kevent *kv = (struct kevent *)event;
	return kv->udata;
#else
#error Unsupported platform. Supported platforms: __linux__ or __APPLE__
#endif
}
