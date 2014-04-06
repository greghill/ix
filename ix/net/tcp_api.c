/*
 * tcp_api.c - plumbing between the TCP and userspace
 */

#define DEBUG 1

#include <ix/stddef.h>
#include <ix/errno.h>
#include <ix/syscall.h>
#include <ix/queue.h>
#include <ix/log.h>
#include <ix/uaccess.h>

#include <lwip/tcp.h>

#include "cfg.h"

#define MAX_PCBS	65536

static DEFINE_PERQUEUE(struct tcp_pcb *, listen_pcb);

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
	struct mbuf *recvd;
	struct mbuf *recvd_tail;
	int queue;
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

	p = &perqueue_get_remote(pcb_mempool, queue);

	api = (struct tcpapi_pcb *) ((uintptr_t) p->buf +
				     idx * sizeof(struct tcpapi_pcb));

	/* check if the handle is actually allocated */
	if (unlikely(api->alive > 1))
		return NULL;

	set_current_queue(api->queue);

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


void bsys_tcp_connect(struct ip_tuple __user *id, unsigned long cookie)
{
	log_debug("tcpapi: bsys_tcp_connect() - id %p, cookie %lx\n",
		  id, cookie);
	usys_tcp_connect_ret(0, cookie, -ENOTSUP);
}

void bsys_tcp_accept(hid_t handle, unsigned long cookie)
{
	/*
	 * FIXME: this function is sort of a placeholder since we have no
	 * choice but to have already accepted the connection under LWIP's
	 * synchronous API.
	 */

	struct tcpapi_pcb *api = handle_to_tcpapi(handle);

	log_debug("tcpapi: bsys_tcp_accept() - handle %lx, cookie %lx\n",
		  handle, cookie);

	if (unlikely(!api)) {
		log_debug("tcpapi: invalid handle\n");
		return;
	}

	if (api->id) {
		mempool_free(&perqueue_get_remote(id_mempool, api->queue),
			     api->id);
		api->id = NULL;
	}
}

void bsys_tcp_reject(hid_t handle)
{
	/*
	 * FIXME: LWIP's synchronous handling of accepts
	 * makes supporting this call impossible.
	 */
	log_err("tcpapi: bsys_tcp_reject() is not implemented\n");
}

void bsys_tcp_send(hid_t handle, void *addr, size_t len)
{
	log_debug("tcpapi: bsys_tcp_send() - addr %p, len %lx\n",
		  addr, len);

	usys_tcp_send_ret(handle, 0, -ENOTSUP);
}

void bsys_tcp_sendv(hid_t handle, struct sg_entry __user *ents,
		    unsigned int nrents)
{
	struct tcpapi_pcb *api = handle_to_tcpapi(handle);
	int i;
	size_t len_xmited = 0;

	log_debug("tcpapi: bsys_tcp_sendv() - handle %lx, ents %p, nrents %ld\n",
		  handle, ents, nrents);

	if (unlikely(!api)) {
		log_debug("tcpapi: invalid handle\n");
		return;
	}

	if (unlikely(!uaccess_okay(ents, nrents * sizeof(struct sg_entry)))) {
		usys_tcp_send_ret(handle, api->cookie, -RET_FAULT);
		return;
	}

	nrents = min(nrents, MAX_SG_ENTRIES);
	for (i = 0; i < nrents; i++) {
		err_t err;
		void *base = (void *) uaccess_peekq((uint64_t *) &ents[i].base);
		size_t len = uaccess_peekq(&ents[i].len);

		if (unlikely(!uaccess_zc_okay(base, len)))
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
	}

	if (len_xmited)
		tcp_output(api->pcb);

	usys_tcp_send_ret(handle, api->cookie, len_xmited);
}

