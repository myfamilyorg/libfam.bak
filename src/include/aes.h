#ifndef _AES_H_
#define _AES_H_

#include <types.H>

#ifndef CTR
#define CTR 1
#endif

#define AES256 1
#define AES_BLOCKLEN 16
#define AES_KEYLEN 32
#define AES_keyExpSize 240

struct AES_ctx {
	u8 RoundKey[AES_keyExpSize];
	u8 Iv[AES_BLOCKLEN];
};

void AES_init_ctx(struct AES_ctx* ctx, const u8* key);
void AES_init_ctx_iv(struct AES_ctx* ctx, const u8* key, const u8* iv);
void AES_ctx_set_iv(struct AES_ctx* ctx, const u8* iv);
void AES_CTR_xcrypt_buffer(struct AES_ctx* ctx, u8* buf, u64 length);

#endif
