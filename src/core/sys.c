#ifdef __linux__
#define _GNU_SOURCE
#endif /* __linux__ */

#include <error.h>
#include <fcntl.h>
#include <sys.h>
#include <sys/time.h>

#ifdef __APPLE__
#include <errno.h>
#include <unistd.h>
#endif /* __APPLE__ */

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#ifdef __linux__
static ssize_t syscall_write(int fd, const void *buf, size_t count) {
	long result;
	__asm__ volatile(
	    "movq $1, %%rax\n"
	    "movq %1, %%rdi\n"
	    "movq %2, %%rsi\n"
	    "movq %3, %%rdx\n"
	    "syscall\n"
	    "movq %%rax, %0\n"
	    : "=r"(result)
	    : "r"((long)fd), "r"((long)buf), "r"((long)count)
	    : "%rax", "%rdi", "%rsi", "%rdx", "%rcx", "%r11", "memory");
	return (ssize_t)result;
}

static int syscall_sched_yield(void) {
	long result;
	__asm__ volatile(
	    "movq $24, %%rax\n"
	    "syscall\n"
	    "movq %%rax, %0\n"
	    : "=r"(result)
	    :
	    : "%rax", "%rcx", "%r11", "memory");
	return (int)result;
}

static int syscall_open(const char *path, int flags, mode_t mode) {
	long result;
	__asm__ volatile(
	    "movq $2, %%rax\n"
	    "movq %1, %%rdi\n"
	    "movq %2, %%rsi\n"
	    "movq %3, %%rdx\n"
	    "syscall\n"
	    "movq %%rax, %0\n"
	    : "=r"(result)
	    : "r"((long)path), "r"((long)flags), "r"((long)mode)
	    : "%rax", "%rdi", "%rsi", "%rdx", "%rcx", "%r11", "memory");
	return (int)result;
}

static int syscall_gettimeofday(struct timeval *tv, void *tz) {
	long result;
	__asm__ volatile(
	    "movq $96, %%rax\n"
	    "movq %1, %%rdi\n"
	    "movq %2, %%rsi\n"
	    "syscall\n"
	    "movq %%rax, %0\n"
	    : "=r"(result)
	    : "r"((long)tv), "r"((long)tz)
	    : "%rax", "%rdi", "%rsi", "%rcx", "%r11", "memory");
	return (int)result;
}

static int syscall_nanosleep(const struct timespec *req, struct timespec *rem) {
	long result;
	__asm__ volatile(
	    "movq $35, %%rax\n" /* nanosleep syscall number */
	    "movq %1, %%rdi\n"	/* req */
	    "movq %2, %%rsi\n"	/* rem */
	    "syscall\n"
	    "movq %%rax, %0\n"
	    : "=r"(result)				       /* Output */
	    : "r"((long)req), "r"((long)rem)		       /* Inputs */
	    : "%rax", "%rdi", "%rsi", "%rcx", "%r11", "memory" /* Clobbered */
	);
	return (int)result;
}

static int syscall_unlink(const char *pathname) {
	long result;
	__asm__ volatile(
	    "movq $87, %%rax\n" /* unlink syscall number */
	    "movq %1, %%rdi\n"	/* pathname */
	    "syscall\n"
	    "movq %%rax, %0\n"
	    : "=r"(result)			       /* Output */
	    : "r"((long)pathname)		       /* Input */
	    : "%rax", "%rdi", "%rcx", "%r11", "memory" /* Clobbered */
	);
	return (int)result;
}

static ssize_t syscall_read(int fd, void *buf, size_t count) {
	long result;
	__asm__ volatile(
	    "movq $0, %%rax\n" /* read syscall number */
	    "movq %1, %%rdi\n" /* fd */
	    "movq %2, %%rsi\n" /* buf */
	    "movq %3, %%rdx\n" /* count */
	    "syscall\n"
	    "movq %%rax, %0\n"
	    : "=r"(result)				      /* Output */
	    : "r"((long)fd), "r"((long)buf), "r"((long)count) /* Inputs */
	    : "%rax", "%rdi", "%rsi", "%rdx", "%rcx", "%r11",
	      "memory" /* Clobbered */
	);
	return (ssize_t)result;
}

