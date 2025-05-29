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
#include <misc.h>
#include <sys.h>

STATIC int channel_get_name(char *buf, const char *name) {
	if (name == NULL || strlen(name) > 128) {
		err = EINVAL;
		return -1;
	}
	memset(buf, 0, 256);
#ifdef __linux__
	strcpy(buf, "/dev/shm/");
#elif defined(__APPLE__)
	strcpy(buf, "/tmp/");
#endif
	strcat(buf, name);
	return 0;
}

STATIC int check_channel_size_overflow(size_t element_size, size_t capacity) {
	size_t product;
	if (element_size == 0 || capacity == 0) {
		product = 0;
	} else if (element_size > SIZE_MAX / capacity) {
		return -1;
	} else {
		product = element_size * capacity;
	}

	if (product > SIZE_MAX - sizeof(Channel)) {
		return -1;
	}

	return 0;
}

int channel_create(const char *name, size_t element_size, size_t capacity) {
	char buf[256];
	int fd;
	size_t needed;

	if (capacity == 0 || element_size == 0 || name == NULL) {
		err = EINVAL;
		return -1;
	}

	/* Circular buffer needs to reserve one extra slot for empty deteection
	 */
	++capacity;

	if (capacity == 0 ||
	    check_channel_size_overflow(element_size, capacity)) {
		err = ENOMEM;
		return -1;
	}

	if (channel_get_name(buf, name)) return -1;

	if (exists(buf)) {
		err = EEXIST;
		return -1;
	}

	fd = file(buf);
	if (fd == -1) return -1;
	needed = capacity * element_size + sizeof(Channel);

	if (fresize(fd, needed) == -1) {
		close(fd);
		return -1;
	} else {
		Channel *channel = fmap(fd);
		if (channel == NULL) {
			close(fd);
			unlink(buf);
			return -1;
		}
		channel->element_size = element_size;
		channel->capacity = capacity;
		channel->lock = LOCK_INIT;
		channel->head = channel->tail = 0;
		munmap(channel, needed);
		close(fd);

		return 0;
	}
}

Channel *channel_open(const char *name) {
	Channel *ret;
	char buf[256];
	int fd;

	if (channel_get_name(buf, name)) return NULL;

	fd = file(buf);
	if (fd == -1) return NULL;
	ret = fmap(fd);
	close(fd);
	return ret;
}

void channel_unmap(Channel *channel) {
	munmap(channel,
	       channel->capacity * channel->element_size + sizeof(Channel));
}

int channel_unlink(const char *name) {
	char buf[256];
	if (channel_get_name(buf, name)) return -1;
	return unlink(buf);
}

int channel_send(Channel *channel, const void *data) {
	uint64_t next;
	LockGuard lg = lock_write(&channel->lock);

	next = (channel->tail + 1) % channel->capacity;
	if (next == channel->head) {
		err = ENOSPC;
		return -1;
	}

	memcpy(
	    channel + sizeof(Channel) + channel->tail * channel->element_size,
	    data, channel->element_size);

	channel->tail = next;
	return 0;
}
int channel_recv(Channel *channel, void *data) {
	LockGuard lg = lock_write(&channel->lock);
	if (channel->head == channel->tail) {
		err = ENOENT;
		return -1;
	}

	memcpy(
	    data,
	    channel + sizeof(Channel) + channel->head * channel->element_size,
	    channel->element_size);

	channel->head = (channel->head + 1) % channel->capacity;

	return 0;
}
