#include <aes.h>
#include <test.h>

Test(crypto1) { ASSERT(1, "1"); }

Test(aes1) {
	const uint8_t key1[32] = {0};
	const uint8_t iv1[16] = {0};
	uint8_t buf[64] = {0};

	AesContext ctx;
	aes_init(&ctx, key1, iv1);
	aes_ctr_xcrypt_buffer(&ctx, buf, sizeof(buf));
	ASSERT_EQ(buf[0], 220, "aes220");
}

