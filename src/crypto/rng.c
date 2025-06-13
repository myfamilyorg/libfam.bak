#include <error.h>
#include <rng.h>
#include <syscall.h>
#include <syscall_const.h>

int rng_init(RngCtx *rng) {
	uint8_t iv[16], key[32];
	int ret = 0;

	if (getrandom(key, 32, GRND_RANDOM)) ret = -1;
	if (getrandom(iv, 16, GRND_RANDOM)) ret = -1;
	if (!ret) aes_init(&rng->ctx, key, iv);

	return ret;
}

int rng_reseed(RngCtx *rng) { return rng_init(rng); }

void rng_gen(RngCtx *rng, void *v, size_t size) {
	aes_ctr_xcrypt_buffer(&rng->ctx, (uint8_t *)v, size);
}

#if TEST == 1
void rng_test_seed(RngCtx *rng, uint8_t key[32], uint8_t iv[16]) {
	uint8_t v0[1] = {0};
	aes_init(&rng->ctx, key, iv);
	rng_gen(rng, &v0, 1);
}
#endif /* TEST */

