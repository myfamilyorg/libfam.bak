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
#include <atomic.h>
#include <env.h>
#include <error.h>
#include <misc.h>
#define SHM_SIZE_DEFAULT (CHUNK_SIZE * 64)

int printf(const char *, ...);

#ifndef CHUNK_SIZE
#define CHUNK_SIZE (0x1 << 22) /* 4mb */
#endif

#ifndef MAX_SLAB_SIZE
#define MAX_SLAB_SIZE (CHUNK_SIZE >> 2) /* 1mb */
#endif

#define NEXT_FREE_BIT(base, max, result, last_free_ptr)                       \
	do {                                                                  \
		size_t max_words;                                             \
		uint64_t *bitmap;                                             \
		size_t local_last_free;                                       \
		(result) = (uint64_t)-1;                                      \
		bitmap = (uint64_t *)((unsigned char *)(base));               \
		max_words = ((max) + 63) >> 6;                                \
		local_last_free = ALOAD(last_free_ptr);                       \
		while (local_last_free < max_words) {                         \
			if (bitmap[local_last_free] != ~0UL) {                \
				uint64_t word = bitmap[local_last_free];      \
				size_t bit_value = __builtin_ctzll(~word);    \
				size_t index =                                \
				    local_last_free * 64 + bit_value;         \
				if (index < max) {                            \
					if (__cas64(                          \
						&bitmap[local_last_free],     \
						&word,                        \
						word | (1UL << bit_value))) { \
						(result) = index;             \
						break;                        \
					} else                                \
						continue;                     \
				}                                             \
			}                                                     \
			local_last_free++;                                    \
		}                                                             \
		if ((result) == (uint64_t)-1 &&                               \
		    local_last_free > ALOAD(last_free_ptr)) {                 \
			uint64_t expected = ALOAD(last_free_ptr);             \
			while (expected < local_last_free &&                  \
			       !__cas64(last_free_ptr, &expected,             \
					local_last_free))                     \
				expected = ALOAD(last_free_ptr);              \
		}                                                             \
	} while (0)
#define RELEASE_BIT(base, index, last_free_ptr)                           \
	do {                                                              \
		size_t new_last_free = index / 64;                        \
		uint64_t expected = ALOAD(last_free_ptr);                 \
		uint64_t *bitmap = (uint64_t *)((unsigned char *)(base)); \
		__and64(&bitmap[index / 64], ~(1UL << (index % 64)));     \
		while (expected > new_last_free &&                        \
		       !__cas64(last_free_ptr, &expected, new_last_free)) \
			expected = ALOAD(last_free_ptr);                  \
	} while (0)
#define CHUNK_OFFSET                                             \
	((uint8_t *)((size_t)memory_base + sizeof(AllocHeader) + \
		     bitmap_pages * PAGE_SIZE))
#define BITMAP_CAPACITY(slab_size) \
	((8UL * (CHUNK_SIZE - sizeof(Chunk))) / (1 + 8 * (slab_size)))
#define BITMAP_SIZE(slab_size) (((BITMAP_CAPACITY(slab_size) + 127) / 128) * 16)

typedef struct {
	uint64_t slab_pointers[32];
	uint64_t last_free;
} AllocHeaderData;

typedef struct {
	AllocHeaderData data;
	uint8_t padding[PAGE_SIZE - sizeof(AllocHeaderData)];
} AllocHeader;

typedef struct {
	uint32_t slab_size;
	uint64_t last_free;
	uint64_t next;
	uint8_t padding[12];
} Chunk;

static void *memory_base = NULL;
static uint64_t bitmap_pages = 0;
static uint64_t bitmap_bits = 0;

#if MEMSAN == 1
static uint64_t *allocated_bytes;
static void __attribute__((constructor)) setup_allocated_bytes(void) {
	allocated_bytes = smap(sizeof(uint64_t));
	*allocated_bytes = 0;
}
uint64_t get_allocated_bytes(void) { return *allocated_bytes; }
#endif /* MEMSAN */

STATIC size_t get_memory_bytes(void) {
	size_t shm_size = SHM_SIZE_DEFAULT;
	char *smembytes = getenv("SHARED_MEMORY_BYTES");
	if (smembytes) {
		size_t bytes = string_to_uint128(smembytes, strlen(smembytes));
		if (bytes != 0 && bytes % CHUNK_SIZE != 0) {
			const char *msg =
			    "WARN: SHARED_MEMORY_BYTES must be divisible by "
			    "CHUNK_SIZE. Using default.\n";
			write(2, msg, strlen(msg));
		} else {
			shm_size = bytes;
		}
	}
	return shm_size;
}

