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

#include <criterion/criterion.h>
#include <object.h>
#include <stdio.h>
#include <types.h>

Test(core, core1) {
	cr_assert_eq(UINT8_MAX, UINT8_MAX);
	cr_assert_eq(UINT16_MAX, UINT16_MAX);
	cr_assert_eq(UINT32_MAX, UINT32_MAX);
	cr_assert_eq(UINT64_MAX, UINT64_MAX);
	cr_assert_eq(UINT128_MAX, UINT128_MAX);
	cr_assert_eq(INT8_MAX, INT8_MAX);
	cr_assert_eq(INT16_MAX, INT16_MAX);
	cr_assert_eq(INT32_MAX, INT32_MAX);
	cr_assert_eq(INT64_MAX, INT64_MAX);
	cr_assert_eq(INT128_MAX, INT128_MAX);

	cr_assert_eq(UINT8_MIN, UINT8_MIN);
	cr_assert_eq(UINT16_MIN, UINT16_MIN);
	cr_assert_eq(UINT32_MIN, UINT32_MIN);
	cr_assert_eq(UINT64_MIN, UINT64_MIN);
	cr_assert_eq(UINT128_MIN, UINT128_MIN);
	cr_assert_eq(INT8_MIN, INT8_MIN);
	cr_assert_eq(INT16_MIN, INT16_MIN);
	cr_assert_eq(INT32_MIN, INT32_MIN);
	cr_assert_eq(INT64_MIN, INT64_MIN);
	cr_assert_eq(INT128_MIN, INT128_MIN);
	cr_assert_eq(SIZE_MAX, SIZE_MAX);
}

Test(core, object1) {
	let a = object_create_uint(1234);
	let b = object_create_int(-100);
	let c = object_create_float(1.23);
	let d = object_create_bool(false);
	let e = object_create_err(EINVAL);

	cr_assert_eq(object_type(&a), ObjectTypeUint);
	cr_assert_eq(object_type(&b), ObjectTypeInt);
	cr_assert_eq(object_type(&c), ObjectTypeFloat);
	cr_assert_eq(object_type(&d), ObjectTypeBool);
	cr_assert_eq(object_type(&e), ObjectTypeErr);
}

typedef struct {
	void (*speak)(const Object *);
} Speak;

void speak(const Object *obj) {
	void *vtable = object_get_vtable(obj);
	((Speak *)vtable)->speak(obj);
}

typedef struct {
	int32_t a;
	uint64_t b;
} MyStruct;
void MyStruct_drop(Object *obj);

Object MyStruct_build(va_list args);
static ObjectDescriptor MyStruct_Descriptor = {
    .type_name = "MyStruct",
    .build = MyStruct_build,
    .drop = MyStruct_drop,
    .table = NULL,

};

Object MyStruct_build(va_list args) {
	void *data = alloc(sizeof(MyStruct));
	if (data) {
		((MyStruct *)data)->a = va_arg(args, int32_t);
		((MyStruct *)data)->b = va_arg(args, uint64_t);
		return object_create_boxed(&MyStruct_Descriptor, data);
	} else {
		return object_create_err(err);
	}
}
void MyStruct_speak(const Object *obj);
static Speak MyStruct_Speak = {.speak = MyStruct_speak};

static int drop_count = 0;
void MyStruct_drop(Object *obj) { drop_count++; };
static int speak_count = 0;
void MyStruct_speak(const Object *obj) { speak_count++; }

Test(core, obj2) {
	{
		let ms1 = $object(MyStruct, 4, 7);
		object_set_vtable(&ms1, &MyStruct_Speak);
		cr_assert_eq(speak_count, 0);
		speak(&ms1);
		cr_assert_eq(speak_count, 1);
		cr_assert_eq(drop_count, 0);
		object_cleanup(&ms1);
		cr_assert_eq(drop_count, 1);
		object_cleanup(&ms1);
		object_cleanup(&ms1);
		cr_assert_eq(drop_count, 1);
	}
	cr_assert_eq(drop_count, 1);
}

typedef struct {
	Object (*clone)(const Object *obj);
} Clone;

Object Clone_clone(const Object *obj) {
	void *vtable = object_get_vtable(obj);
	return ((Clone *)vtable)->clone(obj);
}

