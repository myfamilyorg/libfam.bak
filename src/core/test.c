#include <criterion/criterion.h>
#include <signal.h>
#include <stdio.h>
#include <sys.h>
#include <unistd.h>

void handle_alarm() { printf("exiting\n"); }

Test(core, timer) {
	signal(SIGALRM, handle_alarm);
	alarm(2);
	sleepm(4000);
	printf("timeout complete\n");
}
