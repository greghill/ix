/*
 * mem.c - memory management
 */

#include <ix/stddef.h>
#include <ix/mem.h>
#include <ix/errno.h>
#include <ix/atomic.h>

#include <dune.h>

#include <stdlib.h>
#include <sys/mman.h>
#include <asm/mman.h>
#include <unistd.h>
#include <fcntl.h>

#if !defined(MAP_HUGE_2MB) || !defined(MAP_HUGE_1GB)
#warning "Your system does not support MAP_HUGETLB page sizes"
#endif

/*
 * Current Mapping Strategy:
 * 2 megabyte pages grow up from MEM_PHYS_BASE_ADDR
 * 1 gigabyte pages grow down from MEM_PHYS_BASE_ADDR
 * 4 kilobyte pages are allocated from the standard mmap region
 */
static atomic64_t up_base = ATOMIC_INIT(MEM_PHYS_BASE_ADDR);
static atomic64_t down_base = ATOMIC_INIT(MEM_PHYS_BASE_ADDR);

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
	void *vaddr = NULL;
	int flags = MAP_PRIVATE | MAP_ANONYMOUS;
	size_t len = nr * size;
	int perm;

	switch (size) {
	case PGSIZE_4KB:
		break;
	case PGSIZE_2MB:
		vaddr = (void *) atomic64_fetch_and_add(&up_base, len);
		flags |= MAP_HUGETLB | MAP_FIXED;
#ifdef MAP_HUGE_2MB
		flags |= MAP_HUGE_2MB;
#endif
		break;
	case PGSIZE_1GB:
#ifdef MAP_HUGE_1GB
		vaddr = (void *) atomic64_sub_and_fetch(&down_base, len);
		flags |= MAP_HUGETLB | MAP_HUGE_1GB | MAP_FIXED;
#else
		return MAP_FAILED;
#endif
		break;
	default: /* fail on other sizes */
		return MAP_FAILED;
	}

	vaddr = mmap(vaddr, len, PROT_READ | PROT_WRITE, flags, -1, 0);
	if (vaddr == MAP_FAILED)
		return MAP_FAILED;

	if (mbind(vaddr, len, numa_policy, mask ? mask->maskp : NULL,
		  mask ? mask->size : 0, MPOL_MF_STRICT))
		goto fail;

	perm = PERM_R | PERM_W;
	if (size == PGSIZE_2MB)
		perm |= PERM_BIG;
	else if (size == PGSIZE_1GB)
		perm |= PERM_BIG_1GB;
	if (dune_vm_map_phys(pgroot, vaddr, len,
			     (void *) dune_va_to_pa(vaddr), perm))
		goto fail;

	return vaddr;

fail:
	munmap(vaddr, len);
	return MAP_FAILED;
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
	dune_vm_unmap(pgroot, addr, nr * size);
	munmap(addr, nr * size);
}

#define PAGEMAP_PGN_MASK	0x7fffffffffffffULL
#define PAGEMAP_FLAG_PRESENT	(1ULL << 63)
#define PAGEMAP_FLAG_SWAPPED	(1ULL << 62)
#define PAGEMAP_FLAG_FILE	(1ULL << 61)
#define PAGEMAP_FLAG_SOFTDIRTY	(1ULL << 55)

/**
 * mem_lookup_page_machine_addrs - determines the machine address of pages
 * @addr: a pointer to the start of the pages (must be @size aligned)
 * @nr: the number of pages
 * @size: the page size (4KB, 2MB, or 1GB)
 * @maddrs: a pointer to an array of machine addresses (of @nr elements)
 *
 * @maddrs is filled with each page machine address.
 *
 * Returns 0 if successful, otherwise failure.
 */
int mem_lookup_page_machine_addrs(void *addr, int nr, int size,
			          machaddr_t *maddrs)
{
	int fd, i, ret = 0;
	uint64_t tmp;

	/*
	 * 4 KB pages could be swapped out by the kernel, so it is not
	 * safe to get a machine address. If we later decide to support
	 * 4KB pages, then we need to mlock() the page first.
	 */
	if (size == PGSIZE_4KB)
		return -EINVAL;

	fd = open("/proc/self/pagemap", O_RDONLY);
	if (fd < 0)
		return -EIO;

	for (i = 0; i < nr; i++) {
		if (lseek(fd, (((uintptr_t) addr + (i * size)) /
		    PGSIZE_4KB) * sizeof(uint64_t), SEEK_SET) ==
		    (off_t) -1) {
			ret = -EIO;
			goto out;
		}
	
		if (read(fd, &tmp, sizeof(uint64_t)) <= 0) {
			ret = -EIO;
			goto out;
		}

		if (!(tmp & PAGEMAP_FLAG_PRESENT)) {
			ret = -ENODEV;
			goto out;
		}

		maddrs[i] = (tmp & PAGEMAP_PGN_MASK) * PGSIZE_4KB;
	}

out:
	close(fd);
	return ret;
}