static long syscall_lseek(int fd, long offset, int whence) {
	long result;
	__asm__ volatile(
	    "movq $8, %%rax\n" /* lseek syscall number */
	    "movq %1, %%rdi\n" /* fd */
	    "movq %2, %%rsi\n" /* offset */
	    "movq %3, %%rdx\n" /* whence */
	    "syscall\n"
	    "movq %%rax, %0\n"
	    : "=r"(result)				    /* Output */
	    : "r"((long)fd), "r"(offset), "r"((long)whence) /* Inputs */
	    : "%rax", "%rdi", "%rsi", "%rdx", "%rcx", "%r11",
	      "memory" /* Clobbered */
	);
	return result;
}

static int syscall_fork(void) {
	long result;
	__asm__ volatile(
	    "movq $57, %%rax\n" /* fork syscall number */
	    "syscall\n"
	    "movq %%rax, %0\n"
	    : "=r"(result)		       /* Output */
	    :				       /* No inputs */
	    : "%rax", "%rcx", "%r11", "memory" /* Clobbered */
	);
	return (int)result;
}

static int syscall_pipe(int *fds) {
	long result;
	__asm__ volatile(
	    "movq $22, %%rax\n" /* pipe syscall number */
	    "movq %1, %%rdi\n"	/* fds */
	    "syscall\n"
	    "movq %%rax, %0\n"
	    : "=r"(result)			       /* Output */
	    : "r"((long)fds)			       /* Input */
	    : "%rax", "%rdi", "%rcx", "%r11", "memory" /* Clobbered */
	);
	return (int)result;
}

static void syscall_exit(int status) {
	__asm__ volatile(
	    "movq $60, %%rax\n" /* exit syscall number (Linux) */
	    "movq %0, %%rdi\n"	/* status */
	    "syscall\n"
	    :
	    : "r"((long)status)			       /* Input */
	    : "%rax", "%rdi", "%rcx", "%r11", "memory" /* Clobbered */
	);
}

static void *syscall_mmap(void *addr, size_t length, int prot, int flags,
			  int fd, long offset) {
	long result;
	__asm__ volatile(
	    "movq $9, %%rax\n" /* mmap syscall number (Linux) */
	    "movq %1, %%rdi\n" /* addr */
	    "movq %2, %%rsi\n" /* length */
	    "movq %3, %%rdx\n" /* prot */
	    "movq %4, %%r10\n" /* flags */
	    "movq %5, %%r8\n"  /* fd */
	    "movq %6, %%r9\n"  /* offset */
	    "syscall\n"
	    "movq %%rax, %0\n"
	    : "=r"(result) /* Output */
	    : "r"((long)addr), "r"((long)length), "r"((long)prot),
	      "r"((long)flags), "r"((long)fd), "r"(offset) /* Inputs */
	    : "%rax", "%rdi", "%rsi", "%rdx", "%r10", "%r8", "%r9", "%rcx",
	      "%r11", "memory" /* Clobbered */
	);
	return (void *)result;
}

static int syscall_munmap(void *addr, size_t length) {
	long result;
	__asm__ volatile(
	    "movq $11, %%rax\n" /* munmap syscall number (Linux) */
	    "movq %1, %%rdi\n"	/* addr */
	    "movq %2, %%rsi\n"	/* length */
	    "syscall\n"
	    "movq %%rax, %0\n"
	    : "=r"(result)				       /* Output */
	    : "r"((long)addr), "r"((long)length)	       /* Inputs */
	    : "%rax", "%rdi", "%rsi", "%rcx", "%r11", "memory" /* Clobbered */
	);
	return (int)result;
}

static int syscall_close(int fd) {
	long result;
	__asm__ volatile(
	    "movq $3, %%rax\n" /* close syscall number (Linux) */
	    "movq %1, %%rdi\n" /* fd */
	    "syscall\n"
	    "movq %%rax, %0\n"
	    : "=r"(result)			       /* Output */
	    : "r"((long)fd)			       /* Input */
	    : "%rax", "%rdi", "%rcx", "%r11", "memory" /* Clobbered */
	);
	return (int)result;
}

