#include <criterion/criterion.h>
#include <stdio.h>
#include <signal.h>
#include <sys.h>
#include <unistd.h>

void to_fun() { printf("to_fun\n"); }

void handle_alarm() {
	printf("exiting\n");
	exit(0);
}

Test(core, timer) {
	signal(SIGALRM, handle_alarm);
	alarm(2);
	while (true);
	printf("timeout complete\n");
}
