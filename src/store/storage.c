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
	u64 num_nodes;
	u64 bitmap_pages;
	u64 bitmap_bytes;
	u64 last_freed_word;
	i32 fd;
	u64 seqno;
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
	u64 capacity, num_nodes, bitmap_bytes, bitmap_pages;

	if (fd < 0) return NULL;

	if ((capacity = fsize(fd)) < MIN_SIZE) {
		err = EINVAL;
		close(fd);
		return NULL;
	}

	num_nodes = (capacity - 2 * NODE_SIZE) / NODE_SIZE;
	bitmap_bytes = (num_nodes + 7) / 8;
	bitmap_pages = (bitmap_bytes + NODE_SIZE - 1) / NODE_SIZE;
	num_nodes = (capacity - (2 + bitmap_pages) * NODE_SIZE) / NODE_SIZE;

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
	ret->num_nodes = num_nodes;
	ret->bitmap_bytes = bitmap_bytes;
	ret->bitmap_pages = bitmap_pages;
	ensure_init(ret);

	return ret;
}

i64 env_register_notification(Env *env, i32 wakeupfd) {
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

i32 env_set_root(Env *env, u64 seqno, u64 root) {
	BufferNode *b1, *b2;

	if (!env || root < 2 + env->bitmap_pages ||
	    root >= 2 + env->bitmap_pages + env->num_nodes) {
		err = EINVAL;
		return -1;
	}

	b1 = BUFFER1(env);
	b2 = BUFFER2(env);

	do {
		i64 diff, counter;
		u64 expected;
		BufferNode *target;
		i64 b1_counter = ALOAD(&b1->counter);
		i64 b2_counter = ALOAD(&b2->counter);

		diff = b2_counter - b1_counter;

		if (diff < 0) {
			target = b2;
			counter = b2_counter;
			if (diff == -1) {
				yield();
				expected = counter;
				__cas64(&target->counter, &expected,
					counter - 1);
				continue;
			}
			if (diff != -2)
				panic("Double buffer is in a corrupted state!");
		} else {
			target = b1;
			counter = b1_counter;
			if (diff == 1) {
				yield();
				expected = counter;
				__cas64(&target->counter, &expected,
					counter - 1);
				continue;
			}
			if (diff != 2)
				panic("Double buffer is in a corrupted state!");
		}

		expected = counter;
		if (!__cas64(&target->counter, &expected, counter + 1))
			continue;
		if (ALOAD(&env->seqno) != seqno) return -1;
		ASTORE(&target->root, root);
		expected = counter + 1;
		if (__cas64(&target->counter, &expected, counter + 4)) break;

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

u64 env_alloc(Env *env) {
	u64 bitmap_start, bitmap_words, i, offset, word, bit_value, index,
	    new_word;
	u64 *ptr;

	if (!env || !env->base || env->capacity < 2 * NODE_SIZE) {
		err = EINVAL;
		return 0;
	}

	ptr = (u64 *)env->base;
	bitmap_start = (2 * NODE_SIZE) / 8;
	bitmap_words = (env->bitmap_bytes + 7) / 8;

	for (i = 0; i < bitmap_words; i++) {
		offset = bitmap_start + (env->last_freed_word % bitmap_words);
		if (offset * 8 >= (2 + env->bitmap_pages) * NODE_SIZE) {
			env->last_freed_word = 0;
			offset = bitmap_start;
		}

		word = ptr[offset];
		while (word != ~0UL) {
			bit_value = __builtin_ctzll(~word);
			index = (offset - bitmap_start) * 64 + bit_value;
			if (index >= env->num_nodes) break;

			new_word = word | (1UL << bit_value);
			if (__cas64(&ptr[offset], &word, new_word)) {
				return index + 2 + env->bitmap_pages;
			}
			word = ptr[offset];
		}
	}

	err = ENOMEM;
	return 0;
}

void env_release(Env *env, u64 index) {
	u64 bitmap_start, word_offset, bit_position, word, new_word, expected,
	    *ptr;

	if (index < 2 + env->bitmap_pages)
		panic("Trying to free memory that was not allocated!");

	index -= 2 + env->bitmap_pages;

	if (!env || !env->base) panic("Invalid state!");

	if (index >= env->num_nodes)
		panic("Trying to free memory that was not allocated!");

	ptr = (u64 *)env->base;
	bitmap_start = (2 * NODE_SIZE) / 8;
	env->last_freed_word = index / 64;
	word_offset = bitmap_start + env->last_freed_word;
	bit_position = index % 64;

	if (word_offset * 8 >= (2 + env->bitmap_pages) * NODE_SIZE)
		panic("Trying to free memory that was not allocated!");

	word = ptr[word_offset];
	expected = word | 1UL << bit_position;
	new_word = word & ~(1UL << bit_position);
	if (!__cas64(&ptr[word_offset], &expected, new_word))
		panic("Double free!");
}

void *env_base(Env *env) { return env->base; }
u64 env_root_seqno(Env *env) { return ALOAD(&env->seqno); }
