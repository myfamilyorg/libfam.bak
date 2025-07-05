#include <crc32c.H>
#include <format.H>
#include <types.H>

#if !defined(__x86_64__) && !defined(__aarch64__)
#error Unsupported platform: only x86-64 and ARM64 are supported
#endif

u32 crc32c(const void* data, u64 length) {
	u64 crc;
	u64 i;
	u32 chunk32;
	u8 byte;

	if (data == (void*)0) return 0;

	crc = 0xFFFFFFFF;

#ifdef __x86_64__
	for (i = 0; i + 4 <= length; i = i + 4) {
		memcpy(&chunk32, (const u8*)data + i, 4);
		crc = __builtin_ia32_crc32si(crc, chunk32);
	}
	for (; i < length; i = i + 1) {
		byte = ((const u8*)data)[i];
		crc = __builtin_ia32_crc32qi(crc, byte);
	}
#elif defined(__aarch64__)
	for (i = 0; i + 4 <= length; i = i + 4) {
		memcpy(&chunk32, (const u8*)data + i, 4);
		crc = __builtin_aarch64_crc32w(crc, chunk32);
	}
	for (; i < length; i = i + 1) {
		byte = ((const u8*)data)[i];
		crc = __builtin_aarch64_crc32b(crc, byte);
	}
#endif

	return (u32)(crc ^ 0xFFFFFFFF);
}

u32 crc_test1(u32 value) {
	u32 crc = 0x0;
#ifdef __x86_64__
	return __builtin_ia32_crc32si(crc, value);
#else
	return __builtin_aarch64_crc32w(crc, value);
#endif
}
