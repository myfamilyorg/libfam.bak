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

#include <error.H>
#include <misc.H>
#include <socket.H>
#include <syscall.H>
#include <syscall_const.H>

u16 htons(u16 host) { return ((host & 0xFF) << 8) | ((host >> 8) & 0xFF); }

u16 ntohs(u16 net) { return ((net & 0xFF) << 8) | ((net >> 8) & 0xFF); }

i32 set_nonblocking(i32 socket) {
	i32 flags;
	if ((flags = fcntl(socket, F_GETFL, 0)) == -1) return -1;
	if (fcntl(socket, F_SETFL, flags | O_NONBLOCK) == -1) return -1;
	return 0;
}

i32 socket_connect(int *fd, const u8 addr[4], u16 port) {
	struct sockaddr_in address = {0};

	if (!fd || !addr) {
		err = EINVAL;
		return -1;
	}

	*fd = socket(AF_INET, SOCK_STREAM, 0);
	if (*fd < 0) return -1;

	if (set_nonblocking(*fd) == -1) {
		close(*fd);
		return -1;
	}

	address.sin_family = AF_INET;
	memcpy(&address.sin_addr, addr, 4);
	address.sin_port = htons(port);

	if (connect(*fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
		if (err != EINPROGRESS) close(*fd);
		return -1;
	}

	return 0;
}

i32 socket_listen(i32 *fd, const u8 addr[4], u16 port, u16 backlog) {
	i32 opt = 1;
	struct sockaddr_in address;
	socklen_t addr_len;
	i32 ret = socket(AF_INET, SOCK_STREAM, 0);

	if (ret < 0) return -1;

	if (setsockopt(ret, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
		close(ret);
		return -1;
	}

	if (set_nonblocking(ret) == -1) return -1;

	address.sin_family = AF_INET;
	memcpy(&address.sin_addr, addr, 4);
	address.sin_port = htons(port);

	if (bind(ret, (struct sockaddr *)&address, sizeof(address))) {
		close(ret);
		return -1;
	}

	if (listen(ret, backlog) == -1) {
		close(ret);
		return -1;
	}

	addr_len = sizeof(address);
	if (getsockname(ret, (struct sockaddr *)&address, &addr_len) == -1) {
		close(ret);
		return -1;
	}

	*fd = ret;
	port = ntohs(address.sin_port);
	return port;
}

i32 socket_accept(i32 fd) {
	i32 ret = accept(fd, NULL, NULL);
	if (ret < 0) return -1;
	if (set_nonblocking(ret) == -1) return -1;
	return ret;
}

