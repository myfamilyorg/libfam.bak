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

#include <channel.h>
#include <error.h>

STATIC int check_channel_size_overflow(size_t element_size, size_t capacity) {
	if (element_size == 0 || capacity == 0)
		return 0;
	else if (element_size > SIZE_MAX / capacity)
		return -1;
	else if ((element_size * capacity) > SIZE_MAX - sizeof(Channel))
		return -1;
	else
		return 0;
}

void channel_cleanup(ChannelImpl *channel) {
	size_t len;
	int i;
	if (channel->data) {
		for (i = 0; i < MAX_WORKERS; i++) {
			if (channel->data->piperecv[i])
				close(channel->data->piperecv[i]);
			if (channel->data->pipesend[i])
				close(channel->data->pipesend[i]);
		}
		len = (channel->data->capacity * channel->data->element_size) +
		      sizeof(ChannelImpl);
		munmap(channel->data, len);
		channel->data = NULL;
	}
}

Channel channel(size_t element_size, size_t capacity) {
	size_t needed;
	int i;
	ChannelImpl ret = {0};

	if (check_channel_size_overflow(element_size, capacity)) {
		err = ENOMEM;
		return ret;
	}

	needed = capacity * element_size + sizeof(Channel);
	ret.data = smap(needed);
	if (ret.data == NULL) return ret;

	for (i = 0; i < MAX_WORKERS; i++) {
		int fds[2];
		if (pipe(fds)) {
			channel_cleanup(&ret);
			return ret;
		}
		ret.data->piperecv[i] = fds[0];
		ret.data->pipesend[i] = fds[1];
	}

	ret.data->waiting_workers = 0;
	ret.data->element_size = element_size;
	ret.data->capacity = capacity;
	ret.data->lock = LOCK_INIT;
	ret.data->head = ret.data->tail = 0;

	return ret;
}

bool channel_ok(Channel *channel) { return channel && channel->data != NULL; }

void channel_recv(Channel *channel, void *dst) {
	while (true) {
		int wait_fd = -1;
		char buf[1];
		{
			LockGuard lg = lock_write(&channel->data->lock);
			if (channel->data->head != channel->data->tail) {
				memcpy(dst,
				       channel->data + sizeof(Channel) +
					   channel->data->head *
					       channel->data->element_size,
				       channel->data->element_size);

				channel->data->head =
				    (channel->data->head + 1) %
				    channel->data->capacity;
				return;
			} else {
				if (channel->data->waiting_workers <
				    MAX_WORKERS) {
					int wait_id =
					    channel->data->waiting_workers++;
					wait_fd =
					    channel->data->piperecv[wait_id];
				}
			}
		}

		if (wait_fd >= 0) {
			ssize_t n;
			if ((n = read(wait_fd, buf, 1)) <= 0) {
				if (n == 0) err = ECANCELED;
				return;
			}
		}
	}
}

int channel_send(Channel *channel, const void *src) {
	uint64_t next;
	int fd = -1;
	{
		LockGuard lg = lock_write(&channel->data->lock);

		next = (channel->data->tail + 1) % channel->data->capacity;
		if (next == channel->data->head) {
			err = ENOSPC;
			return -1;
		}

		memcpy(channel->data + sizeof(Channel) +
			   channel->data->tail * channel->data->element_size,
		       src, channel->data->element_size);

		channel->data->tail = next;
		if (channel->data->waiting_workers > 0) {
			int id = --channel->data->waiting_workers;
			fd = channel->data->pipesend[id];
		}
	}
	if (fd != -1)
		if (write(fd, "x", 1) == -1) return -1;
	return 0;
}
