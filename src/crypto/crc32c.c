#include <libfam/crc32c.H>
#include <libfam/format.H>
#include <libfam/types.H>

u32 crc32c(const u8* data, u64 length) {
	u32 crc;
	u64 i;
	u32 chunk32;
	u8 byte;

	if (data == (void*)0) return 0;

	crc = 0xFFFFFFFF;

#ifdef __x86_64__
	for (i = 0; i + 4 <= length; i = i + 4) {
		memcpy(&chunk32, data + i, 4);
		__asm__ volatile("crc32l %2, %0"
				 : "=r"(crc)
				 : "0"(crc), "r"(chunk32)
				 : "cc");
	}
	for (; i < length; i = i + 1) {
		byte = data[i];
		__asm__ volatile("crc32b %2, %0"
				 : "=r"(crc)
				 : "0"(crc), "r"((u8)byte)
				 : "cc");
	}
#elif defined(__aarch64__)
	for (i = 0; i + 4 <= length; i = i + 4) {
		memcpy(&chunk32, data + i, 4);
		__asm__ volatile("crc32cw %w0, %w0, %w1"
				 : "=r"(crc)
				 : "r"(chunk32), "0"(crc));
	}
	for (; i < length; i = i + 1) {
		byte = data[i];
		__asm__ volatile("crc32cb %w0, %w0, %w1"
				 : "=r"(crc)
				 : "r"((u8)byte), "0"(crc));
	}
#else
#error Unsupported platform: only x86-64 and ARM64 are supported
#endif

	return crc ^ 0xFFFFFFFF;
}
