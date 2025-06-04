#include <misc.h>
#include <stdio.h>
#include <test.h>

int cur_tests = 0;
int exe_test = 0;

TestEntry tests[MAX_TESTS];

void add_test_fn(void (*test_fn)(void), const char *name) {
	if (strlen(name) > MAX_TEST_NAME) panic("test name too long!");
	if (cur_tests >= MAX_TESTS) panic("too many tests!");
	tests[cur_tests].test_fn = test_fn;
	memset(tests[cur_tests].name, 0, MAX_TEST_NAME);
	strcpy(tests[cur_tests].name, name);
	cur_tests++;
}

int main() {
	printf("Running %i tests...\n", cur_tests);
	for (exe_test = 0; exe_test < cur_tests; exe_test++) {
		tests[exe_test].test_fn();
	}
	printf("Success! %i tests passed!\n", cur_tests);
	return 0;
}
