#include <bptree.h>
#include <criterion/criterion.h>
#include <sys.h>

Test(store, bptree1) {
	const char *path = "/tmp/bptree1.dat";
	unlink(path);
	int fd = file(path);
	cr_assert(!fresize(fd, PAGE_SIZE * 10));
	BpTree tree;
	bptree_open(&tree, path);

	unlink(path);
}
