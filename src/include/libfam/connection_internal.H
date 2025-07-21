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

#ifndef _CONNECTION_INTERNAL_H
#define _CONNECTION_INTERNAL_H

#include <libfam/connection.H>

/* Internal Only functions */
Connection *connection_accepted(i32 fd, i32 mplex,
				u32 connection_alloc_overhead);
void connection_set_is_connected(Connection *conn);
i64 connection_alloc_overhead(Connection *conn);
i32 connection_set_mplex(Connection *conn, i32 mplex);
i32 connection_write_complete(Connection *connection);

#if TEST == 1
extern bool _debug_force_write_buffer;
extern bool _debug_force_write_error;
extern i32 _debug_write_error_code;
extern u64 _debug_connection_wmax;
#endif /* TEST */

#endif /* _CONNECTION_INTERNAL_H */
