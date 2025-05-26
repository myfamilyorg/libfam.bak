#include <bptree.h>
#include <criterion/criterion.h>
#include <sys.h>
#include <stdio.h>
#include <sys/mman.h>

Test(store, bptree1) {
	const char *dat_file = "/tmp/store1.dat";
	size_t size = 1024 * 16384;
	remove(dat_file);
	int fd = ocreate(dat_file);
	int ftruncate_res = sys_ftruncate(fd, size);
	cr_assert(ftruncate_res == 0);

	char *base =
	    sys_mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	cr_assert(base != MAP_FAILED);

	BpTree tree;
	cr_assert_eq(bptree_init(&tree, base, fd, size), 0);
}
