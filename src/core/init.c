#include <error.h>
#include <init.h>
#include <types.h>

#define MAX_EXIT 64

static int has_begun = 0;

void begin(void) {
	if (!has_begun) {
		signals_init();
		has_begun = 1;
	}
}

static void (*exit_fns[MAX_EXIT])(void);
static size_t exit_count = 0;

int register_exit(void (*fn)(void)) {
	size_t index = exit_count++;
	if (index >= MAX_EXIT) {
		err = ENOSPC;
		return -1;
	}
	exit_fns[index] = fn;
	return 0;
}

void __attribute__((destructor)) execute_exits(void) {
	int i;
	for (i = exit_count - 1; i >= 0; i--) {
		exit_fns[i]();
	}
	exit_count = 0;
}