typedef struct {
	Object (*get_x)(const Object *);
	void (*set_x)(Object *, int x);
} TestObjImpl;

Object TestObjImpl_get_x(const Object *obj) {
	void *vtable = object_get_vtable(obj);
	return ((TestObjImpl *)vtable)->get_x(obj);
}

void TestObjImpl_set_x(Object *obj, int x) {
	void *vtable = object_get_vtable(obj);
	((TestObjImpl *)vtable)->set_x(obj, x);
}

typedef struct {
	int x;
	int y;
} TestObj;

Object TestObj_build(va_list args);
void TestObj_drop(Object *obj);
static ObjectDescriptor TestObj_Descriptor = {
    .type_name = "TestObj",
    .drop = TestObj_drop,
    .build = TestObj_build,
    .table = NULL,

};

Object TestObj_build(va_list args) {
	void *data = alloc(sizeof(TestObj));
	if (data) {
		((TestObj *)data)->x = va_arg(args, int);
		((TestObj *)data)->y = va_arg(args, int);
		return object_create_boxed(&TestObj_Descriptor, data);
	} else {
		return object_create_err(err);
	}
}

Object TestObj_Clone_clone(const Object *obj);
static Clone TestObj_Clone = {.clone = TestObj_Clone_clone};

Object TestObj_TestObjImpl_get_x(const Object *obj) {
	return object_create_int(((TestObj *)object_get_data(obj))->x);
}
void TestObj_TestObjImpl_set_x(Object *obj, int x) {
	((TestObj *)object_get_data(obj))->x = x;
}
static TestObjImpl TestObj_TestObjImpl = {.get_x = TestObj_TestObjImpl_get_x,
					  .set_x = TestObj_TestObjImpl_set_x};

void TestObj_drop(Object *obj) { void *data = object_get_data(obj); }
Object TestObj_Clone_clone(const Object *obj) {
	int x = ((TestObj *)object_get_data(obj))->x;
	int y = ((TestObj *)object_get_data(obj))->y;
	ObjectImpl n = $object(TestObj, x, y);
	return n;
}

Test(core, test_obj) {
	var a = $object(TestObj, 1, 2);
	object_set_vtable(&a, &TestObj_TestObjImpl);
	let v = TestObjImpl_get_x(&a);
	cr_assert($is_int(v));
	cr_assert(!$is_uint(v));
	cr_assert_eq($int(v), 1);
	TestObjImpl_set_x(&a, 101);
	var v2 = TestObjImpl_get_x(&a);
	cr_assert($is_int(v2));
	cr_assert(!$is_uint(v2));
	cr_assert_eq($int(v2), 101);

	object_set_vtable(&a, &TestObj_Clone);
	let b = Clone_clone(&a);
	object_set_vtable(&b, &TestObj_TestObjImpl);
	let v3 = TestObjImpl_get_x(&b);
	cr_assert_eq($int(v3), 101);
}

// test_files.c
#include <sys/stat.h>

#include "stat.h"

// Constants
#define O_RDWR 0x0002
#define O_CREAT 0x0040
#define MODE_0644 0644

// test_files.c
#include <stat.h>

#define MODE_0644 0644

Test(core, files) {
	// Open file for writing
	int fd = open("/tmp/testfile.dat", O_RDWR | O_CREAT, MODE_0644);
	cr_assert(fd >= 0);

	// Write data
	const char *data = "Hello";
	unsigned long data_len = 5;  // Length of "Hello"
	long written = write(fd, data, data_len);
	cr_assert_eq(written, (long)data_len);

	// Close file
	cr_assert(close(fd) >= 0);

	// Reopen file for size check
	fd = open("/tmp/testfile.dat", O_RDWR, 0);
	cr_assert(fd > 0);

	// Check size
	// char stat_buf[256];  // Conservative buffer for struct stat
	struct stat stat_buf;

	fstat(fd, &stat_buf);
	printf("st->size=%i\n", stat_buf.st_size);
	long size = stat_get_size((struct stat *)&stat_buf);
	printf("size=%i,data_len=%i\n", size, data_len);
	cr_assert(size == (long)data_len);

	cr_assert(close(fd) >= 0);
}
