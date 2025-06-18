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

#ifndef _STORAGE_H
#define _STORAGE_H

#include <types.H>

#ifndef NODE_SIZE
#ifndef PAGE_SIZE
#define PAGE_SIZE (4 * 4096)
#endif /* PAGE_SIZE */
#define NODE_SIZE ((u64)PAGE_SIZE)
#endif /* NODE_SIZE */

typedef struct Env Env;

typedef struct {
	u8 data[NODE_SIZE];
} Node;

Env *env_open(const u8 *path);
i32 env_close(Env *);

u64 env_alloc(Env *);
void env_release(Env *, u64 );
i64 env_register_notification(Env *, i32 wakeupfd);
i64 env_counter(Env *);
i32 env_set_root(Env *, u64 root);
u64 env_root(Env *);
void *env_base(Env *);

#endif /* _STORAGE_H */
