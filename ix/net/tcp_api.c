/*
 * tcp_api.c - plumbing between the TCP and userspace
 */

#include <ix/stddef.h>
#include <ix/errno.h>
#include <ix/syscall.h>
#include <ix/queue.h>
#include <ix/log.h>
#include <ix/uaccess.h>
#include <ix/ethdev.h>
#include <ix/toeplitz.h>
#include <ix/kstats.h>

#include <lwip/tcp.h>

#include "cfg.h"

#define MAX_PCBS	65536

static DEFINE_PERQUEUE(struct tcp_pcb *, listen_pcb);
/* FIXME: this should be probably per queue */
static DEFINE_PERCPU(uint16_t, local_port);
/* FIXME: this should be more adaptive to various configurations */
#define PORTS_PER_CPU (65536 / 32)

/*
 * FIXME: LWIP and IX have different lifetime rules so we have to maintain
 * a seperate pcb. Otherwise, we'd be plagued by use-after-free problems.
 */
struct tcpapi_pcb {
	unsigned long alive; /* FIXME: this overlaps with mempool_hdr so
			      * we can tell if this pcb is allocated or not. */
	struct tcp_pcb *pcb;
	unsigned long cookie;
	struct ip_tuple *id;
	hid_t handle;
	struct pbuf *recvd;
	struct pbuf *recvd_tail;
	int queue;
	bool accepted;
};

static DEFINE_PERQUEUE(struct mempool, pcb_mempool);
static DEFINE_PERQUEUE(struct mempool, id_mempool);

/**
 * handle_to_tcpapi - converts a handle to a PCB
 * @handle: the input handle
 *
 * Return a PCB, or NULL if the handle is invalid.
 */
static inline struct tcpapi_pcb *handle_to_tcpapi(hid_t handle)
{
	struct mempool *p;
	struct tcpapi_pcb *api;
	int queue = ((handle >> 48) & 0xffff);
	unsigned long idx = (handle & 0xffffffffffff);

	if (unlikely(queue >= NQUEUE))
		return NULL;
	if (unlikely(idx >= MAX_PCBS))
		return NULL;

	set_current_queue(percpu_get(eth_rx)[queue]);
	p = &perqueue_get(pcb_mempool);

	api = (struct tcpapi_pcb *) ((uintptr_t) p->buf +
				     idx * sizeof(struct tcpapi_pcb));

	/* check if the handle is actually allocated */
	if (unlikely(api->alive > 1))
		return NULL;

	percpu_get(syscall_cookie) = api->cookie;

	return api;
}

/**
 * tcpapi_to_handle - converts a PCB to a handle
 * @pcb: the PCB.
 *
 * Returns a handle.
 */
static inline hid_t tcpapi_to_handle(struct tcpapi_pcb *pcb)
{
	struct mempool *p = &perqueue_get(pcb_mempool);

	return (((uintptr_t) pcb - (uintptr_t) p->buf) / sizeof(struct tcpapi_pcb)) |
	       ((uintptr_t) (perqueue_get(queue_id)) << 48);
}

static void recv_a_pbuf(struct tcpapi_pcb *api, struct pbuf *p)
{
	struct mbuf *pkt;
	 /* Walk through the full receive chain */
	do {
		pkt = p->mbuf;
		pkt->len = p->len; /* repurpose len for recv_done */
		usys_tcp_recv(api->handle, api->cookie,
			      mbuf_to_iomap(pkt, p->payload), p->len);

		p = p->next;
	} while (p);
}

long bsys_tcp_accept(hid_t handle, unsigned long cookie)
{
	/*
	 * FIXME: this function is sort of a placeholder since we have no
	 * choice but to have already accepted the connection under LWIP's
	 * synchronous API.
	 */

	struct tcpapi_pcb *api = handle_to_tcpapi(handle);
	struct pbuf *tmp;

	KSTATS_VECTOR(bsys_tcp_accept);

	log_debug("tcpapi: bsys_tcp_accept() - handle %lx, cookie %lx\n",
		  handle, cookie);

	if (unlikely(!api)) {
		log_debug("tcpapi: invalid handle\n");
		return -RET_BADH;
	}

	if (api->id) {
		mempool_free(&perqueue_get(id_mempool), api->id);
		api->id = NULL;
	}

	api->cookie = cookie;
	api->accepted = true;

	tmp = api->recvd;
	while (tmp) {
		recv_a_pbuf(api, tmp);
		tmp = tmp->tcp_api_next;
	}

	return RET_OK;
}

