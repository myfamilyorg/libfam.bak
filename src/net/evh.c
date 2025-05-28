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

#include <evh.h>
#include <stdio.h>
#include <sys.h>

STATIC void evh_loop(int mplex, int close) {
	printf("evh_loop\n");
	while (true) {
		char buf[1024];
		printf("reading %i\n", close);
		ssize_t len = read(close, buf, 1024);
		if (len <= 0) break;
		printf("len=%lli\n", len);
	}
	printf("exit\n");
	exit(0);
}

int evh_init(Evh *evh) {
	evh->mplex = multiplex();
	if (evh->mplex == -1) return -1;
	return 0;
}

int evh_start(Evh *evh) {
	int fds[2];

	if (pipe(fds) == -1) return -1;
	evh->close = fds[1];
	if (fork() == 0) {
		close(fds[1]);
		evh_loop(evh->mplex, fds[0]);
	} else {
		close(fds[0]);
	}
	return 0;
}

int evh_stop(Evh *evh) {
	printf("evhclose %i\n", evh->close);
	write(evh->close, '1', 1);
	close(evh->close);
}
