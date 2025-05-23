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

static ObjectDescriptor MyStruct_Descriptor = {
    .type_name = "MyStruct",
    .drop = MyStruct_drop,
    .table = NULL,

};

void MyStruct_speak(const Object *obj);
static Speak MyStruct_Speak = {.speak = MyStruct_speak};

void MyStruct_drop(Object *obj) { printf("drop MyStruct\n"); }
void MyStruct_speak(const Object *obj) { printf("bark\n"); }

Test(core, obj2) {
	let ms1 = $object(MyStruct, (MyStruct){.a = 4, .b = 7});
	object_set_vtable(&ms1, &MyStruct_Speak);
	speak(&ms1);
	object_cleanup(&ms1);
	object_cleanup(&ms1);
	object_cleanup(&ms1);
}

