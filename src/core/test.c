#include <criterion/criterion.h>
#include <stdio.h>
#include <sys.h>

void to_fun() { printf("to_fun\n"); }

Test(core, timer) {
	timeout(to_fun, 100);
	/*sleepm(1000);*/
	while (true);
	printf("timeout complete\n");
}
