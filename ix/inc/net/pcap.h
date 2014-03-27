#include <ix/mbuf.h>
#include <net/ethernet.h>

int pcap_open_read(const char *pathname, struct eth_addr *mac);

int pcap_open_write(const char *pathname);

int pcap_write(struct mbuf *mbuf);

int pcap_replay();