STATIC void __attribute__((constructor)) init_memory(void) {
	if (memory_base == NULL) {
		size_t bytes = get_memory_bytes();
		uint64_t bitmap_bytes;
		int i;
		AllocHeader *h;

		bitmap_bits = bytes / CHUNK_SIZE;
		bitmap_bytes = (bitmap_bits + 7) / 8;
		bitmap_pages = (bitmap_bytes + PAGE_SIZE - 1) / PAGE_SIZE;
		memory_base = smap(bytes + bitmap_pages * PAGE_SIZE +
				   sizeof(AllocHeader));
		if (memory_base == NULL)
			panic("Could not allocate shared memory.");

		h = (AllocHeader *)memory_base;
		for (i = 0; i < 32; i++)
			h->data.slab_pointers[i] = (uint64_t)-1;
	}
}

STATIC uint64_t allocate_chunk(void) {
	AllocHeader *alloc_header = (AllocHeader *)memory_base;
	uint64_t res;
	NEXT_FREE_BIT((void *)((size_t)memory_base + sizeof(AllocHeader)),
		      bitmap_bits, res, &alloc_header->data.last_free);
	return res;
}

STATIC size_t calculate_slab_size(size_t value) {
	if (value <= 8) return 8;
	return 1UL << (64 - __builtin_clzll(value - 1));
}

STATIC size_t calculate_slab_index(size_t value) {
	size_t n;
	if (value <= 8) return 0;
	n = 64 - __builtin_clzll(value - 1);
	return n - 3;
}

STATIC uint64_t atomic_load_or_allocate(uint64_t *ptr, uint32_t slab_size) {
	uint64_t cur;
	uint64_t new_value;
	uint64_t expected = (uint64_t)-1;
	AllocHeader *alloc_header = (AllocHeader *)memory_base;
	Chunk *chunk;

	do {
		cur = ALOAD(ptr);
		if (cur != (uint64_t)-1) break;

		new_value = allocate_chunk();
		if (new_value == ((uint64_t)-1)) break;
		chunk = (void *)(new_value * CHUNK_SIZE + CHUNK_OFFSET);
		memset(chunk, 0, sizeof(Chunk) + BITMAP_SIZE(slab_size));
		chunk->next = (uint64_t)-1;
		chunk->slab_size = slab_size;
		chunk->last_free = 0;

		if (__cas64(ptr, &expected, new_value)) {
			cur = new_value;
			break;
		}

		RELEASE_BIT((void *)((size_t)memory_base + sizeof(AllocHeader)),
			    new_value, &alloc_header->data.last_free);
		expected = (uint64_t)-1;
	} while (true);

	return cur;
}

STATIC void *allocate_slab(size_t size) {
	AllocHeader *header = memory_base;
	int index = calculate_slab_index(size);
	void *chunk_bitmap;
	Chunk *chunk;
	size_t slab_size = calculate_slab_size(size);
	uint64_t max, ret,
	    cur = atomic_load_or_allocate(&header->data.slab_pointers[index],
					  slab_size);
	void *ret_ptr = NULL;
	while (cur != (uint64_t)-1) {
		max = BITMAP_CAPACITY(slab_size);
		chunk = (Chunk *)(CHUNK_OFFSET + cur * CHUNK_SIZE);
		chunk_bitmap = (void *)((size_t)chunk + sizeof(Chunk));
		NEXT_FREE_BIT(chunk_bitmap, max, ret, &chunk->last_free);
		if (ret != (uint64_t)-1) {
			uint64_t bitmap_size = BITMAP_SIZE(slab_size);
			ret_ptr = (uint8_t *)chunk + sizeof(Chunk) +
				  bitmap_size + (ret * slab_size);
			break;
		}
		cur = atomic_load_or_allocate(&chunk->next, slab_size);
	}

#if MEMSAN == 1
	if (ret_ptr) {
		__add64(allocated_bytes, slab_size);
	}
#endif /* MEMSAN */

	return ret_ptr;
}

void *alloc(size_t size) {
	void *ret;
	if (size > CHUNK_SIZE) {
		err = EINVAL;
		return NULL;
	} else if (size > MAX_SLAB_SIZE) {
		uint64_t chunk = allocate_chunk();
		if (chunk == (uint64_t)-1) return NULL;
#if MEMSAN == 1
		__add64(allocated_bytes, CHUNK_SIZE);
#endif /* MEMSAN */

		ret = (void *)((size_t)memory_base + sizeof(AllocHeader) +
			       bitmap_pages * PAGE_SIZE + CHUNK_SIZE * chunk);
	} else {
		ret = allocate_slab(size);
	}

	printf("----------->alloc =%p\n", ret);
	return ret;
}

