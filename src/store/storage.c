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
#include <channel.H>
#include <error.H>
#include <format.H>
#include <limits.H>
#include <storage.H>
#include <sys.H>

#define MIN_SIZE (NODE_SIZE * 8)
#define BUFFER1(env) ((BufferNode *)env->base)
#define BUFFER2(env) ((BufferNode *)(((u8 *)env->base + NODE_SIZE)))
#define MAX_WAKEUPS 128

struct Env {
	u8 *base;
	u64 capacity;
	u64 last_freed_word;
	i32 fd;
	u64 counter_pre;
	u64 counter;
	Channel channel;
};

typedef struct {
	u64 counter;
	u64 root;
} BufferNode;

STATIC void ensure_init(Env *env) {
	BufferNode *b1 = BUFFER1(env);
	u64 expected = 0;
	__cas64(&b1->counter, &expected, 2);
}

STATIC void proc_sync(Env *ret, i32 fd) {
	while (true) {
		i32 wakeup_fds[MAX_WAKEUPS];
		u16 wakeup_itt = 0, i;

		recv(&ret->channel, &wakeup_fds[wakeup_itt++]);
		if (wakeup_fds[0] == 0) break;
		while (recv_now(&ret->channel, &wakeup_fds[wakeup_itt]) >= 0) {
			if (wakeup_fds[wakeup_itt]) return;
			wakeup_itt++;
			if (wakeup_itt >= MAX_WAKEUPS) {
				printf("WARN: too many wakeups!\n");
				break;
			}
		}
		__add64(&ret->counter_pre, 1);
		if (fdatasync(fd) < 0) break;
		__add64(&ret->counter, 1);
		for (i = 0; i < wakeup_itt; i++) {
			do {
				i64 ret = write(wakeup_fds[i], "1", 1);
				if (ret < 0 || ret == 1) break;
			} while (true);
		}
	}
}

Env *env_open(const u8 *path) {
	Env *ret;
	i32 fd = file(path), pid;
	u64 capacity;

	if (fd < 0) return NULL;

	if ((capacity = fsize(fd)) < MIN_SIZE) {
		err = EINVAL;
		close(fd);
		return NULL;
	}

	ret = alloc(sizeof(Env));
	if (!ret) {
		close(fd);
		return NULL;
	}

	ret->base = fmap(fd, capacity, 0);
	if (!ret->base) {
		release(ret);
		close(fd);
		return NULL;
	}
	ret->counter_pre = ret->counter = 1;
	ret->channel = channel(sizeof(i32));
	pid = two();

	if (pid < 0) {
		release(ret);
		close(fd);
		release(ret->base);
		return NULL;
	}

	if (!pid) {
		proc_sync(ret, fd);
		ASTORE(&ret->counter, 0);
		exit(0);
	}

	ret->capacity = capacity;
	ret->fd = fd;
	ret->last_freed_word = 0;
	ensure_init(ret);

	return ret;
}

i64 env_register_notification(Env *env, int wakeupfd) {
	i64 ret = ALOAD(&env->counter_pre), res;
	if ((res = send(&env->channel, &wakeupfd) < 0)) return res;
	return ret;
}

i64 env_counter(Env *env) { return ALOAD(&env->counter); }

i32 env_close(Env *env) {
	i32 zero = 0;
	if (!env) {
		err = EINVAL;
		return -1;
	}

	if (munmap(env->base, env->capacity) < 0) return -1;
	send(&env->channel, &zero);
	while (ALOAD(&env->counter));
	channel_destroy(&env->channel);
	return close(env->fd);
}

