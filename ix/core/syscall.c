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

	if (unlikely(sysnr >= KSYS_NR)) {
		sysnr = (uint64_t) -ENOSYS;
		goto out;
	}

	sysnr = bsys_tbl[sysnr](arga, argb, argc, argd);

out:
	uaccess_pokeq(&d->sysnr, sysnr);
	if (unlikely(uaccess_check_fault()))
		return -EFAULT;

	return 0;
}

static int bsys_dispatch(struct bsys_desc __user *d[], unsigned int nr)
{
	unsigned long i;
	int ret;

	if (!nr)
		return 0;
	if (!uaccess_okay(d, sizeof(struct bsys_desc) * nr))
		return -EFAULT;

	for (i = 0; i < nr; i++) {
		ret = bsys_dispatch_one(d[i]);
		if (ret)
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
int sys_bpoll(struct bsys_desc __user *d[], unsigned int nr)
{
	int ret;

	timer_run();
	eth_tx_reclaim(eth_tx);
	usys_reset();
	eth_rx_poll(eth_rx);

	ret = bsys_dispatch(d, nr);

	/* FIXME: transmit waiting packets */

	return ret;
}

/**
 * sys_bcall - issues a batch of system calls
 * @d: the batched system call descriptor array
 * @nr: the number of batched system calls
 *
 * Returns 0 if successful, otherwise failure.
 */
int sys_bcall(struct bsys_desc __user *d[], unsigned int nr)
{
	return bsys_dispatch(d, nr);
}

/**
 * sys_baddr - get the address of the batched syscall array
 *
 * Returns an IOMAP pointer.
 */
void *sys_baddr(void)
{
	return percpu_get(usys_iomap);
}

typedef uint64_t (*sysfn_t) (uint64_t, uint64_t, uint64_t,
			     uint64_t, uint64_t, uint64_t);

static sysfn_t sys_tbl[] = {
	(sysfn_t) sys_bpoll,
	(sysfn_t) sys_bcall,
	(sysfn_t) sys_baddr,
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

