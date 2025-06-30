#ifndef _AES_H_
#define _AES_H_

#include <aes.H>
#include <types.H>

struct AES_ctx {
	u8 RoundKey[AES_KEYEXPSIZE];
	u8 Iv[AES_BLOCKLEN];
};

void AES_init_ctx(struct AES_ctx* ctx, const u8* key);
void AES_init_ctx_iv(struct AES_ctx* ctx, const u8* key, const u8* iv);
void AES_ctx_set_iv(struct AES_ctx* ctx, const u8* iv);
void AES_CTR_xcrypt_buffer(struct AES_ctx* ctx, u8* buf, u64 length);

#endif
