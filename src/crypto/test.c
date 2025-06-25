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

#include <aes.H>
#include <format.H>
#include <rng.H>
#include <sha1.H>
#include <test.H>

Test(crypto1) { ASSERT(1, "1"); }

Test(aes1) {
	const u8 key1[32] = {0};
	const u8 iv1[16] = {0};
	const u8 iv2[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5};
	u8 buf[64] = {0};

	AesContext ctx;
	aes_init(&ctx, key1, iv1);
	aes_ctr_xcrypt_buffer(&ctx, buf, sizeof(buf));
	ASSERT_EQ(buf[0], 220, "aes220");
	aes_set_iv(&ctx, iv2);
	aes_ctr_xcrypt_buffer(&ctx, buf, sizeof(buf));
	ASSERT_EQ(buf[0], 171, "aes220");
}

Test(rng) {
	Rng rng;
	u8 key[32] = {0};
	u8 iv[16] = {1};
	u64 v1 = 0, v2 = 0, v3 = 0, v4 = 0, v5 = 0, v6 = 0;
	rng_init(&rng);
	rng_gen(&rng, &v1, sizeof(u64));
	rng_gen(&rng, &v2, sizeof(u64));
	ASSERT(v1 != v2, "v1!=v2");
	ASSERT(v1 != 0, "v1!=0");
	ASSERT(v2 != 0, "v2!=0");

	rng_test_seed(&rng, key, iv);
	rng_gen(&rng, &v3, sizeof(u64));
	rng_gen(&rng, &v4, sizeof(u64));
	ASSERT_EQ(v3, 15566405176654077661UL, "v3=15566405176654077661");
	ASSERT_EQ(v4, 2865243117314082982UL, "v4=2865243117314082982");

	rng_reseed(&rng);

	rng_gen(&rng, &v5, sizeof(u64));
	rng_gen(&rng, &v6, sizeof(u64));
	ASSERT(v5 != v6, "v5!=v6");
	ASSERT(v5 != 0, "v5!=0");
	ASSERT(v6 != 0, "v6!=0");
}

Test(sha1) {
	u64 v = 123;
	u8 digest[24] = {0};
	u8 out[33] = {0};
	u8 buf4[64] = {4};
	u8 buflong[256] = {5};
	SHA1_CTX ctx = {0};
	sha1_init(&ctx);
	sha1_update(&ctx, &v, sizeof(u64));
	sha1_final(&ctx, digest);
	println("preout={}", out);
	{
		i32 i;
		for (i = 0; i < 24; i++) {
			println("{}={}", i, digest[i]);
		}
	}

	b64_encode(digest, 24, out, 33);
	println("out={}", out);
	{
		i32 i;
		for (i = 0; i < 24; i++) {
			println("{}={}", i, digest[i]);
		}
	}
	ASSERT(!strcmp(out, "bdYNn8pZRfNzUKFV3N4reh2uX2YAAAAA"),
	       "bdYNn8pZRfNzUKFV3N4reh2uX2YAAAAA");
	sha1_init(&ctx);
	sha1_update(&ctx, buf4, 60);
	sha1_final(&ctx, digest);
	b64_encode(digest, 24, out, 33);
	ASSERT(!strcmp(out, "pxH2MkE8YEMdKFrJl1vWVxsVRzAAAAAA"),
	       "pxH2MkE8YEMdKFrJl1vWVxsVRzAAAAAA");
	sha1_init(&ctx);
	sha1_update(&ctx, buflong, 252);
	sha1_update(&ctx, buflong, 150);
	sha1_final(&ctx, digest);
	b64_encode(digest, 24, out, 33);
	ASSERT(!strcmp(out, "UPbAbd1BjBI1Q+p8zMFyP2vVIjwAAAAA"),
	       "UPbAbd1BjBI1Q+p8zMFyP2vVIjwAAAAA");
}
