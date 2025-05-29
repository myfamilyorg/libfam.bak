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

#include <error.h>
#include <evh.h>
#include <stdio.h>
#include <sys.h>

STATIC int evh_proc_wakeup(int mplex, int wakeup) {
	char buf[1024];

	if (read(wakeup, buf, 1024) <= 0)
		return -1;
	else
		return 0;
}

STATIC void evh_loop(int wakeup) {
	Event events[1024];
	int i, count, mplex;

	if ((mplex = multiplex()) == -1) {
		print_error("multiplex");
		exit(0);
	}
	if (mregister(mplex, wakeup, MULTIPLEX_FLAG_READ, &wakeup) == -1) {
		print_error("mregister");
		exit(0);
	}
	while ((count = mwait(mplex, events, 1024, -1)) > 0) {
		for (i = 0; i < count; i++) {
			if (event_attachment(events[i]) == &wakeup) {
				if (evh_proc_wakeup(mplex, wakeup))
					goto breakloop;
			}
		}
	}
	print_error("mwait");
breakloop:
	printf("exit evh_loop\n");
	exit(0);
}

int evh_start(Evh *evh) {
	int fds[2];

	if (pipe(fds) == -1) return -1;
	evh->wakeup = fds[1];
	if (fork() == 0) {
		close(fds[1]);
		evh_loop(fds[0]);
		exit(0);
	} else {
		close(fds[0]);
	}
	return 0;
}

int evh_stop(Evh *evh) {
	printf("evhwakeup %i\n", evh->wakeup);
	return close(evh->wakeup);
}