long bsys_tcp_reject(hid_t handle)
{
	/*
	 * FIXME: LWIP's synchronous handling of accepts
	 * makes supporting this call impossible.
	 */

	KSTATS_VECTOR(bsys_tcp_reject);

	log_err("tcpapi: bsys_tcp_reject() is not implemented\n");

	return -RET_NOTSUP;
}

ssize_t bsys_tcp_send(hid_t handle, void *addr, size_t len)
{
	KSTATS_VECTOR(bsys_tcp_send);

	log_debug("tcpapi: bsys_tcp_send() - addr %p, len %lx\n",
		  addr, len);

	return -RET_NOTSUP;
}

ssize_t bsys_tcp_sendv(hid_t handle, struct sg_entry __user *ents,
		       unsigned int nrents)
{
	struct tcpapi_pcb *api = handle_to_tcpapi(handle);
	int i;
	size_t len_xmited = 0;

	KSTATS_VECTOR(bsys_tcp_sendv);

	log_debug("tcpapi: bsys_tcp_sendv() - handle %lx, ents %p, nrents %ld\n",
		  handle, ents, nrents);

	if (unlikely(!api)) {
		log_debug("tcpapi: invalid handle\n");
		return -RET_BADH;
	}

	if (unlikely(!api->alive))
		return -RET_CLOSED;

	if (unlikely(!uaccess_okay(ents, nrents * sizeof(struct sg_entry))))
		return -RET_FAULT;

	nrents = min(nrents, MAX_SG_ENTRIES);
	for (i = 0; i < nrents; i++) {
		err_t err;
		void *base = (void *) uaccess_peekq((uint64_t *) &ents[i].base);
		size_t len = uaccess_peekq(&ents[i].len);
		bool buf_full = len > min(api->pcb->snd_buf, 0xFFFF);

		if (unlikely(!uaccess_okay(base, len)))
			break;

		/*
		 * FIXME: hacks to deal with LWIP's send buffering
		 * design when handling large send requests. LWIP
		 * buffers send data but in IX we don't want any
		 * buffering in the kernel at all. Thus, the real
		 * limit here should be the TCP cwd. Unfortunately
		 * tcp_out.c needs to be completely rewritten to
		 * support this.
		 */
		if (buf_full)
			len = min(api->pcb->snd_buf, 0xFFFF);
		if (!len)
			break;

		/*
		 * FIXME: Unfortunately LWIP's TX path is compeletely
		 * broken in terms of zero-copy. It's also somewhat
		 * broken in terms of large write requests. Here's a
		 * hacky placeholder until we can rewrite this path.
		 */
		err = tcp_write(api->pcb, base, len, 0);
		if (err != ERR_OK)
			break;

		len_xmited += len;
		if (buf_full)
			break;
	}

	if (len_xmited)
		tcp_output(api->pcb);

	return len_xmited;
}

long bsys_tcp_recv_done(hid_t handle, size_t len)
{
	struct tcpapi_pcb *api = handle_to_tcpapi(handle);
	struct pbuf *recvd, *next;

	KSTATS_VECTOR(bsys_tcp_recv_done);

	log_debug("tcpapi: bsys_tcp_recv_done - handle %lx, len %ld\n",
		  handle, len);

	if (unlikely(!api)) {
		log_debug("tcpapi: invalid handle\n");
		return -RET_BADH;
	}

	recvd = api->recvd;

	if (api->pcb)
		tcp_recved(api->pcb, len);
	while (recvd) {
		if (len < recvd->len)
			break;

		len -= recvd->len;
		next = recvd->tcp_api_next;
		pbuf_free(recvd);
		recvd = next;
	}

	api->recvd = recvd;
	return RET_OK;
}

long bsys_tcp_close(hid_t handle)
{
	struct tcpapi_pcb *api = handle_to_tcpapi(handle);
	struct pbuf *recvd, *next;

	KSTATS_VECTOR(bsys_tcp_close);

	log_debug("tcpapi: bsys_tcp_close - handle %lx\n", handle);

	if (unlikely(!api)) {
		log_debug("tcpapi: invalid handle\n");
		return -RET_BADH;
	}

	if (api->pcb) {
		tcp_close_with_reset(api->pcb);
	}

	recvd = api->recvd;
	while (recvd) {
		next = recvd->tcp_api_next;
		pbuf_free(recvd);
		recvd = next;
	}

	if (api->id) {
		mempool_free(&perqueue_get(id_mempool), api->id);
	}

	mempool_free(&perqueue_get(pcb_mempool), api);
	return RET_OK;
}

