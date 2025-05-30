#include <lock.h>
#include <salloc.h>
#include <stdio.h>
#include <sys.h>

typedef struct {
	void *base;
	int fd;
	Lock lock;
} SContext;

SContext _global_scontext__;

void __attribute__((constructor)) __setup_global_allocator_context() {
	/*printf("setup global alloc\n");*/
}

void __attribute__((destructor)) __teardown_global_allocator_context() {
	/*printf("tear down global alloc\n");*/
}

/*
int salloc_init(SContext *ctx, const char *path) {
	SContextImpl *impl = (SContextImpl *)ctx;
	int fd = file(path);
	void *base;
	if (fd < 0) return -1;
	base = fmap(fd);
	if (!base) {
		close(fd);
		return -1;
	}
	impl->base = base;
	impl->fd = fd;
	return 0;
}
*/
void *salloc(size_t size) {
	if (size == 0) {
		;
	}
	return NULL;
}

void srelease(void *ptr);
void *sresize(void *ptr, size_t size);
