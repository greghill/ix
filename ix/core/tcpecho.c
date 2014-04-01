#include <ix/queue.h>

#include <lwip/tcp.h>

static err_t on_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
	if (p != NULL) {
		tcp_write(pcb, p->payload, p->len, 0);
		tcp_output(pcb);
		tcp_recved(pcb, p->tot_len);
		pbuf_free(p);
	} else if (err == ERR_OK) {
		return tcp_close(pcb);
	}
	return ERR_OK;
}

static err_t on_accept(void *arg, struct tcp_pcb *pcb, err_t err)
{
	tcp_recv(pcb, on_recv);
	return ERR_OK;
}

int tcp_echo_server_init(int port)
{
	struct tcp_pcb *listen_pcb;
	struct ip_addr ipaddr;
	unsigned int queue;

	for_each_queue(queue) {
		listen_pcb = tcp_new();
		ipaddr.addr = 0;
		tcp_bind(listen_pcb, &ipaddr, port);
		listen_pcb = tcp_listen(listen_pcb);
		tcp_accept(listen_pcb, on_accept);
	}

	return 0;
}
