/*
 * syscall.c - system call support
 *
 * FIXME: We should dispatch native system calls directly from the syscall
 * entry assembly to improve performance, but that requires changes to
 * libdune.
 */

#include <ix/stddef.h>
#include <ix/syscall.h>
#include <ix/errno.h>
#include <ix/uaccess.h>
#include <ix/timer.h>
#include <ix/ethdev.h>
#include <ix/cpu.h>
#include <ix/page.h>
#include <ix/vm.h>
#include <ix/kstats.h>
#include <ix/queue.h>

#include <dune.h>

#define UARR_MIN_CAPACITY	8192

DEFINE_PERCPU(struct bsys_arr *, usys_arr);
DEFINE_PERCPU(void *, usys_iomap);

static const int usys_nr = div_up(sizeof(struct bsys_arr) +
				  UARR_MIN_CAPACITY * sizeof(struct bsys_desc),
				  PGSIZE_2MB);

static bsysfn_t bsys_tbl[] = {
	(bsysfn_t) bsys_udp_send,
	(bsysfn_t) bsys_udp_sendv,
	(bsysfn_t) bsys_udp_recv_done,
};

static int bsys_dispatch_one(struct bsys_desc __user *d)
{
	uint64_t sysnr, arga, argb, argc, argd;

	sysnr = uaccess_peekq(&d->sysnr);
	arga = uaccess_peekq(&d->arga);
	argb = uaccess_peekq(&d->argb);
	argc = uaccess_peekq(&d->argc);
	argd = uaccess_peekq(&d->argd);

	if (unlikely(uaccess_check_fault()))
		return -EFAULT;
	if (unlikely(sysnr >= KSYS_NR))
		return -ENOSYS;

	bsys_tbl[sysnr](arga, argb, argc, argd);

	return 0;
}

static int bsys_dispatch(struct bsys_desc __user *d, unsigned int nr)
{
	unsigned long i;
	int ret;

	if (!nr)
		return 0;
	if (unlikely(!uaccess_okay(d, sizeof(struct bsys_desc) * nr)))
		return -EFAULT;

	for (i = 0; i < nr; i++) {
		ret = bsys_dispatch_one(&d[i]);
		if (unlikely(ret))
			return ret;
	}

	return 0;
}

/**
 * sys_bpoll - performs I/O processing and issues a batch of system calls
 * @d: the batched system call descriptor array
 * @nr: the number of batched system calls
 *
 * Returns 0 if successful, otherwise failure.
 */
static int sys_bpoll(struct bsys_desc __user *d, unsigned int nr)
{
	int ret;
	unsigned int i;

	usys_reset();

	KSTATS_PUSH(timer, NULL);
	timer_run();
	KSTATS_POP(NULL);

	KSTATS_PUSH(tx_reclaim, NULL);
	percpu_get(tx_batch_cap) = eth_tx_reclaim(eth_tx);
	KSTATS_POP(NULL);

	KSTATS_PUSH(rx_poll, NULL);
	for_each_queue(i)
		eth_rx_poll(eth_rx[i]);
	KSTATS_POP(NULL);

	KSTATS_PUSH(bsys, NULL);
	ret = bsys_dispatch(d, nr);
	KSTATS_POP(NULL);

	KSTATS_PUSH(tx_xmit, NULL);
	eth_tx_xmit(eth_tx, percpu_get(tx_batch_len), percpu_get(tx_batch));
	percpu_get(tx_batch_len) = 0;
	KSTATS_POP(NULL);

	return ret;
}

/**
 * sys_bcall - issues a batch of system calls
 * @d: the batched system call descriptor array
 * @nr: the number of batched system calls
 *
 * Returns 0 if successful, otherwise failure.
 */
static int sys_bcall(struct bsys_desc __user *d, unsigned int nr)
{
	return bsys_dispatch(d, nr);
}

/**
 * sys_baddr - get the address of the batched syscall array
 *
 * Returns an IOMAP pointer.
 */
static void *sys_baddr(void)
{
	return percpu_get(usys_iomap);
}