static void mark_dead(struct tcpapi_pcb *api, unsigned long cookie)
{
	if (!api) {
		usys_tcp_dead(0, cookie);
		return;
	}

	api->alive = false;
	api->pcb = NULL;
	usys_tcp_dead(api->handle, api->cookie);
}

static err_t on_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
	struct tcpapi_pcb *api;

	log_debug("tcpapi: on_recv - arg %p, pcb %p, pbuf %p, err %d\n",
		  arg, pcb, p, err);

	api = (struct tcpapi_pcb *) arg;

	/* FIXME: It's not really clear what to do with "err" */

	/* Was the connection closed? */
	if (!p) {
		mark_dead(api, api->cookie);
		return ERR_OK;
	}

	if (!api->recvd) {
		api->recvd = p;
		api->recvd_tail = p;
	} else {
		api->recvd_tail->tcp_api_next = p;
		api->recvd_tail = p;
	}
	p->tcp_api_next = NULL;

	/*
	 * FIXME: This is a pretty annoying hack. LWIP accepts connections
	 * synchronously while we have to wait for the app to accept the
	 * connection. As a result, we have no choice but to assume the
	 * connection will be accepted. Thus, we may start receiving data
	 * packets before the app has allocated a recieve context and set
	 * the appropriate cookie value. For now we wait for the app to
	 * accept the connection before we allow receive events to be
	 * sent. Clearly, the receive path needs to be rewritten.
 	 */
	if (!api->accepted)
		goto done;

	recv_a_pbuf(api, p);

done:
	return ERR_OK;
}

static void on_err(void *arg, err_t err)
{
	struct tcpapi_pcb *api;
	unsigned long cookie;

	log_debug("tcpapi: on_err - arg %p err %d\n", arg, err);

	api = (struct tcpapi_pcb *) arg;
	cookie = api->cookie;

	if (err == ERR_ABRT || err == ERR_RST || err == ERR_CLSD)
		mark_dead(api, cookie);
}

static err_t on_sent(void *arg, struct tcp_pcb *pcb, u16_t len)
{
	struct tcpapi_pcb *api;

	log_debug("tcpapi: on_sent - arg %p, pcb %p, len %hd\n",
		  arg, pcb, len);

	api = (struct tcpapi_pcb *) arg;
	usys_tcp_sent(api->handle, api->cookie, len);

	return ERR_OK;
}

static err_t on_accept(void *arg, struct tcp_pcb *pcb, err_t err)
{
	struct tcpapi_pcb *api;
	struct ip_tuple *id;
	hid_t handle;

	log_debug("tcpapi: on_accept - arg %p, pcb %p, err %d\n",
		  arg, pcb, err);

	api = mempool_alloc(&perqueue_get(pcb_mempool));
	if (unlikely(!api))
		return ERR_MEM;

	id = mempool_alloc(&perqueue_get(id_mempool));
	if (unlikely(!id)) {
		mempool_free(&perqueue_get(pcb_mempool), api);
		return ERR_MEM;
	}

	api->pcb = pcb;
	api->alive = true;
	api->cookie = 0;
	api->recvd = NULL;
	api->recvd_tail = NULL;
	api->accepted = false;

	tcp_nagle_disable(pcb);
	tcp_arg(pcb, api);
	tcp_recv(pcb, on_recv);
	tcp_err(pcb, on_err);
	tcp_sent(pcb, on_sent);
	tcp_accepted(perqueue_get(listen_pcb));

	id->src_ip = 0; /* FIXME: LWIP doesn't provide this information :( */
	id->dst_ip = cfg_host_addr.addr;
	id->src_port = pcb->local_port;
	id->dst_port = pcb->remote_port;
	api->id = id;

	handle = tcpapi_to_handle(api);
	api->handle = handle;
	id = (struct ip_tuple *)
		mempool_pagemem_to_iomap(&perqueue_get(id_mempool), id);

	usys_tcp_knock(handle, id);

	return ERR_OK;
}

static err_t on_connected(void *arg, struct tcp_pcb *pcb, err_t err)
{
	struct tcpapi_pcb *api = (struct tcpapi_pcb *) arg;

	if (err != ERR_OK) {
		log_err("tcpapi: connection failed, ret %d\n", err);
		/* FIXME: free memory and mark handle dead */
		usys_tcp_connected(api->handle, api->cookie, RET_CONNREFUSED);
		return err;
	}

	usys_tcp_connected(api->handle, api->cookie, RET_OK);
	return ERR_OK;
}

