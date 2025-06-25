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

#include <error.H>
#include <rng.H>
#include <syscall.H>
#include <syscall_const.H>

i32 rng_init(Rng *rng) {
	u8 iv[16], key[32];
	i32 ret = 0;

	if (getrandom(key, 32, GRND_RANDOM)) ret = -1;
	if (getrandom(iv, 16, GRND_RANDOM)) ret = -1;
	if (!ret) aes_init(&rng->ctx, key, iv);

	return ret;
}

i32 rng_reseed(Rng *rng) { return rng_init(rng); }

void rng_gen(Rng *rng, void *v, u64 size) {
	aes_ctr_xcrypt_buffer(&rng->ctx, (u8 *)v, size);
}

#if TEST == 1
void rng_test_seed(Rng *rng, u8 key[32], u8 iv[16]) {
	u8 v0[1] = {0};
	aes_init(&rng->ctx, key, iv);
	rng_gen(rng, &v0, 1);
}
#endif /* TEST */

