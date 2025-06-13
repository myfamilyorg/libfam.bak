#ifndef _RNG_H
#define _RNG_H

#include <aes.h>
#include <types.h>

typedef struct {
	AesContext ctx;
} RngCtx;

int rng_init(RngCtx *rng);
int rng_reseed(RngCtx *rng);
void rng_gen(RngCtx *rng, void *v, size_t size);

#if TEST == 1
void rng_test_seed(RngCtx *rng, uint8_t key[32], uint8_t iv[16]);
#endif /* TEST */

#endif /* _RNG_H */
