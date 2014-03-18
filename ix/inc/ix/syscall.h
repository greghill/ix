/*
 * syscall.h - system call support (regular and batched)
 */

#pragma once

#include <ix/types.h>
#include <ix/cpu.h>

/*
 * System calls
 */
enum {
	SYS_BPOLL = 0,
	SYS_BCALL,
	SYS_NR,
};


/*
 * Data structures used as arguments.
 */

struct ip_tuple {
	uint32_t src_ip;
	uint32_t dst_ip;
	uint16_t src_port;
	uint16_t dst_port;
} __packed;

struct sg_entry {
	void *base;
	size_t len;
} __packed;


/*
 * Batched system calls
 */

struct bsys_desc {
	uint64_t sysnr;
	uint64_t arga, argb, argc, argd;
} __packed;

#define BSYS_DESC_NOARG(desc, vsysnr) \
	(desc)->sysnr = (uint64_t) (vsysnr)
#define BSYS_DESC_1ARG(desc, vsysnr, varga) \
	BSYS_DESC_NOARG(desc, vsysnr),      \
	(desc)->arga = (uint64_t) (varga)
#define BSYS_DESC_2ARG(desc, vsysnr, varga, vargb) \
	BSYS_DESC_1ARG(desc, vsysnr, varga),       \
	(desc)->argb = (uint64_t) (vargb)
#define BSYS_DESC_3ARG(desc, vsysnr, varga, vargb, vargc) \
	BSYS_DESC_2ARG(desc, vsysnr, varga, vargb),       \
	(desc)->argc = (uint64_t) (vargc)
#define BSYS_DESC_4ARG(desc, vsysnr, varga, vargb, vargc, vargd) \
	BSYS_DESC_3ARG(desc, vsysnr, varga, vargb, vargc),       \
	(desc)->argd = (uint64_t) (vargd)

struct bsys_arr {
	unsigned long len;
	unsigned long max_len;
	struct bsys_desc descs[];
};

/**
 * bsys_arr_next - get the next free descriptor
 * @a: the syscall array
 *
 * Returns a descriptor, or NULL if none are available.
 */
static inline struct bsys_desc *__bsys_arr_next(struct bsys_arr *a)
{
	return &a->descs[a->len++];
}

/**
 * bsys_arr_next - get the next free descriptor, checking for overflow
 * @a: the syscall array
 *
 * Returns a descriptor, or NULL if none are available.
 */
static inline struct bsys_desc *bsys_arr_next(struct bsys_arr *a)
{
	if (a->len >= a->max_len)
		return NULL;

	return __bsys_arr_next(a);
}


/*
 * Commands that can be sent from the user-level application to the kernel.
 */

enum {
	KSYS_UDP_SEND = 0,
	KSYS_UDP_SENDV,
	KSYS_UDP_RECV_DONE,
	KSYS_NR,
};

/**
 * ksys_udp_send - transmits a UDP packet
 * @d: the syscall descriptor to program
 * @addr: the address of the packet data
 * @len: the length of the packet data
 * @id: the UDP 4-tuple
 */
static inline void ksys_udp_send(struct bsys_desc *d, void *addr,
				 size_t len, struct ip_tuple *id)
{
	BSYS_DESC_3ARG(d, KSYS_UDP_SEND, addr, len, id);
}

/**
 * ksys_udp_sendv - transmits a UDP packet using scatter-gather data
 * @d: the syscall descriptor to program
 * @ents: the scatter-gather vector array
 * @nrents: the number of scatter-gather vectors
 * @id: the UDP 4-tuple
 */
static inline void ksys_udp_sendv(struct bsys_desc *d,
				  struct sg_entry *ents[],
				  unsigned int nrents, struct ip_tuple *id)
{
	BSYS_DESC_3ARG(d, KSYS_UDP_SENDV, ents, nrents, id);
}

/**
 * ksys_udp_recv_done - acknowledge the receipt of UDP packets
 * @d: the syscall descriptor to program
 * @count: the number of packet recieves to acknowledge
 *
 * NOTE: Calling this function allows the kernel to free mbuf's when
 * the application has finished using them. Acknowledgements are always
 * in FIFO order.
 */
static inline void ksys_udp_recv_done(struct bsys_desc *d, uint64_t count)
{
	BSYS_DESC_1ARG(d, KSYS_UDP_RECV_DONE, count);
}


/*
 * Commands that can be sent from the kernel to the user-level application.
 */

enum {
	USYS_UDP_RECV = 0,
	USYS_UDP_SEND_DONE,
	USYS_NR,
};

#ifdef __KERNEL__

DECLARE_PERCPU(struct bsys_arr *, usys_arr);

/**
 * usys_reset - reset the batched call array
 *
 * Call this before batching system calls.
 */
static inline void usys_reset(void)
{
	percpu_get(usys_arr)->len = 0;
}

/**
 * usys_next - get the next batched syscall descriptor
 *
 * Returns a syscall descriptor.
 */
static inline struct bsys_desc *usys_next(void)
{
	return __bsys_arr_next(percpu_get(usys_arr));
}

/**
 * usys_udp_recv - receive a UDP packet
 * @addr: the address of the packet data
 * @len: the length of the packet data
 * @id: the UDP 4-tuple
 */
static inline void usys_udp_recv(void *addr, size_t len, struct ip_tuple *id)
{
	struct bsys_desc *d = usys_next();
	BSYS_DESC_3ARG(d, USYS_UDP_RECV, addr, len, id);
}

/**
 * usys_udp_send_done - acknowledge the completion of UDP packet sends
 * @count: the number of packet sends that have completed
 *
 * NOTE: Calling this function allows the user application to unpin memory
 * that was locked for zero copy transfer. Acknowledgements are always in
 * FIFO order.
 */
static inline void usys_udp_send_done(uint64_t count)
{
	struct bsys_desc *d = usys_next();
	BSYS_DESC_1ARG(d, USYS_UDP_SEND_DONE, count);
}


/*
 * Kernel system call definitions
 */

/* FIXME: could use Sparse to statically check */
#define __user

extern ssize_t bsys_udp_send(void __user *addr, size_t len, struct ip_tuple __user *id);
extern ssize_t bsys_udp_sendv(struct sg_entry __user *ents[], unsigned int nrents,
			      struct ip_tuple __user *id);
extern int bsys_udp_recv_done(uint64_t count);

extern int sys_bpoll(struct bsys_desc __user *d[], unsigned int nr);
extern int sys_bcall(struct bsys_desc __user *d[], unsigned int nr);

struct dune_tf *tf;
extern void do_syscall(struct dune_tf *tf, uint64_t sysnr);

extern int syscall_init_cpu(void);
extern void syscall_exit_cpu(void);

#endif /* __KERNEL__ */