/**
 * sys_mmap - maps pages of memory into userspace
 * @addr: the start address of the mapping
 * @nr: the number of pages
 * @size: the size of each page
 * @perm: the permission of the mapping
 *
 * Returns 0 if successful, otherwise fail.
 */
static int sys_mmap(void *addr, int nr, int size, int perm)
{
	void *pages;
	int ret;

	/*
	 * FIXME: so far we can only support 2MB pages, but we should
	 * definitely add at least 1GB page support in the future.
	 */
	if (size != PGSIZE_2MB)
		return -ENOTSUP;

	/* make sure the address lies in the IOMAP region */
	if ((uintptr_t) addr < MEM_USER_IOMAPM_BASE_ADDR ||
	    (uintptr_t) addr + nr * size >= MEM_USER_IOMAPM_END_ADDR)
		return -EINVAL;

	pages = page_alloc_contig(nr);
	if (!pages)
		return -ENOMEM;

	perm |= VM_PERM_U;

	spin_lock(&vm_lock);
	if (__vm_is_mapped(addr, nr * size)) {
		spin_unlock(&vm_lock);
		ret = -EBUSY;
		goto fail;
	}
	if ((ret = __vm_map_phys((physaddr_t) pages, (virtaddr_t) addr,
				 nr, size, perm))) {
		spin_unlock(&vm_lock);
		goto fail;
	}
	spin_unlock(&vm_lock);

	return 0;

fail:
	page_free_contig(addr, nr);
	return ret;
}

/**
 * sys_unmap - unmaps pages of memory from userspace
 * @addr: the start address of the mapping
 * @nr: the number of pages
 * @size: the size of each page
 */
static int sys_unmap(void *addr, int nr, int size)
{
	if (size != PGSIZE_2MB)
		return -ENOTSUP;

	/* make sure the address lies in the IOMAP region */
	if ((uintptr_t) addr < MEM_USER_IOMAPM_BASE_ADDR ||
	    (uintptr_t) addr + nr * size >= MEM_USER_IOMAPM_END_ADDR)
		return -EINVAL;

	vm_unmap(addr, nr, size);
	return 0;
}

typedef uint64_t (*sysfn_t) (uint64_t, uint64_t, uint64_t,
			     uint64_t, uint64_t, uint64_t);

static sysfn_t sys_tbl[] = {
	(sysfn_t) sys_bpoll,
	(sysfn_t) sys_bcall,
	(sysfn_t) sys_baddr,
	(sysfn_t) sys_mmap,
	(sysfn_t) sys_unmap,
};

/**
 * do_syscall - the main IX system call handler routine
 * @tf: the Dune trap frame
 * @sysnr: the system call number
 */
void do_syscall(struct dune_tf *tf, uint64_t sysnr)
{
	if (unlikely(sysnr >= SYS_NR)) {
		tf->rax = (uint64_t) -ENOSYS;
		return;
	}

	tf->rax = (uint64_t) sys_tbl[sysnr](tf->rdi, tf->rsi, tf->rdx,
					    tf->rcx, tf->r8, tf->r9);
}

/**
 * syscall_init_cpu - creates a user-mapped page for batched system calls
 *
 * Returns 0 if successful, otherwise fail.
 */
int syscall_init_cpu(void)
{
	struct bsys_arr *arr;
	void *iomap;

	arr = (struct bsys_arr *) page_alloc_contig(usys_nr);
	if (!arr)
		return -ENOMEM;

	iomap = vm_map_to_user((void *) arr, usys_nr, PGSIZE_2MB, VM_PERM_R);
	if (!iomap) {
		page_free_contig((void *) arr, usys_nr);
		return -ENOMEM;
	}

	percpu_get(usys_arr) = arr;
	percpu_get(usys_iomap) = iomap;
	return 0;
}

/**
 * syscall_exit_cpu - frees the user-mapped page for batched system calls
 */
void syscall_exit_cpu(void)
{
	vm_unmap(percpu_get(usys_iomap), usys_nr, PGSIZE_2MB);
	page_free_contig((void *) percpu_get(usys_arr), usys_nr);
	percpu_get(usys_arr) = NULL;
	percpu_get(usys_iomap) = NULL;
}

