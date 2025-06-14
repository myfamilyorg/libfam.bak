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

#include <misc.H>
#include <types.H>

typedef struct {
	uint32_t state[5];
	uint64_t count;
	uint8_t buffer[64];
} SHA1_CTX;

void sha1_init(SHA1_CTX *ctx) {
	ctx->state[0] = 0x67452301;
	ctx->state[1] = 0xEFCDAB89;
	ctx->state[2] = 0x98BADCFE;
	ctx->state[3] = 0x10325476;
	ctx->state[4] = 0xC3D2E1F0;
	ctx->count = 0;
}

#define ROTL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

static void sha1_transform(SHA1_CTX *ctx, const uint8_t *data) {
	uint32_t a, b, c, d, e, t, w[80];
	int i;

	a = ctx->state[0];
	b = ctx->state[1];
	c = ctx->state[2];
	d = ctx->state[3];
	e = ctx->state[4];

	for (i = 0; i < 16; i++) {
		w[i] = (data[i * 4] << 24) | (data[i * 4 + 1] << 16) |
		       (data[i * 4 + 2] << 8) | data[i * 4 + 3];
	}
	for (; i < 80; i++) {
		w[i] = ROTL(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
	}

	for (i = 0; i < 80; i++) {
		if (i < 20) {
			t = (b & c) | (~b & d);
			t += 0x5A827999;
		} else if (i < 40) {
			t = b ^ c ^ d;
			t += 0x6ED9EBA1;
		} else if (i < 60) {
			t = (b & c) | (b & d) | (c & d);
			t += 0x8F1BBCDC;
		} else {
			t = b ^ c ^ d;
			t += 0xCA62C1D6;
		}
		t += ROTL(a, 5) + e + w[i];
		e = d;
		d = c;
		c = ROTL(b, 30);
		b = a;
		a = t;
	}

	ctx->state[0] += a;
	ctx->state[1] += b;
	ctx->state[2] += c;
	ctx->state[3] += d;
	ctx->state[4] += e;
}

void sha1_update(SHA1_CTX *ctx, const uint8_t *data, size_t len) {
	size_t i = ctx->count % 64;
	ctx->count += len;

	if (i + len < 64) {
		memcpy(ctx->buffer + i, data, len);
		return;
	}

	if (i) {
		size_t n = 64 - i;
		memcpy(ctx->buffer + i, data, n);
		sha1_transform(ctx, ctx->buffer);
		data += n;
		len -= n;
	}

	while (len >= 64) {
		sha1_transform(ctx, data);
		data += 64;
		len -= 64;
	}

	if (len) {
		memcpy(ctx->buffer, data, len);
	}
}

void sha1_final(SHA1_CTX *ctx, uint8_t *digest) {
	uint64_t bits;
	size_t i = ctx->count % 64;
	ctx->buffer[i++] = 0x80;

	if (i > 56) {
		memset(ctx->buffer + i, 0, 64 - i);
		sha1_transform(ctx, ctx->buffer);
		i = 0;
	}
	memset(ctx->buffer + i, 0, 56 - i);

	bits = ctx->count * 8;
	for (i = 0; i < 8; i++) {
		ctx->buffer[56 + i] = (bits >> (56 - i * 8)) & 0xFF;
	}

	sha1_transform(ctx, ctx->buffer);

	for (i = 0; i < 5; i++) {
		digest[i * 4] = (ctx->state[i] >> 24) & 0xFF;
		digest[i * 4 + 1] = (ctx->state[i] >> 16) & 0xFF;
		digest[i * 4 + 2] = (ctx->state[i] >> 8) & 0xFF;
		digest[i * 4 + 3] = ctx->state[i] & 0xFF;
	}
}