i32 env_set_root(Env *env, u64 root) {
	/* Declare all variables at start */
	u64 num_nodes;
	u64 bitmap_bytes;
	u64 bitmap_pages;

	/* Validate input */
	if (!env || !env->base || env->capacity < 2 * NODE_SIZE) {
		err = EINVAL;
		return -1;
	}

	/* Initial estimate of nodes (excluding header) */
	num_nodes = (env->capacity - 2 * NODE_SIZE) / NODE_SIZE;
	if (num_nodes == 0) {
		err = EINVAL;
		return -1;
	}

	/* Bitmap size based on nodes */
	bitmap_bytes = (num_nodes + 7) / 8;
	bitmap_pages = (bitmap_bytes + NODE_SIZE - 1) / NODE_SIZE;

	/* Adjust num_nodes for bitmap space */
	if (env->capacity < (2 + bitmap_pages) * NODE_SIZE) {
		err = EINVAL;
		return -1;
	}
	num_nodes =
	    (env->capacity - (2 + bitmap_pages) * NODE_SIZE) / NODE_SIZE;

	/* Validate root index */
	if (root < 2 + bitmap_pages || root >= 2 + bitmap_pages + num_nodes) {
		err = EINVAL;
		return -1;
	}

	do {
		i64 diff, b1_counter, b2_counter;
		u64 expected;
		BufferNode *b1 = BUFFER1(env);
		BufferNode *b2 = BUFFER2(env);
		b1_counter = ALOAD(&b1->counter);
		b2_counter = ALOAD(&b2->counter);

		diff = b2_counter - b1_counter;

		if (diff < 0) {
			if (diff == -1) {
				yield();
				expected = b2_counter;
				__cas64(&b2->counter, &expected,
					b2_counter - 1);
				continue;
			}
			if (diff != -2)
				panic(
				    "Double buffer is in a corrupted "
				    "state!");
			expected = b2_counter;
			if (!__cas64(&b2->counter, &expected, b2_counter + 1))
				continue;
			ASTORE(&b2->root, root);
			expected = b2_counter + 1;
			if (__cas64(&b2->counter, &expected, b2_counter + 4))
				break;
		} else {
			if (diff == 1) {
				yield();
				expected = b1_counter;
				__cas64(&b1->counter, &expected,
					b1_counter - 1);
				continue;
			}

			if (diff != 2)
				panic(
				    "Double buffer is in a corrupted "
				    "state!");

			expected = b1_counter;
			if (!__cas64(&b1->counter, &expected, b1_counter + 1))
				continue;
			ASTORE(&b1->root, root);
			expected = b1_counter + 1;
			if (__cas64(&b1->counter, &expected, b1_counter + 4))
				break;
		}
	} while (true);
	return 0;
}

u64 env_root(Env *env) {
	BufferNode *b1 = BUFFER1(env);
	BufferNode *b2 = BUFFER2(env);

	while (true) {
		u64 c1 = ALOAD(&b1->counter);
		u64 c2 = ALOAD(&b2->counter);
		u64 root;

		if (c1 > c2)
			root = ALOAD(&b1->root);
		else
			root = ALOAD(&b2->root);

		if (ALOAD(&b1->counter) == c1 && ALOAD(&b2->counter) == c2)
			return root;
		yield();
	}
}

