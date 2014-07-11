/*
 * control_plane.c - control plane implementation
 */

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <ix/control_plane.h>

struct cp_shmem *cp_shmem;

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

	return 0;
}
