#include <error.h>
#include <event.h>
#include <test.h>

Test(event) {
	Event events[10];
	int val = 101;
	int fds[2];
	int mplex = multiplex();
	int x;
	ASSERT(mplex > 0, "mplex");
	pipe(fds);
	ASSERT_EQ(mregister(mplex, fds[0], MULTIPLEX_FLAG_READ, &val), 0,
		  "mreg");
	ASSERT_EQ(write(fds[1], "abc", 3), 3, "write");
	err = 0;
	x = mwait(mplex, events, 10, 10);
	ASSERT_EQ(x, 1, "mwait");

	close(mplex);
	close(fds[0]);
	close(fds[1]);
}