Node *env_alloc(Env *env) {
	u64 num_nodes;
	u64 bitmap_bytes;
	u64 bitmap_pages;
	u64 bitmap_start;
	u64 bitmap_words;
	u64 *ptr;
	u64 i;
	u64 offset;
	u64 word;
	u64 bit_value;
	u64 index;
	u64 new_word;
	Node *ret;

	ret = NULL;

	/* Validate inputs */
	if (!env || !env->base || env->capacity < 2 * NODE_SIZE) {
		err = EINVAL;
		return NULL;
	}

	/* Compute number of nodes, accounting for header and bitmap */
	num_nodes = env->capacity / NODE_SIZE;
	if (num_nodes < 2) {
		err = EINVAL;
		return NULL;
	}

	bitmap_bytes = (num_nodes + 7) / 8;
	bitmap_pages = (bitmap_bytes + NODE_SIZE - 1) / NODE_SIZE;

	/* Check for overflow and ensure bitmap fits */
	if (num_nodes > (UINT64_MAX - 7) / 8 || bitmap_pages >= num_nodes) {
		err = ENOMEM;
		return NULL;
	}

	/* Adjust num_nodes for bitmap space */
	num_nodes =
	    (env->capacity - (2 + bitmap_pages) * NODE_SIZE) / NODE_SIZE;
	if (num_nodes == 0) {
		err = EINVAL;
		return NULL;
	}

	ptr = (u64 *)env->base;
	bitmap_start = (2 * NODE_SIZE) / 8; /* Start of bitmap in u64 units */
	bitmap_words =
	    (bitmap_bytes + 7) / 8; /* Number of u64 words in bitmap */

	/* Scan bitmap for free node, starting from last_freed_word */
	for (i = 0; i < bitmap_words; i++) {
		offset = bitmap_start + (env->last_freed_word % bitmap_words);
		/* Reset to start if offset exceeds bounds */
		if (offset * 8 >= (2 + bitmap_pages) * NODE_SIZE) {
			env->last_freed_word = 0;
			offset = bitmap_start;
		}

		word = ptr[offset];
		if (word != ~0UL) {
			while (word != ~0UL) { /* Retry on CAS failure */
				bit_value = __builtin_ctzll(~word);
				index = (offset - bitmap_start) * 64 +
					bit_value; /* Node index */
				if (index >= num_nodes) {
					break; /* Skip padding bits */
				}

				new_word = word | (1UL << bit_value);
				if (__cas64(&ptr[offset], &word, new_word)) {
					ret = (Node *)((2 + bitmap_pages +
							index) *
							   NODE_SIZE +
						       (u8 *)env->base);
					return ret;
				}
				word = ptr[offset]; /* Refresh word on CAS
						       failure */
			}
		}
		env->last_freed_word =
		    (env->last_freed_word + 1) % bitmap_words;
	}

	err = ENOMEM; /* No free nodes */
	return NULL;
}

void env_release(Env *env, Node *node) {
	/* Declare all variables at start */
	u64 num_nodes;
	u64 bitmap_bytes;
	u64 bitmap_pages;
	u64 bitmap_start;
	u64 index;
	u64 word_offset;
	u64 bit_position;
	u64 word;
	u64 new_word;
	u64 expected;
	u64 *ptr;

	/* Validate inputs */
	if (!env || !env->base || !node || env->capacity < 2 * NODE_SIZE)
		panic("Invalid state!");

	/* Compute number of nodes */
	num_nodes = (env->capacity - 2 * NODE_SIZE) / NODE_SIZE;
	if (num_nodes == 0) panic("Invalid state!");

	/* Bitmap size */
	bitmap_bytes = (num_nodes + 7) / 8;
	bitmap_pages = (bitmap_bytes + NODE_SIZE - 1) / NODE_SIZE;

	/* Adjust num_nodes for bitmap space */
	if (env->capacity < (2 + bitmap_pages) * NODE_SIZE)
		panic("Invalid state!");

	num_nodes =
	    (env->capacity - (2 + bitmap_pages) * NODE_SIZE) / NODE_SIZE;

	/* Compute node index */
	index =
	    ((u8 *)node - (u8 *)env->base - (2 + bitmap_pages) * NODE_SIZE) /
	    NODE_SIZE;
	if (index >= num_nodes)
		panic("Trying to free memory that was not allocated!");

	/* Locate bitmap bit */
	ptr = (u64 *)env->base;
	bitmap_start = (2 * NODE_SIZE) / 8; /* Start of bitmap in u64 units */
	env->last_freed_word = index / 64;
	word_offset = bitmap_start + env->last_freed_word;
	bit_position = index % 64;

	/* Ensure word_offset is within bitmap */
	if (word_offset * 8 >= (2 + bitmap_pages) * NODE_SIZE)
		panic("Trying to free memory that was not allocated!");

	/* Atomically clear the bit */
	word = ptr[word_offset];
	expected = word | 1UL << bit_position;
	new_word = word & ~(1UL << bit_position);
	if (!__cas64(&ptr[word_offset], &expected, new_word))
		panic("Double free!");

	err = 0; /* Success */
}
