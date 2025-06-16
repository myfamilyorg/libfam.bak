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
#include <misc.H>
#include <syscall.H>
#include <syscall_const.H>

#define DEFAULT_CAPACITY 1024

struct ChannelInner {
	u64 element_size;
	u64 capacity;
	u64 head;
	u64 tail;
	u32 wait;
	u8 padding[12];
};

#define ELEMENT_AT(channel, i)                              \
	((u8 *)channel->inner + sizeof(ChannelInner) + \
	 i * channel->inner->element_size)

#define NEXT_POS(channel, pos) ((pos + 1) % channel->inner->capacity)

STATIC int notify(Channel *channel, u64 num_messages) {
	u32 exp = 1;
	if (__cas32(&channel->inner->wait, &exp, 0))
		return futex(&channel->inner->wait, FUTEX_WAKE, num_messages,
			     NULL, NULL, 0) >= 0
			   ? 0
			   : -1;
	return 0;
}

void channel_destroy(Channel *channel) {
	if (channel && channel->inner) {
		u64 size =
		    sizeof(ChannelInner) +
		    channel->inner->element_size * channel->inner->capacity;
		munmap(channel->inner, size);
		channel->inner = NULL;
	}
}
Channel channel(u64 element_size) {
	return channel2(element_size, DEFAULT_CAPACITY);
}
Channel channel2(u64 element_size, u64 capacity) {
	Channel ret = {0};
	if (capacity == 0 || element_size == 0) {
		err = EINVAL;
		return ret;
	}
	capacity++;
	ret.inner = smap(sizeof(ChannelInner) + element_size * capacity);
	if (ret.inner == NULL) return ret;
	ret.inner->element_size = element_size;
	ret.inner->capacity = capacity;
	ret.inner->wait = 0;
	ret.inner->head = ret.inner->tail = 0;
	return ret;
}
bool channel_ok(Channel *channel) { return channel && channel->inner != NULL; }

void recv(Channel *channel, void *dst) {
	u64 tail, head, num_messages;

	while (recv_now(channel, dst) == -1) {
		ASTORE(&channel->inner->wait, 1);
		if (recv_now(channel, dst) == 0) break;
		futex(&channel->inner->wait, FUTEX_WAIT, 1, NULL, NULL, 0);
	}

	tail = ALOAD(&channel->inner->tail);
	head = ALOAD(&channel->inner->head);
	if (tail != head) {
		num_messages = (tail >= head)
				   ? (tail - head)
				   : (tail - head + channel->inner->capacity);
		if (num_messages)
			futex(&channel->inner->wait, FUTEX_WAKE, num_messages,
			      NULL, NULL, 0);
	}
}

int recv_now(Channel *channel, void *dst) {
	u64 tail, head;
	do {
		tail = ALOAD(&channel->inner->tail);
		head = ALOAD(&channel->inner->head);
		if (tail == head) {
			err = EAGAIN;
			return -1;
		}
		memcpy(dst, ELEMENT_AT(channel, tail),
		       channel->inner->element_size);
	} while (
	    !__cas64(&channel->inner->tail, &tail, NEXT_POS(channel, tail)));
	return 0;
}

int send(Channel *channel, const void *src) {
	u64 tail, head, num_messages;
	do {
		head = ALOAD(&channel->inner->head);
		tail = ALOAD(&channel->inner->tail);
		if ((head + 1) % channel->inner->capacity == tail) {
			err = EOVERFLOW;
			return -1;
		}
		memcpy(ELEMENT_AT(channel, head), src,
		       channel->inner->element_size);
	} while (
	    !__cas64(&channel->inner->head, &head, NEXT_POS(channel, head)));

	num_messages = (tail >= head)
			   ? (tail - head)
			   : (tail - head + channel->inner->capacity);

	return notify(channel, num_messages);
}

