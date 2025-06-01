#include <criterion/criterion.h>
#include <error.h>
#include <signal.h>
#include <stdio.h>
#include <sys.h>
#include <unistd.h>

void timeout_test() { printf("tt\n"); }

Test(core, timeout) {
	Event events[10];
	int m = multiplex();
	for (int i = 0; i < 30; i++) {
		timeout(timeout_test, 100);
		int count = mwait(m, events, 10, 1000);
	}
	printf("complete\n");
}
