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
#include <ix/log.h>
#include <ix/control_plane.h>
#include <ix/ethfg.h>

#include <dune.h>

#define UARR_MIN_CAPACITY	8192

DEFINE_PERCPU(struct bsys_arr *, usys_arr);
DEFINE_PERCPU(void *, usys_iomap);
DEFINE_PERCPU(unsigned long, syscall_cookie);

static const int usys_nr = div_up(sizeof(struct bsys_arr) +
				  UARR_MIN_CAPACITY * sizeof(struct bsys_desc),
				  PGSIZE_2MB);

static bsysfn_t bsys_tbl[] = {
	(bsysfn_t) bsys_udp_send,
	(bsysfn_t) bsys_udp_sendv,
	(bsysfn_t) bsys_udp_recv_done,
	(bsysfn_t) bsys_tcp_connect,
	(bsysfn_t) bsys_tcp_accept,
	(bsysfn_t) bsys_tcp_reject,
	(bsysfn_t) bsys_tcp_send,
	(bsysfn_t) bsys_tcp_sendv,
	(bsysfn_t) bsys_tcp_recv_done,
	(bsysfn_t) bsys_tcp_close,
};

static int bsys_dispatch_one(struct bsys_desc __user *d)
{
	uint64_t sysnr, arga, argb, argc, argd, ret;

	sysnr = uaccess_peekq(&d->sysnr);
	arga = uaccess_peekq(&d->arga);
	argb = uaccess_peekq(&d->argb);
	argc = uaccess_peekq(&d->argc);
	argd = uaccess_peekq(&d->argd);

	if (unlikely(uaccess_check_fault()))
		return -EFAULT;
	if (unlikely(sysnr >= KSYS_NR)) {
		ret = (uint64_t) -ENOSYS;
		goto out;
	}

	ret = bsys_tbl[sysnr](arga, argb, argc, argd);

out:
	arga = percpu_get(syscall_cookie);
	uaccess_pokeq(&d->arga, arga);
	uaccess_pokeq(&d->argb, ret);
	if (unlikely(uaccess_check_fault()))
		return -EFAULT;

	return 0;
}

static int bsys_dispatch(struct bsys_desc __user *d, unsigned int nr)
{
	unsigned long i;
	int ret;
#ifdef ENABLE_KSTATS
	kstats_accumulate save;
#endif

	if (!nr)
		return 0;
	if (unlikely(!uaccess_okay(d, sizeof(struct bsys_desc) * nr)))
		return -EFAULT;

	for (i = 0; i < nr; i++) {
		KSTATS_PUSH(bsys_dispatch_one, &save);
		ret = bsys_dispatch_one(&d[i]);
		KSTATS_POP(&save);
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
	int ret, empty;

	usys_reset();

	KSTATS_PUSH(tx_reclaim, NULL);
	eth_process_reclaim();
	KSTATS_POP(NULL);

	KSTATS_PUSH(bsys, NULL);
	ret = bsys_dispatch(d, nr);
	KSTATS_POP(NULL);

	if (ret)
		return ret;

again:
	switch (percpu_get(cp_cmd)->cmd_id) {
		case CP_CMD_MIGRATE_FLOW_GROUP:
			eth_fg_assign_to_cpu(percpu_get(cp_cmd)->migrate_flow_group.flow, percpu_get(cp_cmd)->migrate_flow_group.cpu);
			percpu_get(cp_cmd)->cmd_id = CP_CMD_NOP;
			break;
		case CP_CMD_NOP:
			break;
	}

	KSTATS_PUSH(percpu_bookkeeping, NULL);
	cpu_do_bookkeeping();
	KSTATS_POP(NULL);

	KSTATS_PUSH(timer, NULL);
	timer_run();
	KSTATS_POP(NULL);

	KSTATS_PUSH(rx_poll, NULL);
	eth_process_poll();
	KSTATS_POP(NULL);

	KSTATS_PUSH(rx_recv, NULL);
	empty = eth_process_recv();
	KSTATS_POP(NULL);

	KSTATS_PUSH(tx_send, NULL);
	eth_process_send();
	KSTATS_POP(NULL);

	if (!percpu_get(usys_arr)->len) {
		if (empty) {
			uint64_t deadline = timer_deadline(10 * ONE_MS);
			if (deadline > 0) { 
				KSTATS_PUSH(idle, NULL);
				eth_rx_idle_wait(deadline);
				KSTATS_POP(NULL);
			}
		}
		
		KSTATS_PUSH(tx_reclaim, NULL);
		eth_process_reclaim();
		KSTATS_POP(NULL);

		goto again;
	}

	return 0;
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
	int ret;

	KSTATS_PUSH(tx_reclaim, NULL);
	eth_process_reclaim();
	KSTATS_POP(NULL);

	KSTATS_PUSH(bsys, NULL);
	ret = bsys_dispatch(d, nr);
	KSTATS_POP(NULL);

	KSTATS_PUSH(tx_send, NULL);
	eth_process_send();
	KSTATS_POP(NULL);

	return ret;
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

bool sys_spawn_cores;

/**
 * sys_spawnmode - sets the spawn mode
 * @spawn_cores: the spawn mode. If true, calls to clone will bind
 * to IX cores. If false, calls to clone will spawn a regular linux
 * thread.
 *
 * Returns 0.
 */
static int sys_spawnmode(bool spawn_cores)
{
	sys_spawn_cores = spawn_cores;
	return 0;
}

/**
 * sys_nrcpus - returns the number of active CPUs
 */
static int sys_nrcpus(void)
{
	return cpus_active;
}

typedef uint64_t (*sysfn_t) (uint64_t, uint64_t, uint64_t,
			     uint64_t, uint64_t, uint64_t);

static sysfn_t sys_tbl[] = {
	(sysfn_t) sys_bpoll,
	(sysfn_t) sys_bcall,
	(sysfn_t) sys_baddr,
	(sysfn_t) sys_mmap,
	(sysfn_t) sys_unmap,
	(sysfn_t) sys_spawnmode,
	(sysfn_t) sys_nrcpus,
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

	KSTATS_POP(NULL);
	tf->rax = (uint64_t) sys_tbl[sysnr](tf->rdi, tf->rsi, tf->rdx,
					    tf->rcx, tf->r8, tf->r9);
	KSTATS_PUSH(user, NULL);
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

