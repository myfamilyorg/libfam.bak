#include <init.h>
#include <linux/sched.h>
#include <signal.h>
#include <sys.h>
#include <syscall.h>

pid_t two(void) {
	struct clone_args args = {0};
	long ret;
	args.flags = CLONE_FILES;
	CLONE_FILES;
	args.pidfd = 0;
	args.child_tid = 0;
	args.parent_tid = 0;
	args.exit_signal = SIGCHLD;
	args.stack = 0;
	args.stack_size = 0;
	args.tls = 0;

	ret = clone3(&args, sizeof(args));
	if (ret == 0) begin();
	return (pid_t)ret;
}
