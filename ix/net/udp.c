/*
 * udp.c - Unreliable Datagram Protocol (UDP) Support
 */

#include <ix/stddef.h>
#include <ix/byteorder.h>
#include <ix/errno.h>
#include <ix/log.h>
#include <ix/syscall.h>
#include <ix/uaccess.h>

#include <net/ip.h>
#include <net/udp.h>

#include "net.h"
#include "cfg.h"

#define UDP_PKT_SIZE		  \
	(sizeof(struct eth_hdr) + \
	 sizeof(struct ip_hdr)  + \
	 sizeof(struct udp_hdr))

#define UDP_MAX_LEN \
	(ETH_MTU - sizeof(struct ip_hdr) - sizeof(struct udp_hdr))

static DEFINE_PERCPU(struct mbuf *, udp_free_head);
static DEFINE_PERCPU(struct mbuf **, udp_free_pos);

void udp_input(struct mbuf *pkt, struct ip_hdr *iphdr, struct udp_hdr *udphdr)
{
	void *data = mbuf_nextd(udphdr, void *);
	uint16_t len = ntoh16(udphdr->len);
	struct ip_tuple *id;

	if (unlikely(!mbuf_enough_space(pkt, data, len))) {
		mbuf_free(pkt);
		return;
	}

#ifdef DEBUG
	struct ip_addr addr;
	char src[IP_ADDR_STR_LEN];
	char dst[IP_ADDR_STR_LEN];

	addr.addr = ntoh32(iphdr->src_addr.addr);
	ip_addr_to_str(&addr, src);
	addr.addr = ntoh32(iphdr->dst_addr.addr);
	ip_addr_to_str(&addr, dst);

	log_debug("udp: got UDP packet from '%s' to '%s',"
		  "source port %d, dest port %d, len %d\n",
		  src, dst, ntoh16(udphdr->src_port),
		  ntoh16(udphdr->dst_port), ntoh16(udphdr->len));
#endif /* DEBUG */

	/* reuse part of the header memory */
	id = mbuf_mtod(pkt, struct ip_tuple *);
	id->src_ip = ntoh32(iphdr->src_addr.addr);
	id->dst_ip = ntoh32(iphdr->dst_addr.addr);
	id->src_port = ntoh16(udphdr->src_port);
	id->dst_port = ntoh16(udphdr->dst_port);

	pkt->next = NULL;
	*percpu_get(udp_free_pos) = pkt;
	percpu_get(udp_free_pos) = &pkt->next;

	usys_udp_recv(mbuf_to_iomap(pkt, data), len, mbuf_to_iomap(pkt, id));
}

static int udp_output(struct mbuf *pkt, struct ip_tuple *id, size_t len)
{
	struct eth_hdr *ethhdr = mbuf_mtod(pkt, struct eth_hdr *);
	struct ip_hdr *iphdr = mbuf_nextd(ethhdr, struct ip_hdr *);
	struct udp_hdr *udphdr = mbuf_nextd(iphdr, struct udp_hdr *);
	size_t full_len = len + sizeof(struct udp_hdr);
	struct ip_addr dst_addr;
	struct mbuf *mbufs[1];

	dst_addr.addr = id->dst_ip;
	if (arp_lookup_mac(&dst_addr, &ethhdr->dhost))
		return -EAGAIN;

	ethhdr->shost = cfg_mac;
	ethhdr->type = hton16(ETHTYPE_IP);

	ip_setup_header(iphdr, IPPROTO_UDP,
			cfg_host_addr.addr, id->dst_ip, full_len);

	udphdr->src_port = hton16(id->src_port);
	udphdr->dst_port = hton16(id->dst_port);
	udphdr->len = hton16(full_len);
	udphdr->chksum = 0;

	pkt->len = UDP_PKT_SIZE;
	mbufs[0] = pkt;

	if (eth_tx_xmit(eth_tx, 1, mbufs) != 1)
		return -EIO;

	return 0;
}

/**
 * bsys_udp_send - send a UDP packet
 * @addr: the user-level payload address in memory
 * @len: the length of the payload
 * @id: the IP destination
 *
 * Returns the number of bytes sent, or < 0 if fail.
 */
void bsys_udp_send(void __user *addr, size_t len,
		      struct ip_tuple __user *id)
{
	struct ip_tuple tmp;
	struct mbuf *pkt;
	struct mbuf_iov *iovs;
	struct sg_entry ent;
	int ret;

	/* validate user input */
	if (unlikely(len > UDP_MAX_LEN)) {
		usys_udp_send_ret(-EINVAL);
		return;
	}
	if (unlikely(copy_from_user(id, &tmp, sizeof(struct ip_tuple)))) {
		usys_udp_send_ret(-EFAULT);
		return;
	}
	if (unlikely(!uaccess_zc_okay(addr, len))) {
		usys_udp_send_ret(-EFAULT);
		return;
	}

	pkt = mbuf_alloc_local();
	if (unlikely(!pkt)) {
		usys_udp_send_ret(-ENOBUFS);
		return;
	}

	iovs = mbuf_mtod_off(pkt, struct mbuf_iov *,
			     align_up(UDP_PKT_SIZE, sizeof(uint64_t)));
	pkt->iovs = iovs;
	ent.base = addr;
	ent.len = len;
	mbuf_iov_create(&iovs[0], &ent);
	pkt->nr_iov = 1;

	/*
	 * Handle the case of a crossed page boundary. There
	 * can only be one because of the MTU size.
	 */
	BUILD_ASSERT(UDP_MAX_LEN < PGSIZE_2MB);
	if (ent.len) {
		iovs[1].base = ent.base;
		iovs[1].maddr = page_get(ent.base);
		iovs[1].len = ent.len;
		pkt->nr_iov = 2;
	}

	ret = udp_output(pkt, &tmp, len);
	if (unlikely(ret)) {
		mbuf_free(pkt);
		usys_udp_send_ret(ret);
	}
}

void bsys_udp_sendv(struct sg_entry __user *ents, unsigned int nrents,
		    struct ip_tuple __user *id)
{
	usys_udp_send_ret(-ENOSYS);
}

/**
 * bsys_udp_recv_done - acknowledge received UDP packets
 * @count: the number of packets to acknowledge
 */
void bsys_udp_recv_done(uint64_t count)
{
	struct mbuf *pos = percpu_get(udp_free_head);

	while (count && pos) {
		struct mbuf *tmp = pos;
		pos = pos->next;
		count--;
		mbuf_free(tmp);
	}

	percpu_get(udp_free_head) = pos;
	if (!pos)
		percpu_get(udp_free_pos) = &percpu_get(udp_free_head);
}

/**
 * udp_init - initialize UDP support
 */
void udp_init(void)
{
	percpu_get(udp_free_pos) = &percpu_get(udp_free_head);
}

