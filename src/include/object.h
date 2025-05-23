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

#ifndef _OBJECT_H__
#define _OBJECT_H__

#include <alloc.h>
#include <error.h>
#include <types.h>

typedef enum {
	ObjectTypeUint,
	ObjectTypeInt,
	ObjectTypeFloat,
	ObjectTypeBool,
	ObjectTypeErr,
	ObjectTypeBox
} ObjectType;

typedef uint128_t ObjectImpl;
void object_cleanup(const ObjectImpl *obj);
#define Object ObjectImpl __attribute((cleanup(object_cleanup)))

typedef struct {
	const char *type_name;
	void (*drop)(Object *);
	Object (*build)(__builtin_va_list args);
	void *table;
} ObjectDescriptor;

ObjectType object_type(const Object *obj);
Object object_create_uint(uint64_t value);
Object object_create_int(int64_t value);
Object object_create_float(double value);
Object object_create_bool(bool value);
Object object_create_err(int code);
Object object_create_boxed(ObjectDescriptor *descriptor, void *data);
void object_set_vtable(const Object *obj, void *table);
void *object_get_vtable(const Object *obj);
void *object_get_data(const Object *obj);
void *object_resize_data(const Object *obj, size_t nsize);

int64_t object_int_value(const Object *obj);
uint64_t object_uint_value(const Object *obj);
double object_double_value(const Object *obj);
bool object_bool_value(const Object *obj);
int object_err_value(const Object *obj);

#define let const Object
#define var Object

typedef ObjectImpl (*BuildFn)(__builtin_va_list);
ObjectImpl object_call_build(BuildFn build, ...);

#define $object(type, ...) \
	object_call_build(type##_Descriptor.build, __VA_ARGS__)
/*
#define $object(type, ...)                                                  \
	({                                                                  \
		ObjectImpl _ret__;                                          \
		if (type##_Descriptor.build) {                              \
			_ret__ = object_call_build(type##_Descriptor.build, \
						   __VA_ARGS__);            \
		} else {                                                    \
			typeof(__VA_ARGS__) *_data__ = alloc(sizeof(type)); \
			if (_data__) {                                      \
				*_data__ = (type)(__VA_ARGS__);             \
				_ret__ = object_create_boxed(               \
				    &type##_Descriptor, _data__);           \
			} else {                                            \
				_ret__ = object_create_err(err);            \
			}                                                   \
		}                                                           \
		_ret__;                                                     \
	})
	*/

#define $int(v) object_int_value(&v)
#define $uint(v) object_uint_value(&v)
#define $float(v) object_float_value(&v)
#define $bool(v) object_bool_value(&v)
#define $err(v) object_err_value(&v)

#define $is_int(v) (object_type(&v) == ObjectTypeInt)
#define $is_uint(v) (object_type(&v) == ObjectTypeUint)
#define $is_float(v) (object_type(&v) == ObjectTypeFloat)
#define $is_bool(v) (object_type(&v) == ObjectTypeBool)
#define $is_err(v) (object_type(&v) == ObjectTypeErr)
#define $is_box(v) (object_type(&v) == ObjectTypeBox)

#endif /* _OBJECT_H__ */