static int syscall_ftruncate(int fd, long length) {
	long result;
	__asm__ volatile(
	    "movq $77, %%rax\n" /* ftruncate syscall number (Linux) */
	    "movq %1, %%rdi\n"	/* fd */
	    "movq %2, %%rsi\n"	/* length */
	    "syscall\n"
	    "movq %%rax, %0\n"
	    : "=r"(result)				       /* Output */
	    : "r"((long)fd), "r"(length)		       /* Inputs */
	    : "%rax", "%rdi", "%rsi", "%rcx", "%r11", "memory" /* Clobbered */
	);
	return (int)result;
}

static int syscall_fdatasync(int fd) {
	long result;
	__asm__ volatile(
	    "movq $75, %%rax\n" /* fdatasync syscall number (Linux) */
	    "movq %1, %%rdi\n"	/* fd */
	    "syscall\n"
	    "movq %%rax, %0\n"
	    : "=r"(result)			       /* Output */
	    : "r"((long)fd)			       /* Input */
	    : "%rax", "%rdi", "%rcx", "%r11", "memory" /* Clobbered */
	);
	return (int)result;
}

static int syscall_epoll_create1(int flags) {
	long result;
	__asm__ volatile(
	    "movq $291, %%rax\n" /* epoll_create1 syscall number (Linux) */
	    "movq %1, %%rdi\n"	 /* flags */
	    "syscall\n"
	    "movq %%rax, %0\n"
	    : "=r"(result)			       /* Output */
	    : "r"((long)flags)			       /* Input */
	    : "%rax", "%rdi", "%rcx", "%r11", "memory" /* Clobbered */
	);
	return (int)result;
}

static int syscall_epoll_ctl(int epfd, int op, int fd,
			     struct epoll_event *event) {
	long result;
	__asm__ volatile(
	    "movq $233, %%rax\n" /* epoll_ctl syscall number (Linux) */
	    "movq %1, %%rdi\n"	 /* epfd */
	    "movq %2, %%rsi\n"	 /* op */
	    "movq %3, %%rdx\n"	 /* fd */
	    "movq %4, %%r10\n"	 /* event */
	    "syscall\n"
	    "movq %%rax, %0\n"
	    : "=r"(result) /* Output */
	    : "r"((long)epfd), "r"((long)op), "r"((long)fd),
	      "r"((long)event) /* Inputs */
	    : "%rax", "%rdi", "%rsi", "%rdx", "%r10", "%rcx", "%r11",
	      "memory" /* Clobbered */
	);
	return (int)result;
}

static int syscall_epoll_wait(int epfd, struct epoll_event *events,
			      int maxevents, int timeout) {
	long result;
	__asm__ volatile(
	    "movq $232, %%rax\n" /* epoll_wait syscall number (Linux) */
	    "movq %1, %%rdi\n"	 /* epfd */
	    "movq %2, %%rsi\n"	 /* events */
	    "movq %3, %%rdx\n"	 /* maxevents */
	    "movq %4, %%r10\n"	 /* timeout */
	    "syscall\n"
	    "movq %%rax, %0\n"
	    : "=r"(result) /* Output */
	    : "r"((long)epfd), "r"((long)events), "r"((long)maxevents),
	      "r"((long)timeout) /* Inputs */
	    : "%rax", "%rdi", "%rsi", "%rdx", "%r10", "%rcx", "%r11",
	      "memory" /* Clobbered */
	);
	return (int)result;
}

#endif /* __linux__ */

int open(const char *path, int flags, ...) {
#ifdef __linux__
	int ret = syscall_open(
	    path, flags,
	    0600); /* note: hard coded 0600 which is ok for our purposes */
#elif defined(__APPLE__)
	int ret = syscall(
	    5, path, flags,
	    0600); /* note: hard coded 0600 which is ok for our purposes */
#else
#error Unsupported platform. Supported platforms: __linux__ or __APPLE__
#endif

	if (ret < 0) {
		err = -ret;
		return -1;
	}
	return ret;
}

