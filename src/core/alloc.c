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

u64 _debug_cas_loop = 0;
u64 _debug_alloc_failure = 0;
u64 _debug_alloc_failure_bypass_count = 0;

#define SHM_SIZE_DEFAULT (CHUNK_SIZE * 64)
#define MAX_SLAB_SIZES 32
#define NEXT_FREE_BIT(base, max, result, last_free_ptr)                        \
	do {                                                                   \
		u64 max_words;                                                 \
		u64 *bitmap;                                                   \
		u64 local_last_free;                                           \
		u64 start_word;                                                \
		bool wrapped = false;                                          \
		(result) = (u64) - 1;                                          \
		bitmap = (u64 *)((u8 *)(base));                                \
		max_words = ((max) + 63) >> 6;                                 \
		local_last_free = ALOAD(last_free_ptr);                        \
		start_word = local_last_free;                                  \
		while (true) {                                                 \
			u64 word_idx = local_last_free;                        \
			if (word_idx >= max_words) {                           \
				if (wrapped) break; /* Full scan completed */  \
				word_idx = 0;	    /* Wrap around to start */ \
				wrapped = true;                                \
			}                                                      \
			if (bitmap[word_idx] != ~0UL) {                        \
				u64 word = bitmap[word_idx];                   \
				u64 bit_value = __builtin_ctzll(~word);        \
				u64 index = word_idx * 64 + bit_value;         \
				if (index < max) {                             \
					if (__cas64(                           \
						&bitmap[word_idx], &word,      \
						word | (1UL << bit_value))) {  \
						(result) = index;              \
						break;                         \
					} else                                 \
						continue; /* Retry if CAS      \
							     fails */          \
				}                                              \
			}                                                      \
			local_last_free = word_idx + 1;                        \
			if (wrapped && local_last_free > start_word) break;    \
		}                                                              \
		if ((result) == (u64) - 1 &&                                   \
		    local_last_free > ALOAD(last_free_ptr)) {                  \
			u64 expected = ALOAD(last_free_ptr);                   \
			while (expected < local_last_free &&                   \
			       !__cas64(last_free_ptr, &expected,              \
					local_last_free))                      \
				expected = ALOAD(last_free_ptr);               \
		}                                                              \
	} while (false)

