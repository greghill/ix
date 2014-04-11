/*
 * mem.c - memory management routines
 *
 * FIXME: this is really only a placeholder right now.
 */

#include <ix/stddef.h>
#include <pthread.h>
#include "ix.h"

static uintptr_t ixmem_pos = MEM_USER_IOMAPM_BASE_ADDR;
static pthread_mutex_t ixmem_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * ixmem_alloc_pages - allocates 2MB pages
 * @nrpages: the number of pages
 *
 * Returns a start address, or NULL if fail.
 */
void *ix_alloc_pages(int nrpages)
{
	void *addr;
	int ret;

	pthread_mutex_lock(&ixmem_mutex);
	addr = (void *) ixmem_pos;
	ixmem_pos += PGSIZE_2MB * nrpages;
	pthread_mutex_unlock(&ixmem_mutex);

	ret = sys_mmap(addr, nrpages, PGSIZE_2MB, VM_PERM_R | VM_PERM_W);
	if (ret)
		return NULL;

	return addr;
}

/**
 * ixmem_free_pages - frees 2MB pages
 * @addr: the start address
 * @nrpages: the number of pages
 */
void ix_free_pages(void *addr, int nrpages)
{
	sys_unmap(addr, nrpages, PGSIZE_2MB);
}

