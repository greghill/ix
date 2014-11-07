/*
 * arp.c - Address Resolution Protocol support
 *
 * See RFC 826 for more details.
 *
 * FIXME: currently we don't support RARP (RFC 903). It's low
 * priority since the protocol is obsolete, but it wouldn't be
 * hard to add at all.
 */

#include <ix/stddef.h>
#include <ix/errno.h>
#include <ix/list.h>
#include <ix/timer.h>
#include <ix/hash.h>
#include <ix/mempool.h>
#include <ix/log.h>
#include <ix/lock.h>
#include <ix/cfg.h>

#include <net/ethernet.h>
#include <net/ip.h>
#include <net/arp.h>

#include "net.h"

#define ARP_PKT_SIZE (sizeof(struct eth_hdr) +		\
		      sizeof(struct arp_hdr) +		\
		      sizeof(struct arp_hdr_ethip))

/* NOTE: this struct is exactly a cache line in size */
struct arp_entry {
	struct ip_addr		addr;
	uint32_t		pad;
	struct eth_addr		mac;
	uint8_t			flags;
	uint8_t			retries;
	struct timer		timer;
	struct hlist_node	link;
};

static DEFINE_SPINLOCK(arp_lock);

#define ARP_FLAG_RESOLVING	0x1
#define ARP_FLAG_VALID		0x2
#define ARP_FLAG_STATIC		0x4

#define ARP_REFRESH_TIMEOUT	10 * ONE_SECOND
#define ARP_RESOLVE_TIMEOUT	1 * ONE_SECOND
#define ARP_RETRY_TIMEOUT	1 * ONE_SECOND
#define ARP_MAX_ATTEMPTS	3

#define ARP_MAX_ENTRIES		65536
#define ARP_HASH_SEED		0xa36bdcbe

static struct mempool_datastore arp_datastore;
static struct mempool		arp_mempool;
static struct hlist_head	arp_tbl[ARP_MAX_ENTRIES];

static void arp_timer_handler(struct timer *t);

static inline int arp_ip_to_idx(struct ip_addr *addr)
{
	int idx = hash_crc32c_one(ARP_HASH_SEED, addr->addr);
	idx &= ARP_MAX_ENTRIES - 1;
	return idx;
}

static struct arp_entry *arp_lookup(struct ip_addr *addr, bool create_okay)
{
	struct arp_entry *e;
	struct hlist_node *pos;
	struct hlist_head *h = &arp_tbl[arp_ip_to_idx(addr)];

	hlist_for_each(h, pos) {
		e = hlist_entry(pos, struct arp_entry, link);
		if (e->addr.addr == addr->addr)
			return e;
	}

	if (create_okay) {
		spin_lock(&arp_lock);

		e = (struct arp_entry *)mempool_alloc(&arp_mempool);
		if (unlikely(!e))
			return NULL;

		e->addr.addr = addr->addr;
		e->flags = 0;
		e->retries = 0;
		timer_init_entry(&e->timer, &arp_timer_handler);
		hlist_add_head(h, &e->link);
		spin_unlock(&arp_lock);
		return e;
	}

	return NULL;
}

static int arp_update_mac(struct ip_addr *addr,
			  struct eth_addr *mac, bool create_okay)
{
	struct arp_entry *e = arp_lookup(addr, create_okay);
	if (unlikely(!e))
		return -ENOMEM;

	if (e->flags & ARP_FLAG_STATIC)
		return 0;

#ifdef DEBUG
	if (!(e->flags & ARP_FLAG_VALID)) {
		log_debug("arp: inserting table entry:\n");
		log_debug("\tIP:\t%d.%d.%d.%d\n",
			  ((addr->addr >> 24) & 0xff),
			  ((addr->addr >> 16) & 0xff),
			  ((addr->addr >> 8) & 0xff),
			  (addr->addr & 0xff));
		log_debug("\tMAC:\t%02X:%02X:%02X:%02X:%02X:%02X\n",
			  mac->addr[0], mac->addr[1], mac->addr[2],
			  mac->addr[3], mac->addr[4], mac->addr[5]);
	} else if (memcmp(&mac->addr, &e->mac.addr, ETH_ADDR_LEN)) {
		log_debug("arp: updating table entry:\n");
		log_debug("\tIP:\t%d.%d.%d.%d\n",
			  ((addr->addr >> 24) & 0xff),
			  ((addr->addr >> 16) & 0xff),
			  ((addr->addr >> 8) & 0xff),
			  (addr->addr & 0xff));
		log_debug("\t old MAC:\t%02X:%02X:%02X:%02X:%02X:%02X\n",
			  e->mac.addr[0], e->mac.addr[1], e->mac.addr[2],
			  e->mac.addr[3], e->mac.addr[4], e->mac.addr[5]);
		log_debug("\t new MAC:\t%02X:%02X:%02X:%02X:%02X:%02X\n",
			  mac->addr[0], mac->addr[1], mac->addr[2],
			  mac->addr[3], mac->addr[4], mac->addr[5]);
	}
#endif /* DEBUG */

