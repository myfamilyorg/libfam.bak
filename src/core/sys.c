#include <error.h>
#include <fcntl.h>
#include <sys.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <unistd.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

int sched_yield(void) {
#ifdef __linux__
	int ret = syscall(124);
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