void bsys_tcp_recv_done(hid_t handle, size_t len)
{
	struct tcpapi_pcb *api = handle_to_tcpapi(handle);
	struct mbuf *recvd, *next;

	log_debug("tcpapi: bsys_tcp_recv_done - handle %lx, len %ld\n",
		  handle, len);

	if (unlikely(!api)) {
		log_debug("tcpapi: invalid handle\n");
		return;
	}

	recvd = api->recvd;

	tcp_recved(api->pcb, len);
	while (recvd) {
		if (len < recvd->len) {
			recvd->len -= len;
			break;
		}

		len -= recvd->len;
		next = recvd->next;
		mbuf_free(recvd);
		recvd = next;
	}

	api->recvd = recvd;
}

void bsys_tcp_close(hid_t handle)
{
	struct tcpapi_pcb *api = handle_to_tcpapi(handle);
	struct mbuf *recvd, *next;

	log_debug("tcpapi: bsys_tcp_close - handle %lx\n", handle);

	if (unlikely(!api)) {
		log_debug("tcpapi: invalid handle\n");
		return;
	}

	if (api->pcb) {
		err_t err = tcp_close(api->pcb);
		if (err != ERR_OK)
			tcp_abort(api->pcb);
	}

	recvd = api->recvd;
	while (recvd) {
		next = recvd->next;
		mbuf_free(recvd);
		recvd = next;
	}

	if (api->id) {
		mempool_free(&perqueue_get_remote(id_mempool, api->queue),
			     api->id);
	}

	mempool_free(&perqueue_get_remote(pcb_mempool, api->queue), api);
}

static void mark_dead(struct tcpapi_pcb *api)
{
	api->alive = false;
	api->pcb = NULL;
	usys_tcp_dead(api->handle, api->cookie);
}

static err_t on_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
	struct tcpapi_pcb *api;
	struct mbuf *pkt;
	struct pbuf *tmp;

	log_debug("tcpapi: on_recv - arg %p, pcb %p, pbuf %p, err %d\n",
		  arg, pcb, p, err);

	api = (struct tcpapi_pcb *) arg;

	/* FIXME: It's not really clear what to do with "err" */

	/* Was the connection closed? */
	if (!p) {
		mark_dead(api);
		return ERR_OK;
	}

	if (!api->recvd)
		api->recvd = p->mbuf;
	else
		api->recvd_tail->next = p->mbuf;

	/* Walk through the full receive chain */
	do {
		pkt = p->mbuf;
		pkt->done_data = 0; /* FIXME: so terrible :( */
		pkt->len = p->len; /* repurpose len for recv_done */
		usys_tcp_recv(api->handle, api->cookie,
			      mbuf_to_iomap(pkt, p->payload), p->len);

		tmp = p->next;
		pbuf_free(p);
		p = tmp;
	} while (p);

	api->recvd_tail = pkt;

	return ERR_OK;
}

static void on_err(void *arg, err_t err)
{
	log_debug("tcpapi: on_err - arg %p\n", arg, err);
}

static err_t on_sent(void *arg, struct tcp_pcb *pcb, u16_t len)
{
	struct tcpapi_pcb *api;

	log_debug("tcpapi: on_sent - arg %p, pcb %p, len %wd\n",
		  arg, pcb, len);

	api = (struct tcpapi_pcb *) arg;
	usys_tcp_xmit_win(api->handle, api->cookie, len);

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
	api->queue = perqueue_get(queue_id);

	tcp_arg(pcb, api);
	tcp_recv(pcb, on_recv);
	tcp_err(pcb, on_err);
	tcp_sent(pcb, on_sent);
	tcp_accepted(perqueue_get(listen_pcb));

	id->src_ip = 0; /* FIXME: LWIP doesn't provide this information :( */
	id->dst_ip = cfg_host_addr.addr;
	id->src_port = pcb->local_port;
	id->dst_port = pcb->remote_port;

	handle = tcpapi_to_handle(api);
	api->handle = handle;
	id = (struct ip_tuple *)
		mempool_pagemem_to_iomap(&perqueue_get(id_mempool), id);

	usys_tcp_knock(handle, id);

	return ERR_OK;
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

