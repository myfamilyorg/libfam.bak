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

#include <alloc.h>
#include <misc.h>
#include <object.h>

#define BOX_MASK 0x0
#define UINT_MASK 0x1
#define INT_MASK 0x2
#define FLOAT_MASK 0x3
#define BOOL_MASK 0x4
#define ERR_MASK 0x5

#define UNMASK 0x7

typedef struct {
	ObjectDescriptor *descriptor;
	union {
		uint64_t uint_value;
		int64_t int_value;
		double float_value;
		bool bool_value;
		int err;
		void *data;
	} value;
} ObjectData;

void object_cleanup(const Object *obj) {
	if (obj) {
		ObjectType t = object_type(obj);
		if (t == ObjectTypeBox) {
			void *type_data = NULL;
			ObjectData *data = (ObjectData *)(uint64_t)(obj);
			if (data->value.data) {
				ObjectImpl *obj_mut = (Object *)obj;
				data->descriptor->drop(obj_mut);
				release(type_data);
				data->value.data = NULL;
			}
		}
	}
}

ObjectType object_type(const Object *obj) {
	ObjectData *objdata = (ObjectData *)obj;
	uint64_t tag = (uint64_t)objdata->descriptor & UNMASK;
	switch (tag) {
		case BOOL_MASK:
			return ObjectTypeBool;
		case UINT_MASK:
			return ObjectTypeUint;
		case INT_MASK:
			return ObjectTypeInt;
		case FLOAT_MASK:
			return ObjectTypeFloat;
		case ERR_MASK:
			return ObjectTypeErr;
		default:
			return ObjectTypeBox;
	}
}

Object object_create_uint(uint64_t value) {
	union {
		ObjectData data;
		Object impl;
	} u;
	u.data.descriptor = (void *)UINT_MASK;
	u.data.value.uint_value = value;
	return u.impl;
}

Object object_create_int(int64_t value) {
	union {
		ObjectData data;
		Object impl;
	} u;
	u.data.descriptor = (void *)INT_MASK;
	u.data.value.int_value = value;
	return u.impl;
}
Object object_create_float(double value) {
	union {
		ObjectData data;
		Object impl;
	} u;
	u.data.descriptor = (void *)FLOAT_MASK;
	u.data.value.float_value = value;
	return u.impl;
}
Object object_create_bool(bool value) {
	union {
		ObjectData data;
		Object impl;
	} u;
	u.data.descriptor = (void *)BOOL_MASK;
	u.data.value.bool_value = value;
	return u.impl;
}
Object object_create_err(int value) {
	union {
		ObjectData data;
		Object impl;
	} u;
	u.data.descriptor = (void *)ERR_MASK;
	u.data.value.err = value;
	return u.impl;
}
Object object_create_boxed(ObjectDescriptor *descriptor, void *data) {
	union {
		ObjectData data;
		Object impl;
	} u;
	u.data.descriptor = descriptor;
	u.data.value.data = data;
	return u.impl;
}

void object_set_vtable(const Object *obj, void *table) {
	ObjectData *data = NULL;
	uint64_t tag;
	if (!obj) return;
	data = (ObjectData *)(uint64_t)(obj);
	tag = (uint64_t)data->descriptor->table & UNMASK;
	data->descriptor->table = (void *)((uint64_t)table | tag);
}

void *object_get_vtable(const Object *obj) {
	ObjectData *data = NULL;
	ObjectDescriptor *descriptor = NULL;

	if (!obj) return NULL;
	data = (ObjectData *)(uint64_t)(obj);
	descriptor = (ObjectDescriptor *)((uint64_t)data->descriptor & ~UNMASK);
	return descriptor->table;
}

void *object_get_data(const Object *obj) {
	ObjectData *data = NULL;
	if (!obj) return NULL;
	data = (ObjectData *)(uint64_t)(obj);
	return data->value.data;
}

void *object_resize_data(const Object *obj, size_t nsize) {
	ObjectData *data = (ObjectData *)(uint64_t)(obj);
	void *tmp = resize(data->value.data, nsize);
	if (tmp) {
		data->value.data = tmp;
	}
	return tmp;
}

int64_t object_int_value(const Object *obj) {
	ObjectData *data = (ObjectData *)(uint64_t)(obj);
	return data->value.int_value;
}
uint64_t object_uint_value(const Object *obj) {
	ObjectData *data = (ObjectData *)(uint64_t)(obj);
	return data->value.uint_value;
}
double object_double_value(const Object *obj) {
	ObjectData *data = (ObjectData *)(uint64_t)(obj);
	return data->value.float_value;
}
bool object_bool_value(const Object *obj) {
	ObjectData *data = (ObjectData *)(uint64_t)(obj);
	return data->value.bool_value;
}
int object_err_value(const Object *obj) {
	ObjectData *data = (ObjectData *)(uint64_t)(obj);
	return data->value.err;
}

ObjectImpl object_call_build(BuildFn build, ...) {
	ObjectImpl ret;
	__builtin_va_list args;
	__builtin_va_start(args, build);
	ret = build(args);
	__builtin_va_end(args);
	return ret;
}

