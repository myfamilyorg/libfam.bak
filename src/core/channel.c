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
#include <channel.h>
#include <error.h>
#include <misc.h>
#include <syscall.h>
#include <syscall_const.h>

#define MIN_TIMEOUT_AGE (1000 * 1000 * 60) /* 1 minute in micros */

int printf(const char *, ...);

static __attribute__((unused)) uint64_t send_count = 0;
static __attribute__((unused)) uint64_t recv_count = 0;

typedef struct ChannelElement {
	struct ChannelElement *next;
	uint64_t micros;
} ChannelElement;

struct ChannelInner {
	uint64_t element_size;
	ChannelElement *head;
	ChannelElement *tail;
	uint32_t futex;
};

STATIC int notify(Channel *channel) {
	channel->inner->futex = 0;
	return futex(&channel->inner->futex, FUTEX_WAKE, 1, NULL, NULL, 0);
}

void channel_destroy(Channel *channel) {
	if (channel && channel->inner) {
		ChannelElement *current;

		current = ALOAD(&channel->inner->head);
		while (current) {
			ChannelElement *next = ALOAD(&current->next);
			release(current);
			current = next;
		}

		release(channel->inner);
		channel->inner = NULL;
	}
}

Channel channel(size_t element_size) {
	Channel ret = {0};

	ret.inner = alloc(sizeof(ChannelInner));
	if (ret.inner == NULL) return ret;
	ret.inner->tail = ret.inner->head = alloc(element_size);
	if (!ret.inner->tail) {
		release(ret.inner);
		ret.inner = NULL;
		return ret;
	}
	ret.inner->tail->next = NULL;
	ret.inner->head->next = NULL;
	ret.inner->element_size = element_size;

	return ret;
}

bool channel_ok(Channel *channel) {
	return channel != NULL && channel->inner != NULL;
}

int recv_now(Channel *channel, void *dst) {
	while (true) {
		uint64_t expected_head, expected_tail;
		ChannelElement *head = ALOAD(&channel->inner->head);
		ChannelElement *tail = ALOAD(&channel->inner->tail);
		ChannelElement *next = ALOAD(&head->next);
		if (head == ALOAD(&channel->inner->head)) {
			if (head == tail) {
				if (next == NULL) {
					return -1;
				}
				expected_tail = (uint64_t)tail;
				__cas64((uint64_t *)&channel->inner->tail,
					&expected_tail, (uint64_t)next);
			} else {
				memcpy(dst,
				       (uint8_t *)next + sizeof(ChannelElement),
				       channel->inner->element_size);
				expected_head = (uint64_t)head;
				if (__cas64((uint64_t *)&channel->inner->head,
					    &expected_head, (uint64_t)next)) {
					release(head);
					return 0;
				}
			}
		}
	}
}

void recv(Channel *channel, void *dst) {
	while (recv_now(channel, dst) == -1) {
		channel->inner->futex = 1;
		futex(&channel->inner->futex, FUTEX_WAIT, 1, NULL, NULL, 0);
	}
}

int send(Channel *channel, const void *source) {
	ChannelElement *node =
	    alloc(channel->inner->element_size + sizeof(ChannelElement));
	if (!node) return -1;
	memcpy((uint8_t *)node + sizeof(ChannelElement), source,
	       channel->inner->element_size);
	node->next = NULL;

	/*
	if (++send_count % 100 == 0) {
	}
	*/

	while (true) {
		ChannelElement *tail = ALOAD(&channel->inner->tail);
		ChannelElement *next = ALOAD(&tail->next);
		uint64_t expected_next = (uint64_t)next;
		if (tail == ALOAD(&channel->inner->tail)) {
			if (next == NULL) {
				if (__cas64((uint64_t *)&tail->next,
					    &expected_next, (uint64_t)node)) {
					uint64_t expected_tail;

					expected_tail = (uint64_t)tail;
					__cas64(
					    (uint64_t *)&channel->inner->tail,
					    &expected_tail, (uint64_t)node);
					return notify(channel);
				}
			} else {
				uint64_t expected_tail = (uint64_t)tail;
				__cas64((uint64_t *)&channel->inner->tail,
					&expected_tail, (uint64_t)next);
			}
		}
	}
}
