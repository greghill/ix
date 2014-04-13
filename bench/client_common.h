#include <arpa/inet.h>

void get_ifname(struct sockaddr_in *server_addr, char *ifname);
void get_eth_stats(char *ifname, long *rx_bytes, long *rx_packets, long *tx_bytes, long *tx_packets);