ssize_t write(int fd, const void *buf, size_t count) {
#ifdef __linux__
	int ret = syscall_write(fd, buf, count);
#elif defined(__APPLE__)
	int ret = syscall(4, fd, buf, count);
#else
#error Unsupported platform. Supported platforms: __linux__ or __APPLE__
#endif

	if (ret < 0) {
		err = -ret;
		return -1;
	}
	return ret;
}

int unlink(const char *path) {
#ifdef __linux__
	int ret = syscall_unlink(path);
#elif defined(__APPLE__)
	int ret = syscall(10, path);
#else
#error Unsupported platform. Supported platforms: __linux__ or __APPLE__
#endif

	if (ret < 0) {
		err = -ret;
		return -1;
	}
	return ret;
}

ssize_t read(int fd, void *buf, size_t count) {
#ifdef __linux__
	int ret = syscall_read(fd, buf, count);
#elif defined(__APPLE__)
	int ret = syscall(3, fd, buf, count);
#else
#error Unsupported platform. Supported platforms: __linux__ or __APPLE__
#endif

	if (ret < 0) {
		err = -ret;
		return -1;
	}
	return ret;
}

off_t lseek(int fd, off_t offset, int whence) {
#ifdef __linux__
	int ret = syscall_lseek(fd, offset, whence);
#elif defined(__APPLE__)
	int ret = syscall(199, fd, offset, whence);
#else
#error Unsupported platform. Supported platforms: __linux__ or __APPLE__
#endif

	if (ret < 0) {
		err = -ret;
		return -1;
	}
	return ret;
}

void exit(int status) {
#ifdef __linux__
	syscall_exit(status);
#elif defined(__APPLE__)
	syscall(1, status);
#else
#error Unsupported platform. Supported platforms: __linux__ or __APPLE__
#endif
}

int close(int fd) {
	int ret;
#ifdef __linux__
	ret = syscall_close(fd);
#elif defined(__APPLE__)
	ret = syscall(6, fd);
#else
#error Unsupported platform. Supported platforms: __linux__ or __APPLE__
#endif
	if (ret < 0) {
		err = -ret; /* Set custom err */
		return -1;
	}
	return ret;
}

int ftruncate(int fd, off_t length) {
	int ret;
#ifdef __linux__
	ret = syscall_ftruncate(fd, length);
#elif defined(__APPLE__)
	ret = syscall(201, fd, length);
#else
#error Unsupported platform. Supported platforms: __linux__ or __APPLE__
#endif
	if (ret < 0) {
		err = -ret; /* Set custom err */
		return -1;
	}
	return ret;
}

int fdatasync(int fd) {
	int ret;
#ifdef __linux__
	ret = syscall_fdatasync(fd);
#elif defined(__APPLE__)
	ret = syscall(187, fd);
#else
#error Unsupported platform. Supported platforms: __linux__ or __APPLE__
#endif
	if (ret < 0) {
		err = -ret; /* Set custom err */
		return -1;
	}
	return ret;
}

int munmap(void *addr, size_t len) {
	int ret;
#ifdef __linux__
	ret = syscall_munmap(addr, len);
#elif defined(__APPLE__)
	ret = syscall(73, addr, len);
#else
#error Unsupported platform. Supported platforms: __linux__ or __APPLE__
#endif
	if (ret < 0) {
		err = -ret; /* Set custom err */
		return -1;
	}
	return ret;
}

#ifdef __linux__
int fork(void) {
	int ret = syscall_fork();

	if (ret < 0) {
		err = -ret;
		return -1;
	}
	return ret;
}

int pipe(int fds[2]) {
	int ret = syscall_pipe(fds);

	if (ret < 0) {
		err = -ret;
		return -1;
	}
	return ret;
}

void *mmap(void *addr, size_t length, int prot, int flags, int fd,
	   off_t offset) {
	void *ret;
	ret = syscall_mmap(addr, length, prot, flags, fd, offset);

	if ((long)ret == -1) {
		err = -(long)ret;
		return (void *)-1;
	}
	return ret;
}