void release(void *ptr) {
	AllocHeader *alloc_header = memory_base;
	size_t offset =
	    (size_t)ptr - ((size_t)memory_base + sizeof(AllocHeader) +
			   bitmap_pages * PAGE_SIZE);
	printf("--------->release %p\n", ptr);
	if (ptr == NULL ||
	    (size_t)ptr < (size_t)memory_base + sizeof(AllocHeader) +
			      bitmap_pages * PAGE_SIZE ||
	    (size_t)ptr >= (size_t)memory_base + sizeof(AllocHeader) +
			       bitmap_pages * PAGE_SIZE + get_memory_bytes()) {
		err = EINVAL;
		printf("einval\n");
		return;
	}
	printf("a\n");

	if (offset % CHUNK_SIZE == 0) {
		RELEASE_BIT((void *)((size_t)memory_base + sizeof(AllocHeader)),
			    offset / CHUNK_SIZE, &alloc_header->data.last_free);
#if MEMSAN == 1
		__sub64(allocated_bytes, CHUNK_SIZE);
#endif /* MEMSAN */
	} else {
		Chunk *chunk = (Chunk *)(CHUNK_OFFSET +
					 (offset / CHUNK_SIZE) * CHUNK_SIZE);
		void *base = (void *)((size_t)chunk + sizeof(Chunk));
		uint64_t slab_size = chunk->slab_size;
		uint64_t bitmap_size = BITMAP_SIZE(slab_size);
		uint64_t index =
		    ((size_t)ptr - ((size_t)base + bitmap_size)) / slab_size;
		RELEASE_BIT(base, index, &chunk->last_free);
#if MEMSAN == 1
		__sub64(allocated_bytes, slab_size);
#endif /* MEMSAN */
	}

	printf("b\n");
}

void *resize(void *ptr, size_t new_size) {
	size_t base_offset;
	size_t total_size;
	size_t offset;
	AllocHeader *h;
	void *new_ptr;
	size_t old_size;

	/* Handle null pointer or zero size */
	if (ptr == NULL) {
		return alloc(new_size);
	}
	if (new_size == 0) {
		release(ptr);
		return NULL;
	}

	/* Validate new size */
	if (new_size > CHUNK_SIZE) {
		err = EINVAL;
		return NULL;
	}

	/* Validate pointer */
	h = (AllocHeader *)memory_base;
	base_offset = (size_t)memory_base + sizeof(AllocHeader) +
		      bitmap_pages * PAGE_SIZE;
	total_size = get_memory_bytes();
	if ((size_t)ptr < base_offset ||
	    (size_t)ptr >= base_offset + total_size) {
		err = EINVAL;
		return NULL;
	}

	offset = (size_t)ptr - base_offset;

	if (offset % CHUNK_SIZE == 0) {
		/* Chunk-sized allocation (> MAX_SLAB_SIZE) */
		if (new_size > MAX_SLAB_SIZE) {
			/* New size still requires a chunk, no need to resize */
			return ptr;
		}
		/* Resize to a slab */
		new_ptr = allocate_slab(new_size);
		if (new_ptr == NULL) {
			return NULL;
		}
		/* Copy data (up to new_size, as chunk is larger) */
		memcpy(new_ptr, ptr, new_size);

		RELEASE_BIT((void *)((size_t)memory_base + sizeof(AllocHeader)),
			    offset / CHUNK_SIZE, &h->data.last_free);
#if MEMSAN == 1
		__sub64(allocated_bytes, CHUNK_SIZE);
#endif /* MEMSAN */
		return new_ptr;
	} else {
		/* Slab allocation (<= MAX_SLAB_SIZE) */
		Chunk *chunk = (Chunk *)(CHUNK_OFFSET +
					 (offset / CHUNK_SIZE) * CHUNK_SIZE);
		void *base = (void *)((size_t)chunk + sizeof(Chunk));
		uint64_t old_slab_size = chunk->slab_size;
		uint64_t bitmap_size = BITMAP_SIZE(old_slab_size);
		uint64_t index = ((size_t)ptr - ((size_t)base + bitmap_size)) /
				 old_slab_size;
		uint64_t new_slab_size = calculate_slab_size(new_size);

		if (old_slab_size == new_slab_size) {
			/* Same slab size, keep in-place */
			return ptr;
		}

		/* Allocate new slab or chunk */
		new_ptr = alloc(new_size);
		if (new_ptr == NULL) {
			return NULL;
		}

		/* Copy data (up to smaller of old/new size) */
		old_size = old_slab_size;
		if (new_size < old_size) {
			old_size = new_size;
		}
		memcpy(new_ptr, ptr, old_size);

		/* Release old slab */
		RELEASE_BIT(base, index, &chunk->last_free);
#if MEMSAN == 1
		__sub64(allocated_bytes, old_slab_size);
#endif /* MEMSAN */

		return new_ptr;
	}
}

