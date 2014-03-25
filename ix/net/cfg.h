/*
 * cfg.h - network stack configuration parameters
 */

extern struct ip_addr cfg_host_addr, cfg_broadcast_addr, cfg_gateway_addr;
extern uint32_t cfg_mask;
extern struct eth_addr cfg_mac;

extern int read_configuration(const char *path);