int sched_yield(void) {
	int ret = syscall_sched_yield();
	if (ret < 0) {
		err = -ret;
		return -1;
	}
	return ret;
}

int nanosleep(const struct timespec *req, struct timespec *rem) {
	int ret = syscall_nanosleep(req, rem);
	if (ret < 0) {
		err = -ret;
		return -1;
	}
	return ret;
}

int gettimeofday(struct timeval *tv, void *tz) {
	int ret = syscall_gettimeofday(tv, tz);

	if (ret < 0) {
		err = -ret;
		return -1;
	}
	return ret;
}

int epoll_create1(int ign) {
	int ret = syscall_epoll_create1(ign);
	if (ret < 0) {
		err = -ret;
		return -1;
	}
	return ret;
}

int epoll_wait(int epfd, struct epoll_event *events, int maxevents,
	       int timeout) {
	int ret = syscall_epoll_wait(epfd, events, maxevents, timeout);
	if (ret < 0) {
		err = -ret;
		return -1;
	}
	return ret;
}

int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event) {
	int ret = syscall_epoll_ctl(epfd, op, fd, event);
	if (ret < 0) {
		err = -ret;
		return -1;
	}
	return ret;
}

#endif /* __linux__ */

#pragma GCC diagnostic pop

int multiplex(void) {
#ifdef __linux__
	return epoll_create1(0);
#elif defined(__APPLE__)
	return kqueue();
#else
#error Unsupported platform. Supported platforms: __linux__ or __APPLE__
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
	if (res < 0) err = -errno;
	if (res < 0) return -1;
	return 0;
#else
#error Unsupported platform. Supported platforms: __linux__ or __APPLE__
#endif
}

int mwait(int multiplex, void *events, int max_events, int64_t timeout_millis) {
#ifdef __linux__
	int timeout = (timeout_millis >= 0) ? (int)timeout_millis : -1;

	return epoll_wait(multiplex, (struct epoll_event *)events, max_events,
			  timeout);
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

int event_getfd(Event event) {
#ifdef __linux__
	struct epoll_event *epoll_ev = (struct epoll_event *)&event;
	return epoll_ev->data.fd;
#elif defined(__APPLE__)
	struct kevent *kv = (struct kevent *)&event;
	return kv->ident;
#else
#error Unsupported platform. Supported platforms: __linux__ or __APPLE__
#endif
}

int event_is_read(Event event) {
#ifdef __linux__
	struct epoll_event *epoll_ev = (struct epoll_event *)&event;
	return epoll_ev->events & EPOLLIN;
#elif defined(__APPLE__)
	struct kevent *kv = (struct kevent *)&event;
	return kv->filter == EVFILT_READ;
#else
#error Unsupported platform. Supported platforms: __linux__ or __APPLE__
#endif
}

int event_is_write(Event event) {
#ifdef __linux__
	struct epoll_event *epoll_ev = (struct epoll_event *)&event;
	return epoll_ev->events & EPOLLOUT;
#elif defined(__APPLE__)
	struct kevent *kv = (struct kevent *)&event;
	return kv->filter == EVFILT_WRITE;
#else
#error Unsupported platform. Supported platforms: __linux__ or __APPLE__
#endif
}

void *event_attachment(Event event) {
#ifdef __linux__
	struct epoll_event *epoll_ev = (struct epoll_event *)&event;
	return epoll_ev->data.ptr;
#elif defined(__APPLE__)
	struct kevent *kv = (struct kevent *)&event;
	return kv->udata;
#else
#error Unsupported platform. Supported platforms: __linux__ or __APPLE__
#endif
}

int file(const char *path) { return open(path, O_CREAT | O_RDWR, 0600); }

int64_t micros(void) {
	struct timeval tv;

	if (gettimeofday(&tv, NULL) == -1) {
		return -1;
	}

	return (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

int sleepm(uint64_t millis) {
	struct timespec req;
	req.tv_sec = millis / 1000;
	req.tv_nsec = (millis % 1000) * 1000000;
	return nanosleep(&req, &req);
}

