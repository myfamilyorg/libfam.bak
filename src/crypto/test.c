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
#include <crc32c.H>
#include <format.H>
#include <rng.H>
#include <sha1.H>
#include <test.H>

Test(aes1) {
	u8 key[32] = {0x60, 0x3d, 0xeb, 0x10, 0x15, 0xca, 0x71, 0xbe,
		      0x2b, 0x73, 0xae, 0xf0, 0x85, 0x7d, 0x77, 0x81,
		      0x1f, 0x35, 0x2c, 0x07, 0x3b, 0x61, 0x08, 0xd7,
		      0x2d, 0x98, 0x10, 0xa3, 0x09, 0x14, 0xdf, 0xf4};
	u8 in[64] = {0x60, 0x1e, 0xc3, 0x13, 0x77, 0x57, 0x89, 0xa5, 0xb7, 0xa7,
		     0xf5, 0x04, 0xbb, 0xf3, 0xd2, 0x28, 0xf4, 0x43, 0xe3, 0xca,
		     0x4d, 0x62, 0xb5, 0x9a, 0xca, 0x84, 0xe9, 0x90, 0xca, 0xca,
		     0xf5, 0xc5, 0x2b, 0x09, 0x30, 0xda, 0xa2, 0x3d, 0xe9, 0x4c,
		     0xe8, 0x70, 0x17, 0xba, 0x2d, 0x84, 0x98, 0x8d, 0xdf, 0xc9,
		     0xc5, 0x8d, 0xb6, 0x7a, 0xad, 0xa6, 0x13, 0xc2, 0xdd, 0x08,
		     0x45, 0x79, 0x41, 0xa6};

	u8 iv[16] = {0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
		     0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff};
	u8 out[64] = {
	    0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96, 0xe9, 0x3d, 0x7e,
	    0x11, 0x73, 0x93, 0x17, 0x2a, 0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03,
	    0xac, 0x9c, 0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51, 0x30,
	    0xc8, 0x1c, 0x46, 0xa3, 0x5c, 0xe4, 0x11, 0xe5, 0xfb, 0xc1, 0x19,
	    0x1a, 0x0a, 0x52, 0xef, 0xf6, 0x9f, 0x24, 0x45, 0xdf, 0x4f, 0x9b,
	    0x17, 0xad, 0x2b, 0x41, 0x7b, 0xe6, 0x6c, 0x37, 0x10};
	AesContext ctx;

	aes_init(&ctx, key, iv);
	aes_set_iv(&ctx, iv);
	aes_ctr_xcrypt_buffer(&ctx, in, 64);

	ASSERT(!memcmp((u8*)out, (u8*)in, 64), "aes256 test vector");
}

void hex_to_bytes(const char* hex, u8* bytes, u64 len) {
	u64 i;
	u8 high;
	u8 low;
	u8 byte;

	for (i = 0; i < len; i++) {
		high = hex[2 * i];
		low = hex[2 * i + 1];

		if (high == 0 || low == 0) {
			println("hex_to_bytes: null char at {}", 2 * i);
			ASSERT(0, "bad hex value");
		}

		byte = (high <= '9' ? high - '0' : high - 'A' + 10) << 4;
		byte |= (low <= '9' ? low - '0' : low - 'A' + 10);
		bytes[i] = byte;
	}
}

Test(sha1) {
	u64 v = 123;
	u8 digest[20] = {0};
	u8 out[33] = {0};
	u8 buf4[64] = {4};
	u8 buflong[256] = {5};
	SHA1_CTX ctx = {0};
	sha1_init(&ctx);
	sha1_update(&ctx, &v, sizeof(u64));
	sha1_final(&ctx, digest);
	b64_encode(digest, 20, out, 33);
	ASSERT(!strcmp(out, "bdYNn8pZRfNzUKFV3N4reh2uX2Y="),
	       "bdYNn8pZRfNzUKFV3N4reh2uX2Y=");
	sha1_init(&ctx);
	sha1_update(&ctx, buf4, 60);
	sha1_final(&ctx, digest);
	b64_encode(digest, 20, out, 33);
	ASSERT(!strcmp(out, "pxH2MkE8YEMdKFrJl1vWVxsVRzA="),
	       "pxH2MkE8YEMdKFrJl1vWVxsVRzA=");
	sha1_init(&ctx);
	sha1_update(&ctx, buflong, 252);
	sha1_update(&ctx, buflong, 150);
	sha1_final(&ctx, digest);
	b64_encode(digest, 20, out, 33);
	ASSERT(!strcmp(out, "UPbAbd1BjBI1Q+p8zMFyP2vVIjw="),
	       "UPbAbd1BjBI1Q+p8zMFyP2vVIjw=");
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

Test(crc32c) {
	/* Note: seem to produce different results than test vectors.
	 * While non-standard it appears to work.
	 * */
	const char* hello_crc = "Hello, CRC-32C!";
	u32 out = crc32c(hello_crc, strlen(hello_crc));
	ASSERT_EQ(out, 0x6F23DDF, "Hello, CRC-32C!");
}
