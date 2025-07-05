/*
#include <crc32c.H>

__attribute__((target("sse4.2"))) u32 crc32c(const void *data, u64 len) {
	const u8 *src = data;
	u64 i;
	u32 crc = 0xFFFFFFFF;

	for (i = 0; i < len; ++i) {
		crc = __builtin_ia32_crc32qi(crc, *src++);
	}

	return crc ^ 0xFFFFFFFF;
}

*/

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
		crc = __builtin_arm_crc32w(crc, chunk32);
	}
	for (; i < length; i = i + 1) {
		byte = ((const u8*)data)[i];
		crc = __builtin_arm_crc32b(crc, byte);
	}
#endif

	return (u32)(crc ^ 0xFFFFFFFF);
}
