#include <types.h>

int __cas32(volatile uint32_t *a, uint32_t *expected, uint32_t desired) {
	uint32_t result;
	int success;
	__asm__ volatile(
	    "ldaxr %w0, [%2]\n"	   /* Load-exclusive 32-bit */
	    "cmp %w0, %w3\n"	   /* Compare with *expected */
	    "b.ne 1f\n"		   /* Jump to fail if not equal */
	    "stxr w1, %w4, [%2]\n" /* Store-exclusive desired */
	    "cbz w1, 2f\n"	   /* Jump to success if store succeeded */
	    "1: mov %w1, #0\n"	   /* Set success = 0 (fail) */
	    "b 3f\n"
	    "2: mov %w1, #1\n" /* Set success = 1 (success) */
	    "3: dmb ish\n"     /* Memory barrier for sequential consistency */
	    : "=&r"(result), "=&r"(success)
	    : "r"(a), "r"(*expected), "r"(desired)
	    : "x0", "x1", "memory");
	if (!success) *expected = result; /* Update *expected on failure */
	return success;
}

uint32_t __add32(volatile uint32_t *a, uint32_t v) {
	uint32_t old, tmp;
	__asm__ volatile(
	    "1: ldaxr %w0, [%2]\n" /* Load-exclusive 32-bit */
	    "add %w1, %w0, %w3\n"  /* Compute new value */
	    "stxr w4, %w1, [%2]\n" /* Store-exclusive */
	    "cbnz w4, 1b\n"	   /* Retry if store failed */
	    "dmb ish\n"		   /* Memory barrier */
	    : "=&r"(old), "=&r"(tmp), "+r"(a)
	    : "r"(v)
	    : "x4", "memory");
	return old;
}

uint32_t __sub32(volatile uint32_t *a, uint32_t v) { return __add32(a, -v); }

uint32_t __load32(volatile uint32_t *a) {
	uint32_t value;
	__asm__ volatile(
	    "ldar %w0, [%1]\n" /* Load with acquire ordering 32-bit */
	    : "=r"(value)
	    : "r"(a)
	    : "memory");
	return value;
}

void __store32(volatile uint32_t *a, uint32_t v) {
	__asm__ volatile(
	    "stlr %w1, [%0]\n" /* Store with release ordering 32-bit */
	    :
	    : "r"(a), "r"(v)
	    : "memory");
}

uint32_t __or32(volatile uint32_t *a, uint32_t v) {
	uint32_t old, tmp;
	__asm__ volatile(
	    "1: ldaxr %w0, [%2]\n" /* Load-exclusive 32-bit */
	    "orr %w1, %w0, %w3\n"  /* Bitwise OR */
	    "stxr w4, %w1, [%2]\n" /* Store-exclusive */
	    "cbnz w4, 1b\n"	   /* Retry if store failed */
	    "dmb ish\n"		   /* Memory barrier */
	    : "=&r"(old), "=&r"(tmp), "+r"(a)
	    : "r"(v)
	    : "x4", "memory");
	return old;
}

uint32_t __and32(volatile uint32_t *a, uint32_t v) {
	uint32_t old, tmp;
	__asm__ volatile(
	    "1: ldaxr %w0, [%2]\n" /* Load-exclusive 32-bit */
	    "and %w1, %w0, %w3\n"  /* Bitwise AND */
	    "stxr w4, %w1, [%2]\n" /* Store-exclusive */
	    "cbnz w4, 1b\n"	   /* Retry if store failed */
	    "dmb ish\n"		   /* Memory barrier */
	    : "=&r"(old), "=&r"(tmp), "+r"(a)
	    : "r"(v)
	    : "x4", "memory");
	return old;
}

int __cas64(volatile uint64_t *a, uint64_t expected, uint64_t desired) {
	uint64_t result;
	int success;
	__asm__ volatile(
	    "ldaxr %0, [%2]\n"	  /* Load-exclusive 64-bit */
	    "cmp %0, %3\n"	  /* Compare with expected */
	    "b.ne 1f\n"		  /* Jump to fail */
	    "stxr w1, %4, [%2]\n" /* Store-exclusive desired */
	    "cbz w1, 2f\n"	  /* Jump to success */
	    "1: mov %w1, #0\n"	  /* Set success = 0 (fail) */
	    "b 3f\n"
	    "2: mov %w1, #1\n" /* Set success = 1 (success) */
	    "3: dmb ish\n"     /* Memory barrier */
	    : "=&r"(result), "=&r"(success)
	    : "r"(a), "r"(expected), "r"(desired)
	    : "x0", "x1", "memory");
	if (!success) *(&expected) = result;
	return success;
}

uint64_t __load64(volatile uint64_t *a) {
	uint64_t value;
	__asm__ volatile(
	    "ldar %0, [%1]\n" /* Load with acquire ordering 64-bit */
	    : "=r"(value)
	    : "r"(a)
	    : "memory");
	return value;
}

uint64_t __add64(volatile uint64_t *a, uint64_t v) {
	uint64_t old, tmp;
	__asm__ volatile(
	    "1: ldaxr %0, [%2]\n" /* Load-exclusive 64-bit */
	    "add %1, %0, %3\n"	  /* Compute new value */
	    "stxr w4, %1, [%2]\n" /* Store-exclusive */
	    "cbnz w4, 1b\n"	  /* Retry if store failed */
	    "dmb ish\n"		  /* Memory barrier */
	    : "=&r"(old), "=&r"(tmp), "+r"(a)
	    : "r"(v)
	    : "x4", "memory");
	return old;
}

uint64_t __sub64(volatile uint64_t *a, uint64_t v) { return __add64(a, -v); }

void __store64(volatile uint64_t *a, uint64_t v) {
	__asm__ volatile(
	    "stlr %1, [%0]\n" /* Store with release ordering 64-bit */
	    :
	    : "r"(a), "r"(v)
	    : "memory");
}

uint64_t __or64(volatile uint64_t *a, uint64_t v) {
	uint64_t old, tmp;
	__asm__ volatile(
	    "1: ldaxr %0, [%2]\n" /* Load-exclusive 64-bit */
	    "orr %1, %0, %3\n"	  /* Bitwise OR (64-bit) */
	    "stxr w4, %1, [%2]\n" /* Store-exclusive 64-bit */
	    "cbnz w4, 1b\n"	  /* Retry if store failed */
	    "dmb ish\n"		  /* Memory barrier */
	    : "=&r"(old), "=&r"(tmp), "+r"(a)
	    : "r"(v)
	    : "x4", "memory");
	return old;
}

uint64_t __and64(volatile uint64_t *a, uint64_t v) {
	uint64_t old, tmp;
	__asm__ volatile(
	    "1: ldaxr %0, [%2]\n" /* Load-exclusive 64-bit */
	    "and %1, %0, %3\n"	  /* Bitwise AND (64-bit) */
	    "stxr w4, %1, [%2]\n" /* Store-exclusive 64-bit */
	    "cbnz w4, 1b\n"	  /* Retry if store failed */
	    "dmb ish\n"		  /* Memory barrier */
	    : "=&r"(old), "=&r"(tmp), "+r"(a)
	    : "r"(v)
	    : "x4", "memory");
	return old;
}