	e->mac = *mac;
	e->flags = ARP_FLAG_VALID;
	e->retries = 0;
	timer_mod(&e->timer, ARP_REFRESH_TIMEOUT);

	return 0;
}

static int arp_send_pkt(uint16_t op,
			struct ip_addr *target_ip,
			struct eth_addr *target_mac)
{
	int ret;
	struct mbuf *pkt;
	struct eth_hdr *ethhdr;
	struct arp_hdr *arphdr;
	struct arp_hdr_ethip *ethip;

	pkt = mbuf_alloc_local();
	if (unlikely(!pkt))
		return -ENOMEM;

	ethhdr = mbuf_mtod(pkt, struct eth_hdr *);
	arphdr = mbuf_nextd(ethhdr, struct arp_hdr *);
	ethip = mbuf_nextd(arphdr, struct arp_hdr_ethip *);

	ethhdr->dhost = *target_mac;
	ethhdr->shost = cfg_mac;
	ethhdr->type = hton16(ETHTYPE_ARP);

	arphdr->htype = hton16(ARP_HTYPE_ETHER);
	arphdr->ptype = hton16(ETHTYPE_IP);
	arphdr->hlen = sizeof(struct eth_addr);
	arphdr->plen = sizeof(struct ip_addr);
	arphdr->op = hton16(op);

	ethip->sender_mac = cfg_mac;
	ethip->sender_ip.addr = hton32(cfg_host_addr.addr);
	ethip->target_mac = *target_mac;
	ethip->target_ip.addr = hton32(target_ip->addr);

	pkt->ol_flags = 0;

	/* FIXME: need an API to specify default TX queue */
//	set_current_queue(percpu_get(eth_rxqs[0]));
	ret = eth_send_one(percpu_get(eth_txqs)[0],pkt, ARP_PKT_SIZE);

	if (unlikely(ret)) {
		mbuf_free(pkt);
		return -EIO;
	}

	return 0;
}

static int arp_send_response_reuse(struct mbuf *pkt,
				   struct arp_hdr *arphdr,
				   struct arp_hdr_ethip *ethip)
{
	int ret;
	struct eth_hdr *ethhdr = mbuf_mtod(pkt, struct eth_hdr *);
	ethhdr->dhost = ethhdr->shost;
	arphdr->op = hton16(ARP_OP_REPLY);
	ethip->target_mac = ethip->sender_mac;
	ethip->target_ip.addr = ethip->sender_ip.addr;

	ethhdr->shost = cfg_mac;
	ethip->sender_ip.addr = hton32(cfg_host_addr.addr);
	ethip->sender_mac = cfg_mac;

	pkt->ol_flags = 0;
	/* FIXME - is this always queue 0? (EdB) */
	ret = eth_send_one(percpu_get(eth_txqs)[0],pkt, ARP_PKT_SIZE);

	if (unlikely(ret)) {
		mbuf_free(pkt);
		return -EIO;
	}

	return 0;
}

/**
 * arp_input - handles an ARP request from the network
 * @pkt: the packet
 * @hdr: the ARP header (inside the packet)
 */
