#include <criterion/criterion.h>
#include <error.h>
#include <socket.h>
#include <sys.h>

Test(net, socket1) {
	byte addr[4] = {127, 0, 0, 1};
	Socket s1, s2, s3, s4;
	int port = socket_listen(&s1, addr, 0, 10);
	cr_assert(port > 0);
	sleepm(10);
	cr_assert(!socket_connect(&s2, addr, port));
	cr_assert(s2 > 0);
	while (true) {
		if (socket_accept(&s1, &s3)) {
			if (err != EAGAIN && err != EDEADLK) cr_assert(false);
			sleepm(1);
		} else
			break;
	}

	cr_assert(s3 > 0);

	/* Test an error */
	err = SUCCESS;
	socket_accept(&s2, &s4);
	cr_assert_eq(err, EINVAL);

	int total_written = 0;

	while (total_written < 4) {
		int v = write(s2, "test", 4);
		cr_assert(v >= 0);
		total_written += v;
	}

	char buf[10];
	int total_read = 0;

	while (total_read < 4) {
		int v = read(s3, buf + total_read, 10);
		if (v < 0 && (err == EAGAIN || err == EDEADLK)) continue;
		cr_assert(v >= 0);
		total_read += v;
	}
	cr_assert_eq(total_read, 4);
	cr_assert_eq(total_written, 4);
	cr_assert_eq(buf[0], 't');
	cr_assert_eq(buf[1], 'e');
	cr_assert_eq(buf[2], 's');
	cr_assert_eq(buf[3], 't');

	close(s1);
	close(s2);
	close(s3);
}
