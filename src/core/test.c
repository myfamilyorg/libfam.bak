#include <criterion/criterion.h>
#include <error.h>
#include <signal.h>
#include <stdio.h>
#include <sys.h>
#include <unistd.h>

void timeout_test() { printf("tt\n"); }

Test(core, timeout) {
	timeout(timeout_test, 1);
	sleepm(3000);
	printf("complete\n");
}
