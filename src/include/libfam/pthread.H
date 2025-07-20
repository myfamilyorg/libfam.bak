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

#ifndef _PTHREAD_H
#define _PTHREAD_H

#include <libfam/format.H>
#include <libfam/lock.H>

#define PTHREAD_PROCESS_SHARED 0 /* Not used */

typedef u32 pthread_key_t;
typedef i32 pthread_mutexattr_t;
typedef u32 pthread_mutex_t;
typedef u32 pthread_t;
typedef u32 pthread_cond_t;

void *pthread_getspecific(pthread_key_t key);
int pthread_key_delete(pthread_key_t key);
int pthread_key_create(pthread_key_t *key, void (*destructor)(void *));
int pthread_setspecific(pthread_key_t key, const void *value);
int pthread_mutexattr_destroy(pthread_mutexattr_t *attr);
int pthread_mutexattr_setpshared(pthread_mutexattr_t *attr, int pshared);
int pthread_mutexattr_init(pthread_mutexattr_t *attr);
int pthread_mutex_unlock(pthread_mutex_t *mutex);
int pthread_mutex_lock(pthread_mutex_t *mutex);
int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr);
LockGuard pthread_mutex_lock_guard(pthread_mutex_t *mutex);
pthread_t pthread_self(void);

#endif /* _PTHREAD_H */
