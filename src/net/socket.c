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

#include <arpa/inet.h>
#include <fcntl.h>
#include <misc.h>
#include <socket.h>
#include <sys.h>
#include <sys/socket.h>

int socket_connect(Socket* s, byte addr[4], uint16_t port) {
	struct sockaddr_in serv_addr = {0};

	*s = socket(AF_INET, SOCK_STREAM, 0);
	if (*s < 0) return -1;

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	memcpy(&serv_addr.sin_addr.s_addr, addr, 4);

	if (connect(*s, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
		close(*s);
		return -1;
	}

	int flags = fcntl(*s, F_GETFL, 0);
	if (flags == -1) {
		close(*s);
		return -1;
	}

	if (fcntl(*s, F_SETFL, flags | O_NONBLOCK) == -1) {
		close(*s);
		return -1;
	}
	return 0;
}

int socket_listen(Socket* s, byte addr[4], uint16_t port, int backlog) {
	int opt = 1, flags;
	struct sockaddr_in address;

	*s = socket(AF_INET, SOCK_STREAM, 0);
	if (*s < 0) return -1;

	if (setsockopt(*s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
		close(*s);
		return -1;
	}

	flags = fcntl(*s, F_GETFL, 0);
	if (flags == -1) {
		close(*s);
		return -1;
	}

	if (fcntl(*s, F_SETFL, flags | O_NONBLOCK)) {
		close(*s);
		return -1;
	}

	address.sin_family = AF_INET;
	memcpy(&address.sin_addr.s_addr, addr, 4);
	address.sin_port = port;

	if (bind(*s, (struct sockaddr*)&address, sizeof(address))) {
		close(*s);
		return -1;
	}

	if (listen(*s, backlog) == -1) {
		close(*s);
		return -1;
	}

	socklen_t addr_len = sizeof(address);
	if (getsockname(*s, (struct sockaddr*)&address, &addr_len) == -1) {
		close(*s);
		return -1;
	}

	port = ntohs(address.sin_port);
	return port;
}

int socket_accept(Socket* s, Socket* accepted) {
	int flags, res;
	int fd = *accepted = accept(*s, NULL, NULL);
	if (fd < 0) return -1;

	flags = fcntl(*accepted, F_GETFL, 0);
	if (flags < 0) {
		close(*accepted);
		return -1;
	}

	res = fcntl(*accepted, F_SETFL, flags | O_NONBLOCK);
	if (res == -1) {
		close(*accepted);
		return -1;
	}

	return 0;
}
