#include <bptree.h>
#include <criterion/criterion.h>
#include <misc.h>

Test(core, store1) {
	const char *dat_file = "/tmp/store1.dat";
	remove(dat_file);
	int fd = open_create(dat_file);

	remove(dat_file);
}
