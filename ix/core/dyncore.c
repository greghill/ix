/*
 * dyncore.c - the dynamic core system
 */

#define _GNU_SOURCE

#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/inotify.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>

#include <asm/cpu.h>
#include <ix/dyncore.h>

/*
TODO: better mkdir mechanism. handle existing directories and files.
TODO: inotify does not seem very robust. what will happen if user writes invalid value in cpuset? we will have to write back to the file to fix it.
TODO: handle old_cpuset -> new_cpuset migration. NUMA aware. migrate some workers. kill (through the yield mechanism) excess workers. launch new workers.
TODO: implement the yield mechanism.
TODO: handle yield timeout. what to do? kill the process?
TODO: process inotify's event->name
TODO: process multiple inotify events
*/

static int inotify_fd;
static dune_worker_cb worker_cb;
static dune_worker_pending worker_pending;
static char pathname[256];

struct worker {
	int cpu;
	int active;
	int stop;
	int startpipefd[2];
};

struct worker worker[32]; /* TODO: fix constant CPU count */

static pid_t gettid(void)
{
	return syscall(SYS_gettid);
}

static int start_worker(void *p)
{
	struct worker *worker;
	int ret;
	int fd;
	char buffer[16];

	worker = p;

	ret = snprintf(pathname, sizeof(pathname), "/dev/cpuset/%d/tasks", worker->cpu);
	if (ret < 0 || ret >= sizeof(pathname))
		return 1;

	fd = open(pathname, O_WRONLY);
	if (fd == -1)
		return 1;

	ret = snprintf(buffer, sizeof(buffer), "%d", gettid());
	if (ret < 0 || ret >= sizeof(pathname))
		return 1;

	if (write(fd, buffer, ret) != ret)
		return 1;

	if (close(fd) == -1)
		return 1;

	// TODO: uncomment when we start using Dune
	//if (dune_enter())
	//	return 1;

	while (1) {
		read(worker->startpipefd[0], buffer, 1);

		// TODO: are there possible race conditions here?
		worker->active = 1;
		worker->stop = 0;

		while (1) {
			while (!worker->stop && !worker_pending(worker))
				cpu_relax();
			if (worker->stop)
				break;
			if (worker_cb(worker))
				break;
		}

		worker->active = 0;
	}

	return 0;
}

#define STACK_SIZE 32768

static int handle_cpuset_change(int cpuset)
{
	//int wait = 0;
	int cpu;

	// TODO: these hardcoded 0 and 31 look wrong.
	for (cpu = 0; cpu < 31; cpu++) {
		if (!worker[cpu].active && cpuset & 1)
			write(worker[cpu].startpipefd[1], &cpu, 1);
		else if (worker[cpu].active && !(cpuset & 1))
			worker[cpu].stop = 1;
		cpuset >>= 1;
	}

	//if (wait) {
	//	sleep(5);
        //
	//	for (i=0;i<32;i++) {
	//		if (worker[i].active) {
	//			fprintf(stderr, "Worker %d did not respond to a stop request in a timely manner. Terminating.\n", i);
	//			exit(1);
	//		}
	//	}
	//}

	return 0;
}

static int dune_register_worker(dune_worker_cb cb, dune_worker_pending pending)
{
	char *dune_ctl_path;
	int ret;
	mode_t mode;

	worker_cb = 0;
	worker_pending = 0;

	inotify_fd = inotify_init();
	if (inotify_fd == -1)
		return 1;

	dune_ctl_path = getenv("DUNE_CTL_PATH");
	if (dune_ctl_path == NULL)
		return 1;

	ret = snprintf(pathname, sizeof(pathname), "%s/planes", dune_ctl_path);
	if (ret < 0 || ret >= sizeof(pathname))
		return 1;

	if (mkdir(pathname, 0777) == -1)
		return 1;

	ret = snprintf(pathname, sizeof(pathname), "%s/planes/%d", dune_ctl_path, getpid());
	if (ret < 0 || ret >= sizeof(pathname))
		return 1;

	if (mkdir(pathname, 0777) == -1)
		return 1;

	if (inotify_add_watch(inotify_fd, pathname, IN_CLOSE_WRITE) == -1)
		return 1;

	ret = snprintf(pathname, sizeof(pathname), "%s/planes/%d/cpuset", dune_ctl_path, getpid());
	if (ret < 0 || ret >= sizeof(pathname))
		return 1;

	mode = S_IFREG | 0666;
	if (mknod(pathname, mode, 0) == -1)
		return 1;

	worker_cb = cb;
	worker_pending = pending;

	return 0;
}

static int start_threads(void)
{
	int i;
	int flags;
	void *stack;

	// TODO: this hardcoded 0 and 31 look wrong.
	for (i = 0; i < 31; i++) {
		// TODO: store stack somewhere. we might need to free it
		stack = malloc(STACK_SIZE);
		pipe(worker[i].startpipefd);
		worker[i].cpu = i;
		flags = CLONE_THREAD|CLONE_SIGHAND|CLONE_VM|CLONE_FS|CLONE_FILES;
		clone(start_worker, stack + STACK_SIZE, flags, &worker[i]);
	}

	return 0;
}

static int dune_start_workers(void)
{
	int cpuset_fd;
	int count;
	char buffer[256];
	int new_cpuset;
	fd_set readfds;
	char *dune_ctl_path;
	int ret;
	//struct inotify_event *event;

	if (worker_cb == 0 || worker_pending == 0)
		return 1;

	dune_ctl_path = getenv("DUNE_CTL_PATH");
	if (dune_ctl_path == NULL)
		return 1;

	ret = snprintf(pathname, sizeof(pathname), "%s/planes/%d/cpuset", dune_ctl_path, getpid());
	if (ret < 0 || ret >= sizeof(pathname))
		return 1;

	cpuset_fd = open(pathname, O_RDONLY);
	if (cpuset_fd == -1)
		return 1;

	start_threads();

	FD_ZERO(&readfds);

	while (1) {
		if (lseek(cpuset_fd, 0, SEEK_SET) == (off_t) -1)
			return 1;
		count = read(cpuset_fd, buffer, sizeof(buffer)-1);
		if (count == -1)
			return 1;
		buffer[count] = 0;
		new_cpuset = strtoul(buffer, NULL, 0);
		if (handle_cpuset_change(new_cpuset))
			return 1;
		FD_SET(inotify_fd, &readfds);
		if (select(inotify_fd+1, &readfds, NULL, NULL, NULL) == -1)
			return 1;
		count = read(inotify_fd, &buffer, sizeof(buffer));
		if (count == -1)
			return 1;
		//event = (struct inotify_event *) buffer;
	}

	return 0;
}

int dyncore_init(dune_worker_cb cb, dune_worker_pending pending)
{
	dune_register_worker(cb, pending);
	dune_start_workers();
	return 0;
}
