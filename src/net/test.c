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
#include <event.H>
#include <test.H>

Test(event) {
	Event events[10];
	int val = 101;
	int fds[2];
	int mplex = multiplex();
	int x;
	ASSERT(mplex > 0, "mplex");
	pipe(fds);
	ASSERT_EQ(mregister(mplex, fds[0], MULTIPLEX_FLAG_READ, &val), 0,
		  "mreg");
	ASSERT_EQ(write(fds[1], "abc", 3), 3, "write");
	err = 0;
	x = mwait(mplex, events, 10, 10);
	ASSERT_EQ(x, 1, "mwait");

	close(mplex);
	close(fds[0]);
	close(fds[1]);
}
