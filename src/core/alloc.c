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
#include <misc.h>
#include <robust.h>
#include <stdio.h>
#include <types.h>

#define MIN_SIZE CHUNK_SIZE

#define NEXT_FREE_BIT(base, max, result)                                     \
	do {                                                                 \
		(result) = (uint64_t) - 1;                                   \
		uint64_t *bitmap =                                           \
		    (uint64_t *)((unsigned char *)(base) +                   \
				 sizeof(GlobalAllocatorMetadata));           \
		size_t max_words = ((max) + 63) >> 6;                        \
		while (allocator.last_free < max_words) {                    \
			if (bitmap[allocator.last_free] !=                   \
			    0xFFFFFFFFFFFFFFFF) {                            \
				uint64_t word = bitmap[allocator.last_free]; \
				size_t bit_value = __builtin_ctzll(~word);   \
				size_t index =                               \
				    allocator.last_free * 64 + bit_value;    \
				if (index < max) {                           \
					(result) = index;                    \
					break;                               \
				}                                            \
			}                                                    \
			allocator.last_free++;                               \
		}                                                            \
	} while (0)

#define SET_BIT(base, index)                                           \
	do {                                                           \
		unsigned char *tmp;                                    \
		tmp = (unsigned char *)(base);                         \
		tmp[sizeof(GlobalAllocatorMetadata) + (index >> 3)] |= \
		    0x1 << (index & 7);                                \
	} while (0);

typedef struct {
	uint64_t ref_count;
	RobustLock lock;
} GlobalAllocatorMetadata;

typedef struct {
	GlobalAllocatorMetadata meta;
	byte bitmap[CHUNK_SIZE - sizeof(GlobalAllocatorMetadata)];
} GlobalAllocatorHeader;

typedef struct {
	int fd;
	void *base;
	char path[64];
	uint64_t last_free;
	size_t last_map_size;
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
	allocator.last_free = 0;
	if (allocator.fd < 0) {
		print_error("global allocator open");
		exit(-1);
	}

	if (fresize(allocator.fd, MIN_SIZE)) {
		print_error("ftruncate");
		exit(-1);
	}
	allocator.last_map_size = MIN_SIZE;
	allocator.base = fmap(allocator.fd);
	if (allocator.base == NULL) {
		print_error("fmap");
		exit(-1);
	}

	header = (GlobalAllocatorHeader *)allocator.base;
	header->meta.ref_count = 1;
	header->meta.lock = ROBUST_LOCK_INIT;
}

STATIC void __attribute__((destructor)) destroy_global_allocator(void) {
	GlobalAllocatorHeader *header = (GlobalAllocatorHeader *)allocator.base;
	bool do_unlink = false;

	{
		RobustGuard lg = robust_lock(&header->meta.lock);
		if (--(header->meta.ref_count) == 0) do_unlink = true;
	}
	close(allocator.fd);
	if (do_unlink) {
		unlink(allocator.path);
	}
}

STATIC size_t calculate_slab_size(size_t value) {
	if (value <= 8) return 8;
	return 1UL << (64 - __builtin_clzll(value - 1));
}

STATIC int check_fsize(uint64_t chunk) {
	int res;
	if (fsize(allocator.fd) < (chunk + 2) * CHUNK_SIZE) {
		/*
		printf("fresize\n");
		res = fresize(allocator.fd, (chunk + 2) * CHUNK_SIZE);
		if (res == -1) return -1;
		munmap(allocator.base, allocator.last_map_size);
		allocator.last_map_size = (chunk + 2) * CHUNK_SIZE;
		allocator.base = fmap(allocator.fd);
		printf("ok\n");
		*/
	}
	return 0;
}

STATIC uint64_t allocate_chunk(void) {
	uint64_t ret;
	NEXT_FREE_BIT(allocator.base, CHUNK_SIZE * 8, ret);
	if (ret != (uint64_t)-1) {
		if (check_fsize(ret)) return (uint64_t)-1;
		SET_BIT(allocator.base, ret);
		printf("set bit\n");
	}
	return ret;
}

STATIC void *alloc_slab(size_t slab_size) { return NULL; }

void *alloc(size_t size) {
	if (size > CHUNK_SIZE) {
		err = EINVAL;
		return NULL;
	} else if (size <= MAX_SLAB_SIZE) {
		size = calculate_slab_size(size);
		return alloc_slab(size);
	} else {
		GlobalAllocatorHeader *header =
		    (GlobalAllocatorHeader *)allocator.base;
		RobustGuard rg = robust_lock(&header->meta.lock);
		uint64_t chunk_id = allocate_chunk();
		if (chunk_id == (uint64_t)-1) return NULL;
		printf("allocbase=%lu\n", allocator.base);
		printf("x\n");
		return NULL;
		/*
		return (void *)((chunk_id + 1) * CHUNK_SIZE +
				(size_t)allocator.base);
				*/
	}
}

void release(void *ptr);
void *resize(void *ptr, size_t size);

void ga_init(void) {
	GlobalAllocatorHeader *header = (GlobalAllocatorHeader *)allocator.base;
	RobustGuard lg = robust_lock(&header->meta.lock);
	++(header->meta.ref_count);
}
