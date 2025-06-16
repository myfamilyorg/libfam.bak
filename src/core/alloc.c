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

#include <alloc.H>
#include <atomic.H>
#include <env.H>
#include <error.H>
#include <format.H>
#include <misc.H>

#define SHM_SIZE_DEFAULT (CHUNK_SIZE * 64)

#ifndef CHUNK_SIZE
#define CHUNK_SIZE (0x1 << 22) /* 4mb */
#endif

#ifndef MAX_SLAB_SIZE
#define MAX_SLAB_SIZE (CHUNK_SIZE >> 2) /* 1mb */
#endif

#define NEXT_FREE_BIT(base, max, result, last_free_ptr)                       \
	do {                                                                  \
		u64 max_words;                                                \
		u64 *bitmap;                                                  \
		u64 local_last_free;                                          \
		(result) = (u64) - 1;                                         \
		bitmap = (u64 *)((u8 *)(base));                               \
		max_words = ((max) + 63) >> 6;                                \
		local_last_free = ALOAD(last_free_ptr);                       \
		while (local_last_free < max_words) {                         \
			if (bitmap[local_last_free] != ~0UL) {                \
				u64 word = bitmap[local_last_free];           \
				u64 bit_value = __builtin_ctzll(~word);       \
				u64 index = local_last_free * 64 + bit_value; \
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
		if ((result) == (u64) - 1 &&                                  \
		    local_last_free > ALOAD(last_free_ptr)) {                 \
			u64 expected = ALOAD(last_free_ptr);                  \
			while (expected < local_last_free &&                  \
			       !__cas64(last_free_ptr, &expected,             \
					local_last_free))                     \
				expected = ALOAD(last_free_ptr);              \
		}                                                             \
	} while (0)
#define RELEASE_BIT(base, index, last_free_ptr)                           \
	do {                                                              \
		u64 new_last_free = index / 64;                           \
		u64 expected = ALOAD(last_free_ptr);                      \
		u64 *bitmap = (u64 *)((u8 *)(base));                      \
		u64 *word_ptr = &bitmap[index / 64];                      \
		u64 bit_mask = 1UL << (index % 64);                       \
		while (1) {                                               \
			u64 old_value = *word_ptr, new_value;             \
			if ((old_value & bit_mask) == 0) {                \
				panic("Double free!");                    \
			}                                                 \
			new_value = old_value & ~bit_mask;                \
			if (__cas64(word_ptr, &old_value, new_value)) {   \
				break;                                    \
			}                                                 \
		}                                                         \
		while (expected > new_last_free &&                        \
		       !__cas64(last_free_ptr, &expected, new_last_free)) \
			expected = ALOAD(last_free_ptr);                  \
	} while (0)
#define CHUNK_OFFSET                                     \
	((u8 *)((u64)memory_base + sizeof(AllocHeader) + \
		bitmap_pages * PAGE_SIZE))
#define BITMAP_CAPACITY(slab_size) \
	((8UL * (CHUNK_SIZE - sizeof(Chunk))) / (1 + 8 * (slab_size)))
#define BITMAP_SIZE(slab_size) (((BITMAP_CAPACITY(slab_size) + 127) / 128) * 16)

typedef struct {
	u64 slab_pointers[32];
	u64 last_free;
} AllocHeaderData;

typedef struct {
	AllocHeaderData data;
	u8 padding[PAGE_SIZE - sizeof(AllocHeaderData)];
} AllocHeader;

typedef struct {
	u32 slab_size;
	u64 last_free;
	u64 next;
	u8 padding[12];
} Chunk;

static void *memory_base = NULL;
static u64 bitmap_pages = 0;
static u64 bitmap_bits = 0;

#if MEMSAN == 1
static u64 *allocated_bytes;
static void __attribute__((constructor)) setup_allocated_bytes(void) {
	allocated_bytes = smap(sizeof(u64));
	*allocated_bytes = 0;
}
u64 get_allocated_bytes(void) { return ALOAD(allocated_bytes); }
void reset_allocated_bytes(void) { ASTORE(allocated_bytes, 0); }
#endif /* MEMSAN */

STATIC u64 get_memory_bytes(void) {
	u64 shm_size = SHM_SIZE_DEFAULT;
	u8 *smembytes;
	smembytes = getenv("SHARED_MEMORY_BYTES");
	if (smembytes) {
		u64 bytes = string_to_uint128(smembytes, strlen(smembytes));
		if (bytes < CHUNK_SIZE * 4 || bytes % CHUNK_SIZE != 0) {
			const u8 *msg =
			    "WARN: SHARED_MEMORY_BYTES must be divisible by "
			    "CHUNK_SIZE and greater than or equal CHUNK_SIZE * "
			    "4. Using default.\n";
			write(2, msg, strlen(msg));
		} else {
			shm_size = bytes;
		}
	}
	return shm_size;
}

STATIC void __attribute__((constructor)) init_memory(void) {
	if (memory_base == NULL) {
		u64 bytes = get_memory_bytes();
		u64 bitmap_bytes;
		i32 i;
		AllocHeader *h;

		bitmap_bits = bytes / CHUNK_SIZE;
		bitmap_bytes = (bitmap_bits + 7) / 8;
		bitmap_pages = (bitmap_bytes + PAGE_SIZE - 1) / PAGE_SIZE;
		memory_base = smap(bytes + bitmap_pages * PAGE_SIZE +
				   sizeof(AllocHeader));
		if (memory_base == NULL)
			panic("Could not allocate shared memory.");

		h = (AllocHeader *)memory_base;
		for (i = 0; i < 32; i++) h->data.slab_pointers[i] = (u64)-1;
	}
}

STATIC u64 allocate_chunk(void) {
	AllocHeader *alloc_header = (AllocHeader *)memory_base;
	u64 res;
	NEXT_FREE_BIT((void *)((u64)memory_base + sizeof(AllocHeader)),
		      bitmap_bits, res, &alloc_header->data.last_free);
	if (res == (u64)-1) err = ENOMEM;
	return res;
}

STATIC u64 calculate_slab_size(u64 value) {
	if (value <= 8) return 8;
	return 1UL << (64 - __builtin_clzll(value - 1));
}

STATIC u64 calculate_slab_index(u64 value) {
	u64 n;
	if (value <= 8) return 0;
	n = 64 - __builtin_clzll(value - 1);
	return n - 3;
}

STATIC u64 atomic_load_or_allocate(u64 *ptr, u32 slab_size) {
	u64 cur;
	u64 new_value;
	u64 expected = (u64)-1;
	AllocHeader *alloc_header = (AllocHeader *)memory_base;
	Chunk *chunk;

	do {
		cur = ALOAD(ptr);
		if (cur != (u64)-1) break;

		new_value = allocate_chunk();
		if (new_value == ((u64)-1)) break;
		chunk = (void *)(new_value * CHUNK_SIZE + CHUNK_OFFSET);
		memset(chunk, 0, sizeof(Chunk) + BITMAP_SIZE(slab_size));
		chunk->next = (u64)-1;
		chunk->slab_size = slab_size;
		chunk->last_free = 0;

		if (__cas64(ptr, &expected, new_value)) {
			cur = new_value;
			break;
		}

		RELEASE_BIT((void *)((u64)memory_base + sizeof(AllocHeader)),
			    new_value, &alloc_header->data.last_free);
		expected = (u64)-1;
	} while (true);

	return cur;
}

STATIC void *allocate_slab(u64 size) {
	AllocHeader *header = memory_base;
	i32 index = calculate_slab_index(size);
	void *chunk_bitmap;
	Chunk *chunk;
	u64 slab_size = calculate_slab_size(size);
	u64 max, ret,
	    cur = atomic_load_or_allocate(&header->data.slab_pointers[index],
					  slab_size);
	void *ret_ptr = NULL;
	while (cur != (u64)-1) {
		max = BITMAP_CAPACITY(slab_size);
		chunk = (Chunk *)(CHUNK_OFFSET + cur * CHUNK_SIZE);
		chunk_bitmap = (void *)((u64)chunk + sizeof(Chunk));
		NEXT_FREE_BIT(chunk_bitmap, max, ret, &chunk->last_free);
		if (ret != (u64)-1) {
			u64 bitmap_size = BITMAP_SIZE(slab_size);
			ret_ptr = (u8 *)chunk + sizeof(Chunk) + bitmap_size +
				  (ret * slab_size);
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

void *alloc(u64 size) {
	void *ret;
	if (size > CHUNK_SIZE) {
		err = EINVAL;
		return NULL;
	} else if (size > MAX_SLAB_SIZE) {
		u64 chunk = allocate_chunk();
		if (chunk == (u64)-1) return NULL;
#if MEMSAN == 1
		__add64(allocated_bytes, CHUNK_SIZE);
#endif /* MEMSAN */

		ret = (void *)((u64)memory_base + sizeof(AllocHeader) +
			       bitmap_pages * PAGE_SIZE + CHUNK_SIZE * chunk);
	} else {
		ret = allocate_slab(size);
	}

	return ret;
}

void release(void *ptr) {
	AllocHeader *alloc_header = memory_base;
	u64 offset = (u64)ptr - ((u64)memory_base + sizeof(AllocHeader) +
				 bitmap_pages * PAGE_SIZE);
	if (ptr == NULL ||
	    (u64)ptr < (u64)memory_base + sizeof(AllocHeader) +
			   bitmap_pages * PAGE_SIZE ||
	    (u64)ptr >= (u64)memory_base + sizeof(AllocHeader) +
			    bitmap_pages * PAGE_SIZE + get_memory_bytes()) {
		err = EINVAL;
		return;
	}

	if (offset % CHUNK_SIZE == 0) {
		RELEASE_BIT((void *)((u64)memory_base + sizeof(AllocHeader)),
			    offset / CHUNK_SIZE, &alloc_header->data.last_free);
#if MEMSAN == 1
		__sub64(allocated_bytes, CHUNK_SIZE);
#endif /* MEMSAN */
	} else {
		Chunk *chunk = (Chunk *)(CHUNK_OFFSET +
					 (offset / CHUNK_SIZE) * CHUNK_SIZE);
		void *base = (void *)((u64)chunk + sizeof(Chunk));
		u64 slab_size = chunk->slab_size;
		u64 bitmap_size = BITMAP_SIZE(slab_size);
		u64 index = ((u64)ptr - ((u64)base + bitmap_size)) / slab_size;
		RELEASE_BIT(base, index, &chunk->last_free);
#if MEMSAN == 1
		__sub64(allocated_bytes, slab_size);
#endif /* MEMSAN */
	}
}

void *resize(void *ptr, u64 new_size) {
	u64 base_offset;
	u64 total_size;
	u64 offset;
	AllocHeader *h;
	void *new_ptr;
	u64 old_size;

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
	base_offset =
	    (u64)memory_base + sizeof(AllocHeader) + bitmap_pages * PAGE_SIZE;
	total_size = get_memory_bytes();
	if ((u64)ptr < base_offset || (u64)ptr >= base_offset + total_size) {
		err = EINVAL;
		return NULL;
	}

	offset = (u64)ptr - base_offset;

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

		RELEASE_BIT((void *)((u64)memory_base + sizeof(AllocHeader)),
			    offset / CHUNK_SIZE, &h->data.last_free);
#if MEMSAN == 1
		__sub64(allocated_bytes, CHUNK_SIZE);
#endif /* MEMSAN */
		return new_ptr;
	} else {
		/* Slab allocation (<= MAX_SLAB_SIZE) */
		Chunk *chunk = (Chunk *)(CHUNK_OFFSET +
					 (offset / CHUNK_SIZE) * CHUNK_SIZE);
		void *base = (void *)((u64)chunk + sizeof(Chunk));
		u64 old_slab_size = chunk->slab_size;
		u64 bitmap_size = BITMAP_SIZE(old_slab_size);
		u64 index =
		    ((u64)ptr - ((u64)base + bitmap_size)) / old_slab_size;
		u64 new_slab_size = calculate_slab_size(new_size);

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

