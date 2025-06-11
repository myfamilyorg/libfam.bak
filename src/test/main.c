#include <colors.h>
#include <env.h>
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
	int test_count = 0;
	char *tp;

	tp = getenv("TEST_PATTERN");
	if (!tp || !strcmp(tp, "*"))
		printf("Running %i tests...\n", cur_tests);
	else
		printf("Running test %s...\n", tp);

	for (exe_test = 0; exe_test < cur_tests; exe_test++) {
		if (!tp || !strcmp(tp, "*") ||
		    !strcmp(tests[exe_test].name, tp)) {
			printf("running test %i [%s]\n", test_count,
			       tests[exe_test].name);
			tests[exe_test].test_fn();
			test_count++;
		}
	}
	printf("%sSuccess%s! %i %stests passed!%s\n", GREEN, RESET, test_count,
	       CYAN, CYAN);
	return 0;
}
