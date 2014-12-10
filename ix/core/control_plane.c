/*
 * control_plane.c - control plane implementation
 */

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <ix/control_plane.h>
#include <ix/log.h>

volatile struct cp_shmem *cp_shmem;

DEFINE_PERCPU(volatile struct command_struct *, cp_cmd);

int cp_init(void)
{
	int fd, ret;
	void *vaddr;

	fd = shm_open("/ix", O_RDWR | O_CREAT | O_TRUNC, 0660);
	if (fd == -1)
		return 1;

	ret = ftruncate(fd, sizeof(struct cp_shmem));
	if (ret)
		return ret;

	vaddr = mmap(NULL, sizeof(struct cp_shmem), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (vaddr == MAP_FAILED)
		return 1;

	cp_shmem = vaddr;

	bzero((void *)cp_shmem, sizeof(struct cp_shmem));

	return 0;
}

void cp_idle(void)
{
	int fd, ret;
	char buf;

	percpu_get(cp_cmd)->cmd_id = CP_CMD_NOP;
	percpu_get(cp_cmd)->status = CP_STATUS_READY;
	fd = open((char *) percpu_get(cp_cmd)->idle.fifo, O_RDONLY);
	if (fd != -1) {
		ret = read(fd, &buf, 1);
		if (ret == -1)
			log_err("read on wakeup pipe returned -1 (errno=%d)\n", errno);
		close(fd);
	}
}
