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

#ifndef _EVENT_H
#define _EVENT_H

#include <types.H>

#define MULTIPLEX_FLAG_NONE 0
#define MULTIPLEX_FLAG_READ 0x1
#define MULTIPLEX_FLAG_ACCEPT (0x1 << 1)
#define MULTIPLEX_FLAG_WRITE (0x1 << 2)

typedef struct {
	u8 opaque[12];
#ifdef __aarch64__
	u8 opaque2[4]; /* to account for non packing */
#endif
} Event;

i32 multiplex(void);
i32 mregister(i32 multiplex, int fd, int flags, void *attach);
i32 mwait(i32 multiplex, Event events[], int max_events, i64 timeout);
i32 event_is_read(Event event);
i32 event_is_write(Event event);
void *event_attachment(Event event);

#endif /* _EVENT_H */
