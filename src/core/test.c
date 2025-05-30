#include <criterion/criterion.h>
#include <sys.h>

Test(core, sys1) {
	write(2, "hi\n", 3);
}
