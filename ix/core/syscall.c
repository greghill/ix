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

#include <dune.h>

typedef uint64_t (*bsysfn_t) (uint64_t, uint64_t, uint64_t, uint64_t);

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

typedef uint64_t (*sysfn_t) (uint64_t, uint64_t, uint64_t,
			     uint64_t, uint64_t, uint64_t);

static sysfn_t sys_tbl[] = {
	(sysfn_t) sys_bpoll,
	(sysfn_t) sys_bcall,
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