void arp_input(struct mbuf *pkt, struct arp_hdr *hdr)
{
	int op;
	struct arp_hdr_ethip *ethip;
	struct ip_addr sender_ip, target_ip;
	bool am_target;

	if (!mbuf_enough_space(pkt, hdr, sizeof(struct arp_hdr)))
		goto out;

	/* make sure the arp header is valid */
	if (ntoh16(hdr->htype) != ARP_HTYPE_ETHER ||
	    ntoh16(hdr->ptype) != ETHTYPE_IP ||
	    hdr->hlen != sizeof(struct eth_addr) ||
	    hdr->plen != sizeof(struct ip_addr))
		goto out;

	/* now validate the variable length portion */
	ethip = mbuf_nextd(hdr, struct arp_hdr_ethip *);
	if (!mbuf_enough_space(pkt, ethip, sizeof(struct arp_hdr_ethip)))
		goto out;

	op = ntoh16(hdr->op);
	sender_ip.addr = ntoh32(ethip->sender_ip.addr);
	target_ip.addr = ntoh32(ethip->target_ip.addr);

	/* refuse ARP packets with multicast source MAC's */
	if (eth_addr_is_multicast(&ethip->sender_mac))
		goto out;

	am_target = (cfg_host_addr.addr == target_ip.addr);
	arp_update_mac(&sender_ip, &ethip->sender_mac, am_target);

	if (am_target && op == ARP_OP_REQUEST) {
		log_debug("arp: responding to arp request "
			  "from IP %d.%d.%d.%d\n",
			  ((sender_ip.addr >> 24) & 0xff),
			  ((sender_ip.addr >> 16) & 0xff),
			  ((sender_ip.addr >> 8) & 0xff),
			  (sender_ip.addr & 0xff));

		arp_send_response_reuse(pkt, hdr, ethip);
		return;
	}

out:
	mbuf_free(pkt);
}

/**
 * arp_lookup_mac - gives back a MAC value for a given IP address
 * @addr: the IP address to lookup
 * @mac: a buffer to store the MAC value
 *
 * Returns 0 if successful, -EAGAIN if waiting to resolve, otherwise fail.
 */
int arp_lookup_mac(struct ip_addr *addr, struct eth_addr *mac)
{
	struct arp_entry *e = arp_lookup(addr, true);
	if (!e)
		return -ENOENT;

	if (!(e->flags & ARP_FLAG_VALID)) {
		if (!timer_pending(&e->timer)) {
			struct eth_addr target = ETH_ADDR_BROADCAST;

			e->flags |= ARP_FLAG_RESOLVING;
			arp_send_pkt(ARP_OP_REQUEST, addr, &target);
			timer_add(&e->timer, ARP_RESOLVE_TIMEOUT);
		}
		return -EAGAIN;
	}

	*mac = e->mac;
	return 0;
}

/**
 * arp_insert - insert a static entry into the ARP table
 * @addr: the IP address to insert
 * @mac: the MAC address to insert
 *
 * Returns 0 if successful.
 */
int arp_insert(struct ip_addr *addr, struct eth_addr *mac)
{
	struct arp_entry *e;
	struct hlist_head *h;

	e = arp_lookup(addr, false);
	if (!e) {
		h = &arp_tbl[arp_ip_to_idx(addr)];
		e = (struct arp_entry *)mempool_alloc(&arp_mempool);
		if (unlikely(!e))
			return -ENOMEM;
		e->addr.addr = addr->addr;
		e->flags = ARP_FLAG_VALID | ARP_FLAG_STATIC;
		e->retries = 0;
		timer_init_entry(&e->timer, NULL);
		hlist_add_head(h, &e->link);
	}

	timer_del(&e->timer);
	e->mac = *mac;

	return 0;
}

static void arp_timer_handler(struct timer *t)
{
	struct arp_entry *e = container_of(t, struct arp_entry, timer);

	e->retries++;
	if (e->retries >= ARP_MAX_ATTEMPTS) {
		log_debug("arp: removing dead entry "
			  "IP %d.%d.%d.%d\n",
			  ((e->addr.addr >> 24) & 0xff),
			  ((e->addr.addr >> 16) & 0xff),
			  ((e->addr.addr >> 8) & 0xff),
			  (e->addr.addr & 0xff));

		hlist_del(&e->link);
		mempool_free(&arp_mempool, e);
		return;
	}

	e->flags |= ARP_FLAG_RESOLVING;

	if (e->flags & ARP_FLAG_VALID) {
		arp_send_pkt(ARP_OP_REQUEST, &e->addr, &e->mac);
	} else {
		struct eth_addr target = ETH_ADDR_BROADCAST;
		arp_send_pkt(ARP_OP_REQUEST, &e->addr, &target);
	}

	timer_add(t, ARP_RETRY_TIMEOUT);
}

/**
 * arp_init - initializes the ARP service
 */
int arp_init(void)
{
	int ret;
	ret = mempool_create_datastore(&arp_datastore,ARP_MAX_ENTRIES, sizeof(struct arp_entry), 0, MEMPOOL_DEFAULT_CHUNKSIZE,"arp");
	if (ret) 
		return ret;
	return mempool_create(&arp_mempool, &arp_datastore, MEMPOOL_SANITY_GLOBAL,0);
}

