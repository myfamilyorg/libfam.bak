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

static __attribute__((unused)) uint64_t send_count = 0;
static __attribute__((unused)) uint64_t recv_count = 0;

typedef struct ChannelElement {
	struct ChannelElement *next;
	uint64_t micros;
} ChannelElement;

struct ChannelInner {
	uint64_t head_seq;
	uint64_t element_size;
	ChannelElement *retired;
	ChannelElement *head;
	ChannelElement *tail;
	uint32_t futex;
};

STATIC int notify(Channel *channel) {
	channel->inner->futex = 0;
	return futex(&channel->inner->futex, FUTEX_WAKE, 1, NULL, NULL, 0);
}

STATIC void free_element_list(ChannelElement *list) {
	ChannelElement *current = list;
	while (current) {
		ChannelElement *next = current->next;
		release(current);
		current = next;
	}
}

STATIC void check_retired(Channel *channel) {
	ChannelElement *retired;
	if (!channel || !channel->inner) return;

	retired = ALOAD(&channel->inner->retired);
	if (retired && retired->micros != 0) {
		uint64_t now = micros();
		if (now >= retired->micros + MIN_TIMEOUT_AGE) {
			if (__cas64((uint64_t *)&channel->inner->retired,
				    (uint64_t *)&retired, (uint64_t)NULL)) {
				free_element_list(retired);
			}
		}
	}
}

STATIC void retire_head(Channel *channel, ChannelElement *current_head) {
	ChannelElement *old_retired;
	current_head->micros = micros();
	do {
		old_retired = ALOAD(&channel->inner->retired);
		current_head->next = old_retired;
	} while (!__cas64((uint64_t *)&channel->inner->retired,
			  (uint64_t *)&old_retired, (uint64_t)current_head));
}

void channel_destroy(Channel *channel) {
	if (channel && channel->inner) {
		ChannelElement *current = channel->inner->head;
		while (current) {
			ChannelElement *next = current->next;
			release(current);
			current = next;
		}

		current = channel->inner->retired;
		while (current) {
			ChannelElement *next = current->next;
			release(current);
			current = next;
		}
		channel->inner->retired = NULL;

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
	ret.inner->element_size = element_size;
	ret.inner->head_seq = 0;
	ret.inner->retired = NULL;

	return ret;
}

bool channel_ok(Channel *channel) {
	return channel != NULL && channel->inner != NULL;
}

int recv_now(Channel *channel, void *dst) {
	uint64_t initial_seq = ALOAD(&channel->inner->head_seq), final_seq;
	if (++recv_count % 100 == 0) check_retired(channel);

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
					final_seq =
					    ALOAD(&channel->inner->head_seq);

					if (final_seq == initial_seq + 1)
						release(head);
					else
						retire_head(channel, head);
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

	if (++send_count % 100 == 0) check_retired(channel);

	while (true) {
		ChannelElement *tail = ALOAD(&channel->inner->tail);
		ChannelElement *next = ALOAD(&tail->next);
		uint64_t expected_next = (uint64_t)next;
		if (tail == ALOAD(&channel->inner->tail)) {
			if (next == NULL) {
				if (__cas64((uint64_t *)&tail->next,
					    &expected_next, (uint64_t)node)) {
					uint64_t expected_tail;
					ChannelElement *head =
					    ALOAD(&channel->inner->head);
					if (head == tail)
						__add64(
						    &channel->inner->head_seq,
						    1);

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
