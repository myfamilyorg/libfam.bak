#include <criterion/criterion.h>
#include <error.h>
#include <socket.h>
#include <sys.h>

Test(net, evh1) {
	byte addr[4] = {127, 0, 0, 1};
	Socket s1, s2, s3;
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

	/* Test an error */
	socket_accept(&s2, &s3);
	cr_assert_eq(err, EINVAL);
}