/* FIXME: we should maintain a bitmap to hold the available TCP ports */
int get_local_port_and_set_queue(struct ip_tuple *id)
{
	int i;
	uint32_t hash;
	uint32_t queue_idx;

	if (!percpu_get(local_port))
		percpu_get(local_port) = percpu_get(cpu_id) * PORTS_PER_CPU;
	while (1) {
		percpu_get(local_port)++;
		if (percpu_get(local_port) >= (percpu_get(cpu_id) + 1) * PORTS_PER_CPU)
			percpu_get(local_port) = percpu_get(cpu_id) * PORTS_PER_CPU + 1;
		hash = toeplitz_rawhash_addrport(id->src_ip, id->dst_ip, hton16(percpu_get(local_port)), hton16(id->dst_port));
		queue_idx = hash & (ETH_RSS_RETA_MAX_QUEUE - 1);
		if (percpu_get(eth_rx_bitmap) & (1 << queue_idx)) {
			id->src_port = percpu_get(local_port);
			for (i = 0; i < percpu_get(eth_rx_count); i++) {
				if (percpu_get(eth_rx)[i]->queue_idx == queue_idx) {
					set_current_queue(percpu_get(eth_rx)[i]);
					break;
				}
			}
			return 0;
		}
	}

	return 1;
}

long bsys_tcp_connect(struct ip_tuple __user *id, unsigned long cookie)
{
	err_t err;
	struct ip_tuple tmp;
	struct ip_addr addr;
	struct tcp_pcb *pcb;
	struct tcpapi_pcb *api;

	KSTATS_VECTOR(bsys_tcp_connect);

	log_debug("tcpapi: bsys_tcp_connect() - id %p, cookie %lx\n",
		  id, cookie);

	percpu_get(syscall_cookie) = cookie;

        if (unlikely(copy_from_user(id, &tmp, sizeof(struct ip_tuple)))) {
                return -RET_FAULT;
        }

	tmp.src_ip = hton32(cfg_host_addr.addr);

	if (unlikely(get_local_port_and_set_queue(&tmp))) {
                return -RET_FAULT;
	}

	pcb = tcp_new();
	if (unlikely(!pcb))
		goto pcb_fail;
	tcp_nagle_disable(pcb);

	api = mempool_alloc(&perqueue_get(pcb_mempool));
	if (unlikely(!api)) {
		goto connect_fail;
	}

	api->pcb = pcb;
	api->alive = true;
	api->cookie = cookie;
	api->recvd = NULL;
	api->recvd_tail = NULL;
	api->accepted = true;

	tcp_arg(pcb, api);

	api->handle = tcpapi_to_handle(api);

	tcp_recv(pcb, on_recv);
	tcp_err(pcb, on_err);
	tcp_sent(pcb, on_sent);

	addr.addr = tmp.src_ip;

	err = tcp_bind(pcb, &addr, tmp.src_port);
	if (unlikely(err != ERR_OK))
		goto connect_fail;

	addr.addr = tmp.dst_ip;

	err = tcp_connect(pcb, &addr, tmp.dst_port, on_connected);
	if (unlikely(err != ERR_OK))
		goto connect_fail;

	return api->handle;

connect_fail:
	tcp_abort(pcb);
pcb_fail:

	return -RET_NOMEM;
}

int tcp_api_init(void)
{
	struct tcp_pcb *lpcb;
	unsigned int queue;
	int ret;

	/* FIXME: Do a better job of freeing memory on failures */
	for_each_queue(queue) {
		lpcb = tcp_new();
		if (!lpcb)
			return -ENOMEM;

		ret = tcp_bind(lpcb, IP_ADDR_ANY, 8000);
		if (ret != ERR_OK)
			return -EIO;

		lpcb = tcp_listen(lpcb);
		if (!lpcb)
			return -ENOMEM;

		perqueue_get(listen_pcb) = lpcb;
		tcp_accept(lpcb, on_accept);

		ret = mempool_create(&perqueue_get(pcb_mempool), MAX_PCBS,
				     sizeof(struct tcpapi_pcb));
		if (ret)
			return ret;

		ret = mempool_pagemem_create(&perqueue_get(id_mempool),
					     MAX_PCBS,
					     sizeof(struct ip_tuple));
		if (ret)
			return ret;

		ret = mempool_pagemem_map_to_user(&perqueue_get(id_mempool));
		if (ret)
			return ret;
	}

	return 0;
}

