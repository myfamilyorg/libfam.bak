#include <atomic.h>
#include <criterion/criterion.h>
#include <stdio.h>
#include <sys.h>

typedef struct {
	int value;
} CloneTest;

Test(core, clone1) {
	CloneTest *clone_test = smap(sizeof(CloneTest));
	cr_assert(clone_test);
	clone_test->value = -1;
	const char *tmpfile = "/tmp/clone1.dat";
	unlink(tmpfile);
	int fd = file(tmpfile);
	write(fd, "abc", 3);
	close(fd);
	if (two()) {
		char buf[10] = {0};
		ssize_t len;
		while (ALOAD(&clone_test->value) == -1) yield();
		len = read(clone_test->value, buf, 10);
		cr_assert_eq(len, 3);
		cr_assert_eq(buf[0], 'a');
		cr_assert_eq(buf[1], 'b');
		cr_assert_eq(buf[2], 'c');
	} else {
		int fd = file(tmpfile);
		cr_assert(fd != -1);
		ASTORE(&clone_test->value, fd);
		exit(0);
	}

	unlink(tmpfile);
}
