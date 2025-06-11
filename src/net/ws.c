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
#include <ws.h>

typedef struct {
	Evh evh;
} WsImpl;

typedef struct {
	Connection conn;
} WsConnectionImpl;

#define STATIC_ASSERT(condition, message) \
	typedef char static_assert_##message[(condition) ? 1 : -1]

STATIC_ASSERT(sizeof(WsConnectionImpl) == sizeof(WsConnection),
	      ws_connection_impl_size);
STATIC_ASSERT(sizeof(WsImpl) == sizeof(Ws), ws_impl_size);

int stop_ws(Ws* ws) {
	if (ws == NULL) {
		err = EINVAL;
		return -1;
	}
	return 0;
}
