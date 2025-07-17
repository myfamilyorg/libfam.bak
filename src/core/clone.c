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

#include <libfam/format.H>
#include <libfam/init.H>
#include <libfam/sys.H>
#include <libfam/syscall.H>
#include <libfam/syscall_const.H>
#include <libfam/types.H>

i32 two(void) { return two2(true); }

i32 two2(bool share_fds) {
	struct clone_args args = {0};
	i64 ret;
	args.flags = share_fds ? CLONE_FILES : 0;
	args.pidfd = 0;
	args.child_tid = 0;
	args.parent_tid = 0;
	args.exit_signal = SIGCHLD;
	args.stack = 0;
	args.stack_size = 0;
	args.tls = 0;

	ret = clone3(&args, sizeof(args));
	if (ret == 0) begin();
	return (i32)ret;
}
