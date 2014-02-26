#include <lwip/ip.h>

struct ip_globals ip_data;

void tcp_timer_needed(void)
{
	printf("TODO: implement tcp_timer_needed\n");
	asm("int $3");
}

err_t ip_output(struct pbuf *p, ip_addr_t *src, ip_addr_t *dest, u8_t ttl, u8_t tos, u8_t proto)
{
	printf("TODO: implement ip_output\n");
	asm("int $3");
	return 0;
}

struct netif *ip_route(ip_addr_t *dest)
{
	printf("TODO: implement ip_route\n");
	asm("int $3");
	return NULL;
}
