#include <criterion/criterion.h>
#include <error.h>
#include <signal.h>
#include <stdio.h>
#include <sys.h>
#include <unistd.h>

void handle_alarm() { printf("exiting\n"); }

Test(core, timer) {
	struct sigaction response = {0};
	struct sigaction myaction = {
	    .sa_handler = handle_alarm, .sa_mask = 0, .sa_flags = 0};

	cr_assert(!sigaction(SIGALRM, &myaction, &response));

	signal(SIGALRM, handle_alarm);
	alarm(2);
	sleepm(4000);
	printf("timeout complete\n");
}
