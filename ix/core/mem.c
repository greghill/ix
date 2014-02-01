/*
 * mem.c - memory management
 */

#include <ix/stddef.h>
#include <ix/mem.h>
#include <ix/errno.h>

#include <stdlib.h>
#include <sys/mman.h>
#include <asm/mman.h>
#include <unistd.h>
#include <fcntl.h>

#if !defined(MAP_HUGE_2MB) || !defined(MAP_HUGE_1GB)
#warning "Your system does not support MAP_HUGETLB page sizes"
#endif

/**
 * mem_alloc_pages - allocates pages of memory
 * @nr: the number of pages to allocate
 * @size: the page size (4KB, 2MB, or 1GB)
 * @mask: the numa node mask
 * @numa_policy: the numa policy
 *
 * Returns a pointer (virtual address) to a page, or NULL if fail.
 */
void *mem_alloc_pages(int nr, int size, struct bitmask *mask, int numa_policy)
{
	void *vaddr;
	int flags = MAP_PRIVATE | MAP_ANONYMOUS;
	size_t len = nr * size;

	switch (size) {
	case PAGE_SIZE_4KB:
		break;
	case PAGE_SIZE_2MB:
		flags |= MAP_HUGETLB;
#ifdef MAP_HUGE_2MB
		flags |= MAP_HUGE_2MB;
#endif
	case PAGE_SIZE_1GB:
#ifdef MAP_HUGE_1GB
		flags |= MAP_HUGETLB | MAP_HUGE_1GB;
#else
		return NULL;
#endif
	default: /* fail on other sizes */
		return NULL;
	}

	vaddr = mmap(NULL, len, PROT_READ | PROT_WRITE, flags, -1, 0);
	if (vaddr == MAP_FAILED)
		return NULL;

	if (mbind(vaddr, len, numa_policy, mask ? mask->maskp : NULL,
		  mask ? mask->size : 0, MPOL_MF_STRICT))
		goto fail;

	/* FIXME: hook into Dune here */

	return vaddr;

fail:
	munmap(vaddr, len);
	return NULL;
}

/**
 * mem_alloc_pages_onnode - allocates pages on a given numa node
 * @nr: the number of pages
 * @size: the page size (4KB, 2MB, or 1GB)
 * @numa_node: the numa node to allocate the pages from
 * @numa_policy: how strictly to take @numa_node
 *
 * Returns a pointer (virtual address) to a page or NULL if fail.
 */
void *mem_alloc_pages_onnode(int nr, int size, int node, int numa_policy)
{
	void *vaddr;
	struct bitmask *mask = numa_allocate_nodemask();

	numa_bitmask_setbit(mask, node);
	vaddr = mem_alloc_pages(nr, size, mask, numa_policy);
	numa_bitmask_free(mask);

	return vaddr;
}

/**
 * mem_free_pages - frees pages of memory
 * @addr: a pointer to the start of the pages
 * @nr: the number of pages
 * @size: the page size (4KB, 2MB, or 1GB)
 */
void mem_free_pages(void *addr, int nr, int size)
{
	/* FIXME: hook into Dune here */

	munmap(addr, nr * size);
}

/**
 * mem_lookup_page_phys_addrs - determines the host-physical address of pages
 * @addr: a pointer to the start of the pages (must be @size aligned)
 * @nr: the number of pages
 * @size: the page size (4KB, 2MB, or 1GB)
 * @paddrs: a pointer to an array of physical addresses (of @nr elements)
 *
 * @paddrs is filled with each page physical address.
 *
 * Returns 0 if successful, otherwise failure.
 */
int mem_lookup_page_phys_addrs(void *addr, int nr, int size,
			       physaddr_t *paddrs)
{
	int fd, i, ret = 0;
	uint64_t tmp;

	fd = open("/proc/self/pagemap", O_RDONLY);
	if (fd < 0)
		return -EIO;

	for (i = 0; i < nr; i++) {
		if (lseek(fd, (((uintptr_t) addr + (i * size)) /
		    PAGE_SIZE_4KB) * sizeof(uint64_t), SEEK_SET) ==
		    (off_t) -1) {
			ret = -EIO;
			goto out;
		}
	
		if (read(fd, &tmp, sizeof(uint64_t)) <= 0) {
			ret = -EIO;
			goto out;
		}

		paddrs[i] = (tmp & 0x7fffffffffffffULL) * PAGE_SIZE_4KB;
	}

out:
	close(fd);
	return ret;
}

