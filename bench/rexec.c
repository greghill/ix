#include <signal.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static pid_t pid;

static void sig_handler(int signal)
{
	kill(-pid, SIGKILL);
	exit(0);
}

int main(int argc, char **argv)
{
	char buf;

	signal(SIGINT, sig_handler);
	signal(SIGHUP, sig_handler);
	signal(SIGTERM, sig_handler);
	signal(SIGKILL, sig_handler);
	signal(SIGPIPE, sig_handler);
	signal(SIGCHLD, sig_handler);
	prctl(PR_SET_PDEATHSIG, SIGINT, 0, 0, 0);
	if ((pid = fork())) {
		setpgid(pid, pid);
		read(0, &buf, sizeof(buf));
		sig_handler(0);
		return 0;
	}
	execvp(argv[1], &argv[1]);
	return 0;
}
