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

#define DEFAULT_MAX_RECV 8

typedef struct ChannelElement {
	struct ChannelElement *next;
} ChannelElement;

struct ChannelInner {
	uint128_t head_state;
	uint64_t element_size;
	uint32_t max_recv;
	ChannelElement *head;
	ChannelElement *tail;
};

void channel_cleanup(ChannelImpl *channel) {}

Channel channel(size_t element_size) {
	return channel2(element_size, DEFAULT_MAX_RECV);
}

Channel channel2(size_t element_size, uint32_t max_receivers) {
	int64_t i, j;
	ChannelImpl ret = {0};

	ret.inner =
	    alloc(sizeof(ChannelInner) + sizeof(int) * max_receivers * 2);
	if (ret.inner == NULL) return ret;
	ret.inner->tail = ret.inner->head = NULL;
	ret.inner->element_size = element_size;
	ret.inner->max_recv = max_receivers;
	ret.inner->head_state = 0;

	for (i = 0; i < (int)max_receivers; i++) {
		int fds[2];
		if (pipe(fds)) {
			ChannelImpl ret_null = {0};
			release(ret.inner);
			for (j = i - 1; j >= 0; j--) {
				close(*((int32_t *)ret.inner +
					sizeof(ChannelInner) +
					sizeof(int) * 2 * j));
				close(*((int32_t *)ret.inner +
					sizeof(ChannelInner) +
					sizeof(int) * (2 * j + 1)));
			}

			return ret_null;
		}
		*((int32_t *)ret.inner + sizeof(ChannelInner) +
		  sizeof(int) * 2 * i) = fds[0];
		*((int32_t *)ret.inner + sizeof(ChannelInner) +
		  sizeof(int) * (2 * i + 1)) = fds[1];
	}
	return ret;
}

bool channel_ok(Channel *channel) {
	return channel != NULL && channel->inner != NULL;
}
int recv_now(Channel *channel, void *dst) {
	size_t size;
	struct ChannelElement *current_head;
	struct ChannelElement *next;
	struct ChannelElement *expected;
	int success;

	size = channel->inner->element_size;

	current_head = ALOAD(&channel->inner->head);

	if (current_head == NULL) {
		return -1;
	}

	next = ALOAD(&current_head->next);

	while (true) {
		expected = current_head;
		success = CAS(&channel->inner->head, &expected, next);
		if (success) {
			if (next == NULL) {
				ASTORE(&channel->inner->tail, NULL);
			}
			memcpy(dst,
			       (char *)current_head +
				   sizeof(struct ChannelElement),
			       size);
			release(current_head);
			return 0;
		}

		current_head = ALOAD(&channel->inner->head);
		if (current_head == NULL) {
			return -1;
		}
		next = ALOAD(&current_head->next);
	}
}

void recv(Channel *channel, void *dst);

/*
int send(Channel *channel, const void *src) {
	size_t size = channel->inner->element_size;
	if (channel->inner->tail == NULL) {
		channel->inner->tail = channel->inner->head =
		    alloc(size + sizeof(ChannelElement));
		if (!channel->inner->head) return -1;
		channel->inner->head->next = NULL;
		memcpy(channel->inner->head + sizeof(ChannelElement), src,
		       size);
	} else {
		ChannelElement *elem = alloc(size + sizeof(ChannelElement));
		if (!elem) return -1;
		channel->inner->tail->next = elem;
		channel->inner->tail = elem;
		channel->inner->tail->next = NULL;
		memcpy(channel->inner->tail + sizeof(ChannelElement), src,
		       size);
	}

	return 0;
}
*/

int send(Channel *channel, const void *src) {
	size_t size = channel->inner->element_size;
	size_t alloc_size = size + sizeof(ChannelElement);
	ChannelElement *current_tail;

	ChannelElement *elem = alloc(alloc_size);
	if (!elem) return -1;
	elem->next = NULL;
	memcpy((char *)elem + sizeof(ChannelElement), src, size);

	current_tail = ALOAD(&channel->inner->tail);

	if (current_tail == NULL) {
		ChannelElement *expected = NULL;
		if (CAS(&channel->inner->tail, &expected, elem)) {
			ASTORE(&channel->inner->head, elem);
			return 0;
		}
		current_tail = ALOAD(&channel->inner->tail);
	}

	while (true) {
		ChannelElement *last = current_tail;
		ChannelElement *expected_next = NULL;
		while (last->next != NULL) {
			last = ALOAD(&last->next);
		}

		if (CAS(&last->next, &expected_next, elem)) {
			ChannelElement *expected_tail = last;
			CAS(&channel->inner->tail, &expected_tail, elem);
			return 0;
		}

		current_tail = ALOAD(&channel->inner->tail);
	}
}