#define RELEASE_BIT(base, index, last_free_ptr)                           \
	do {                                                              \
		u64 new_last_free = index / 64;                           \
		u64 expected = ALOAD(last_free_ptr);                      \
		u64 *bitmap = (u64 *)((u8 *)(base));                      \
		u64 *word_ptr = &bitmap[index / 64];                      \
		u64 bit_mask = 1UL << (index % 64);                       \
		while (true) {                                            \
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
	} while (false)
#define CHUNK_OFFSET(a) \
	((u8 *)((u64)a + sizeof(Alloc) + a->bitmap_pages * PAGE_SIZE))
#define BITMAP_CAPACITY(slab_size) \
	((8UL * (CHUNK_SIZE - sizeof(Chunk))) / (1 + 8 * (slab_size)))
#define BITMAP_SIZE(slab_size) (((BITMAP_CAPACITY(slab_size) + 127) / 128) * 16)

struct Alloc {
	u64 bitmap_pages;
	u64 bitmap_bits;
	u64 bitmap_bytes;
	u64 size;
	u64 slab_pointers[MAX_SLAB_SIZES];
	u64 last_free;
#if MEMSAN == 1
	u64 allocated_bytes;
#endif /* MEMSAN */
};

typedef struct {
	u32 slab_size;
	u64 last_free;
	u64 next;
	u8 padding[12];
} Chunk;

STATIC Alloc *_alloc_ptr__ = NULL;

STATIC u64 get_memory_bytes(void) {
	u64 shm_size = SHM_SIZE_DEFAULT;
	u8 *smembytes;
	smembytes = getenv("SHARED_MEMORY_BYTES");
	if (smembytes) {
		u64 bytes = string_to_uint128(smembytes, strlen(smembytes));
		if (bytes < CHUNK_SIZE * 4 || bytes % CHUNK_SIZE != 0) {
			const u8 *msg =
			    "WARN: SHARED_MEMORY_BYTES must be "
			    "divisible by "
			    "CHUNK_SIZE and greater than or equal "
			    "CHUNK_SIZE * "
			    "4. Using default.\n";
			write(2, msg, strlen(msg));
		} else {
			shm_size = bytes;
		}
	}
	return shm_size;
}

STATIC __attribute__((constructor)) void __init_alloc(void) {
	u64 size = get_memory_bytes();
	_alloc_ptr__ = alloc_init(ALLOC_TYPE_SMAP, size);
}

STATIC u64 calculate_slab_size_impl(u64 value) {
	if (value <= 8) return 8;
	return 1UL << (64 - __builtin_clzll(value - 1));
}

STATIC u64 calculate_slab_index_impl(u64 value) {
	u64 n;
	if (value <= 8) return 0;
	n = 64 - __builtin_clzll(value - 1);
	return n - 3;
}

STATIC u64 allocate_chunk_impl(Alloc *a) {
	u64 res;
	NEXT_FREE_BIT((void *)((u64)a + sizeof(Alloc)), a->bitmap_bits, res,
		      &a->last_free);
	if (res == (u64)-1) err = ENOMEM;
	return res;
}

STATIC u64 atomic_load_or_allocate_impl(Alloc *a, u64 *ptr, u32 slab_size) {
	u64 cur, new_value, expected = (u64)-1;
	Chunk *chunk;

	do {
		cur = ALOAD(ptr);

		if (cur != (u64)-1) break;

		new_value = allocate_chunk_impl(a);
		if (new_value == ((u64)-1)) break;
		chunk = (void *)(new_value * CHUNK_SIZE + CHUNK_OFFSET(a));
		memset(chunk, 0, sizeof(Chunk) + BITMAP_SIZE(slab_size));
		chunk->next = (u64)-1;
		chunk->slab_size = slab_size;
		chunk->last_free = 0;

		if (__cas64(ptr, &expected, new_value)) {
			cur = new_value;
#if TEST == 1
			if (!ALOAD(&_debug_cas_loop))
#endif /* TEST */
				break;
#if TEST == 1
			__sub64(&_debug_cas_loop, 1);
#endif /* TEST */
		}

		RELEASE_BIT((void *)((u64)a + sizeof(Alloc)), new_value,
			    &a->last_free);
		expected = (u64)-1;
	} while (true);

	return cur;
}

STATIC void *allocate_slab_impl(Alloc *a, u64 size) {
	i32 index = calculate_slab_index_impl(size);
	void *chunk_bitmap;
	Chunk *chunk;
	u64 slab_size = calculate_slab_size_impl(size);
	u64 max, ret,
	    cur = atomic_load_or_allocate_impl(a, &a->slab_pointers[index],
					       slab_size);
	void *ret_ptr = NULL;

	while (cur != (u64)-1) {
		max = BITMAP_CAPACITY(slab_size);
		chunk = (Chunk *)(CHUNK_OFFSET(a) + cur * CHUNK_SIZE);
		chunk_bitmap = (void *)((u64)chunk + sizeof(Chunk));
		NEXT_FREE_BIT(chunk_bitmap, max, ret, &chunk->last_free);
		if (ret != (u64)-1) {
			u64 bitmap_size = BITMAP_SIZE(slab_size);
			ret_ptr = (u8 *)chunk + sizeof(Chunk) + bitmap_size +
				  (ret * slab_size);
			break;
		}
		cur = atomic_load_or_allocate_impl(a, &chunk->next, slab_size);
	}

#if MEMSAN == 1
	if (ret_ptr) {
		__add64(&a->allocated_bytes, slab_size);
	}
#endif /* MEMSAN */

	return ret_ptr;
}

Alloc *alloc_init(AllocType t, u64 size, ...) {
	Alloc *ret = NULL;
	int i;
	u64 bitmap_bits = size / CHUNK_SIZE;
	u64 bitmap_bytes = (bitmap_bits + 7) / 8;
	u64 bitmap_pages = (bitmap_bytes + PAGE_SIZE - 1) / PAGE_SIZE;

	if (t == ALLOC_TYPE_MAP) {
		ret = map(size + sizeof(Alloc) + bitmap_pages * PAGE_SIZE);
	} else if (t == ALLOC_TYPE_SMAP) {
		ret = smap(size + sizeof(Alloc) + bitmap_pages * PAGE_SIZE);
	} else if (t == ALLOC_TYPE_FMAP) {
		i32 fd;
		__builtin_va_list list;
		__builtin_va_start(list, size);
		fd = (i32) __builtin_va_arg(list, i32);
		__builtin_va_end(list);
		ret = fmap(fd, size + sizeof(Alloc) + bitmap_pages * PAGE_SIZE,
			   0);
	} else {
		err = EINVAL;
	}
	if (!ret) return NULL;

	ret->bitmap_bits = bitmap_bits;
	ret->bitmap_bytes = bitmap_bytes;
	ret->bitmap_pages = bitmap_pages;
	ret->size = size;
	for (i = 0; i < MAX_SLAB_SIZES; i++) ret->slab_pointers[i] = -1;
	ret->last_free = 0;
#if MEMSAN == 1
	ret->allocated_bytes = 0;
#endif /* MEMSAN */

	return ret;
}

void alloc_destroy(Alloc *a) {
	munmap(a, a->size + sizeof(Alloc) + a->bitmap_pages * PAGE_SIZE);
}

u64 allocated_bytes_impl(Alloc *a __attribute__((unused))) {
	u64 ret = 0;
#if MEMSAN == 1
	ret = a->allocated_bytes;
#endif /* MEMSAN */
	return ret;
}

void reset_allocated_bytes_impl(Alloc *a __attribute__((unused))) {
#if MEMSAN == 1
	a->allocated_bytes = 0;
#endif /* MEMSAN */
}

void *alloc_impl(Alloc *a, u64 size) {
	void *ret = NULL;

#if TEST == 1
	u64 cur, bypass;
	if ((bypass = ALOAD(&_debug_alloc_failure_bypass_count)) == 0) {
		while ((cur = ALOAD(&_debug_alloc_failure))) {
			u64 exp = cur;
			if (__cas64(&_debug_alloc_failure, &exp, cur - 1))
				return NULL;
		}
	} else {
		u64 exp = bypass;
		__cas64(&_debug_alloc_failure_bypass_count, &exp, bypass - 1);
	}
#endif

	if (size > CHUNK_SIZE) {
		err = EINVAL;
		return NULL;
	} else if (size > MAX_SLAB_SIZE) {
		u64 chunk = allocate_chunk_impl(a);
		if ((i64)chunk < 0) return NULL;
#if MEMSAN == 1
		__add64(&a->allocated_bytes, CHUNK_SIZE);
#endif
		ret =
		    (void *)((u64)a + sizeof(Alloc) +
			     a->bitmap_pages * PAGE_SIZE + CHUNK_SIZE * chunk);
	} else {
		ret = allocate_slab_impl(a, size);
	}

	return ret;
}

void release_impl(Alloc *a, void *ptr) {
	u64 offset;

	if (a == NULL || ptr == NULL ||
	    (u64)ptr < ((u64)a + sizeof(Alloc) + a->bitmap_pages * PAGE_SIZE) ||
	    (u64)ptr >= ((u64)a + a->size + sizeof(Alloc) +
			 a->bitmap_pages * PAGE_SIZE)) {
		panic("Invalid memory release!");
		return;
	}

	offset =
	    (u64)ptr - ((u64)a + sizeof(Alloc) + a->bitmap_pages * PAGE_SIZE);

	if (offset % CHUNK_SIZE == 0) {
		RELEASE_BIT((void *)((u64)a + sizeof(Alloc)),
			    offset / CHUNK_SIZE, &a->last_free);
#if MEMSAN == 1
		__sub64(&a->allocated_bytes, CHUNK_SIZE);
#endif /* MEMSAN */
	} else {
		Chunk *chunk = (Chunk *)(CHUNK_OFFSET(a) +
					 (offset / CHUNK_SIZE) * CHUNK_SIZE);
		void *base = (void *)((u64)chunk + sizeof(Chunk));
		u64 slab_size = chunk->slab_size;
		u64 bitmap_size = BITMAP_SIZE(slab_size);
		u64 index = ((u64)ptr - ((u64)base + bitmap_size)) / slab_size;
		RELEASE_BIT(base, index, &chunk->last_free);
#if MEMSAN == 1
		__sub64(&a->allocated_bytes, slab_size);
#endif
	}
}
void *resize_impl(Alloc *a, void *ptr, u64 new_size) {
	u64 base_offset;
	u64 offset;
	void *new_ptr;
	u64 old_size;

	/* Handle null pointer or zero size */
	if (ptr == NULL) {
		return alloc_impl(a, new_size);
	}
	if (new_size == 0) {
		release_impl(a, ptr);
		return NULL;
	}

	/* Validate new size */
	if (new_size > CHUNK_SIZE) {
		err = EINVAL;
		return NULL;
	}

	/* Validate pointer */
	base_offset = (u64)a + sizeof(Alloc) + a->bitmap_pages * PAGE_SIZE;
	if ((u64)ptr < base_offset || (u64)ptr >= base_offset + a->size) {
		panic("Invalid pointer resized!\n");
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
		new_ptr = allocate_slab_impl(a, new_size);
		if (new_ptr == NULL) {
			return NULL;
		}
		/* Copy data (up to new_size, as chunk is larger) */
		memcpy(new_ptr, ptr, new_size);

		RELEASE_BIT((void *)((u64)a + sizeof(Alloc)),
			    offset / CHUNK_SIZE, &a->last_free);
#if MEMSAN == 1
		__sub64(&a->allocated_bytes, CHUNK_SIZE);
#endif /* MEMSAN */
		return new_ptr;
	} else {
		/* Slab allocation (<= MAX_SLAB_SIZE) */
		Chunk *chunk = (Chunk *)(CHUNK_OFFSET(a) +
					 (offset / CHUNK_SIZE) * CHUNK_SIZE);
		void *base = (void *)((u64)chunk + sizeof(Chunk));
		u64 old_slab_size = chunk->slab_size;
		u64 bitmap_size = BITMAP_SIZE(old_slab_size);
		u64 index =
		    ((u64)ptr - ((u64)base + bitmap_size)) / old_slab_size;
		u64 new_slab_size = calculate_slab_size_impl(new_size);

		if (old_slab_size == new_slab_size) {
			/* Same slab size, keep in-place */
			return ptr;
		}

		/* Allocate new slab or chunk */
		new_ptr = alloc_impl(a, new_size);
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
		__sub64(&a->allocated_bytes, old_slab_size);
#endif /* MEMSAN */
		return new_ptr;
	}
}

void reset_allocated_bytes(void) { reset_allocated_bytes_impl(_alloc_ptr__); }
u64 get_allocated_bytes(void) { return allocated_bytes_impl(_alloc_ptr__); }
void *alloc(u64 size) { return alloc_impl(_alloc_ptr__, size); }
void release(void *ptr) { release_impl(_alloc_ptr__, ptr); }
void *resize(void *ptr, u64 size) {
	return resize_impl(_alloc_ptr__, ptr, size);
}

