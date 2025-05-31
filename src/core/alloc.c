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
#include <lock.h>
#include <misc.h>
#include <robust.h>
#include <stdio.h>
#include <types.h>

#define MIN_SIZE MIN_ALIGN_SIZE

typedef struct {
	uint64_t ref_count;
	Lock lock;

} GlobalAllocatorHeader;

typedef struct {
	int fd;
	void *base;
	char path[64];
} _GlobalAllocator__;

STATIC _GlobalAllocator__ allocator;

STATIC void __attribute__((constructor)) load_global_allocator(void) {
	uint128_t value;
	GlobalAllocatorHeader *header;

	memset(allocator.path, 0, 64);
	stringcpy(allocator.path, "/tmp/ga_");
	if (getentropy(&value, sizeof(uint128_t))) {
		print_error("getentropy");
		exit(-1);
	}
	if (b64_encode(&value, sizeof(uint128_t),
		       allocator.path + strlen(allocator.path)) == -1) {
		print_error("b64_encode");
		exit(-1);
	}

	allocator.path[strlen("/tmp/ga_") + 20] = '\0';
	allocator.fd = file(allocator.path);
	if (allocator.fd < 0) {
		print_error("global allocator open");
		exit(-1);
	}

	if (fresize(allocator.fd, MIN_SIZE)) {
		print_error("ftruncate");
		exit(-1);
	}
	allocator.base = fmap(allocator.fd);
	if (allocator.base == NULL) {
		print_error("fmap");
		exit(-1);
	}

	header = (GlobalAllocatorHeader *)allocator.base;
	header->ref_count = 1;
	header->lock = LOCK_INIT;
}

STATIC void __attribute__((destructor)) destroy_global_allocator(void) {
	GlobalAllocatorHeader *header = (GlobalAllocatorHeader *)allocator.base;
	bool do_unlink = false;

	{
		LockGuard lg = lock_write(&header->lock);
		if (--(header->ref_count) == 0) do_unlink = true;
	}
	close(allocator.fd);
	if (do_unlink) {
		unlink(allocator.path);
	}
}

void *alloc(size_t size) { return NULL; }

void release(void *ptr);
void *resize(void *ptr, size_t size);

void ga_init(void) {
	GlobalAllocatorHeader *header = (GlobalAllocatorHeader *)allocator.base;
	LockGuard lg = lock_write(&header->lock);
	++(header->ref_count);
	robust_init();
}
