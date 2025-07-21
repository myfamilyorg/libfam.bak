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

#include <libfam/alloc.H>
#include <libfam/error.H>
#include <libfam/misc.H>
#include <libfam/vec.H>

struct Vec {
	u64 capacity;
	u64 bytes;
};

u64 vec_capacity(Vec *v) {
	if (!v) return 0;
	return v->capacity;
}
u64 vec_size(Vec *v) {
	if (!v) return 0;
	return v->bytes;
}
void *vec_data(Vec *v) {
	if (!v) return NULL;
	return (u8 *)v + sizeof(Vec);
}
i32 vec_extend(Vec *v, void *data, u64 len) {
	if (!v || len + v->bytes > v->capacity) {
		err = EFAULT;
		return -1;
	}
	memcpy((u8 *)v + sizeof(Vec) + v->bytes, data, len);
	v->bytes += len;
	return 0;
}

Vec *vec_resize(Vec *v, u64 nsize) {
	Vec *ret = resize(v, nsize + sizeof(Vec));
	if (!ret) return NULL;
	ret->capacity = nsize;
	if (!v)
		ret->bytes = 0;
	else if (ret->bytes > ret->capacity)
		ret->bytes = ret->capacity;
	return ret;
}

i32 vec_set_size(Vec *v, u64 bytes) {
	if (!v || v->capacity < bytes) {
		err = EFAULT;
		return -1;
	}
	v->bytes = bytes;
	return 0;
}

Vec *vec_new(u64 size) { return vec_resize(NULL, size); }

void vec_release(Vec *v) {
	if (v) release(v);
}

i32 vec_truncate(Vec *v, u64 nsize) {
	u64 elems = vec_size(v);
	if (nsize > elems) {
		err = EINVAL;
		return -1;
	}
	if (elems) {
		v->bytes = nsize;
	}
	return 0;
}

