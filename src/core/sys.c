#ifdef __linux__
#define _GNU_SOURCE
#endif /* __linux__ */

#include <error.h>
#include <fcntl.h>
#include <sys.h>
#include <sys/time.h>

#ifdef __APPLE__
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

static int syscall_settimeofday(const struct timeval *tv, const void *tz) {
	long result;
	__asm__ volatile(
	    "movq $170, %%rax\n"
	    "movq %1, %%rdi\n"
	    "movq %2, %%rsi\n"
	    "syscall\n"
	    "movq %%rax, %0\n"
	    : "=r"(result)
	    : "r"((long)tv), "r"((long)tz)
	    : "%rax", "%rdi", "%rsi", "%rcx", "%r11", "memory");
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

#endif /* __linux__ */

int sched_yield(void) {
#ifdef __linux__
	int ret = syscall_sched_yield();
#elif defined(__APPLE__)
	int ret = syscall(21);
#else
#error Unsupported platform. Supported platforms: __linux__ or __APPLE__
#endif

	if (ret < 0) {
		err = -ret;
		return -1;
	}
	return ret;
}

int open(const char *path, int flags, ...) {
#ifdef __linux__
	int ret = syscall_open(
	    path, flags,
	    0644); /* note: hard coded 0644 which is ok for our purposes */
#elif defined(__APPLE__)
	int ret = syscall(
	    5, path, flags,
	    0644); /* note: hard coded 0644 which is ok for our purposes */
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

int gettimeofday(struct timeval *tv, void *tz) {
#ifdef __linux__
	int ret = syscall_gettimeofday(tv, tz);
#elif defined(__APPLE__)
	int ret = syscall(116, tv, tz);
#else
#error Unsupported platform. Supported platforms: __linux__ or __APPLE__
#endif

	if (ret < 0) {
		err = -ret;
		return -1;
	}
	return ret;
}
int settimeofday(const struct timeval *tv, const struct timezone *tz) {
#ifdef __linux__
	int ret = syscall_settimeofday(tv, tz);
#elif defined(__APPLE__)
	int ret = syscall(122, tv, tz);
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

int fork(void) {
#ifdef __linux__
	int ret = syscall_fork();
#elif defined(__APPLE__)
	int ret = syscall(2);
#else
#error Unsupported platform. Supported platforms: __linux__ or __APPLE__
#endif

	if (ret < 0) {
		err = -ret;
		return -1;
	}
	return ret;
}

int pipe(int fds[2]) {
#ifdef __linux__
	int ret = syscall_pipe(fds);
#elif defined(__APPLE__)
	int ret = syscall(42, fds);
#else
#error Unsupported platform. Supported platforms: __linux__ or __APPLE__
#endif

	if (ret < 0) {
		err = -ret;
		return -1;
	}
	return ret;
}

#ifdef __linux__
int nanosleep(const struct timespec *req, struct timespec *rem) {
	int ret = syscall_nanosleep(req, rem);
	if (ret < 0) {
		err = -ret; /* Set custom err */
		return -1;
	}
	return ret;
}
#endif /* __linux__ */

#pragma GCC diagnostic pop

int set_micros(int64_t v) {
	struct timeval tv;

	if (v < 0) {
		err = EINVAL;
		return -1;
	}

	tv.tv_sec = v / 1000000;
	tv.tv_usec = v % 1000000;

	if (settimeofday(&tv, NULL) == -1) return -1;

	return 0;
}

int ocreate(const char *path) { return open(path, O_CREAT | O_RDWR, 0644); }

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

