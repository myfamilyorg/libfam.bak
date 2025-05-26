#ifdef __linux__
#define _GNU_SOURCE
#endif /* __linux__ */

#include <error.h>
#include <fcntl.h>
#include <sys.h>
#include <sys/time.h>
/*#include <time.h>*/

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

int nanosleep(const struct timespec *req, struct timespec *rem) {
#ifdef __linux__
	int ret = syscall_nanosleep(req, rem);
#elif defined(__APPLE__)
	int ret = syscall(199, req, rem);
#else
#error Unsupported platform. Supported platforms: __linux__ or __APPLE__
#endif
	if (ret < 0) {
		err = -ret; /* Set custom err */
		return -1;
	}
	return ret;
}

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

int sleep_millis(uint64_t millis) {
	struct timespec req;
	req.tv_sec = millis / 1000;
	req.tv_nsec = (millis % 1000) * 1000000;
	return nanosleep(&req, &req);
}

