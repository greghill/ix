#include <arpa/inet.h>
#include <linux/if.h>
#include <linux/if_ether.h>
#include <net/ethernet.h>
#include <netpacket/packet.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/ether.h>

#include "cfg.h"
#include "nic.h"

static char buffer[4096];

static struct rte_mbuf mypkt;
static int sock;
static char broadcast_mac[] = {0xff,0xff,0xff,0xff,0xff,0xff};
static int iface_index;

static int raw_socket_init(const char *device)
{
	struct ifreq ifr;
	int sock;

	memset (&ifr, 0, sizeof (struct ifreq));

	if ((sock = socket(PF_PACKET, SOCK_RAW, htons (ETH_P_ALL))) < 1) {
		printf("ERROR: Could not open socket, Got #?\n");
		exit(1);
	}

	strcpy(ifr.ifr_name, device);

	if (ioctl(sock, SIOCGIFINDEX, &ifr) == -1) {
		perror("Error: Could not retrive the interface index from the device.\n");
		exit(1);
	}

	iface_index = ifr.ifr_ifindex;

	if (ioctl(sock, SIOCGIFFLAGS, &ifr) == -1) {
		perror("Error: Could not retrive the flags from the device.\n");
		exit(1);
	}

	ifr.ifr_flags |= IFF_PROMISC;
	if (ioctl(sock, SIOCSIFFLAGS, &ifr) == -1) {
		perror("Error: Could not set flag IFF_PROMISC");
		exit(1);
	}

	if (ioctl(sock, SIOCGIFINDEX, &ifr) < 0) {
		perror("Error: Error getting the device index.\n");
		exit(1);
	}

	return sock;
}

static int virtual_has_pending_pkts(void)
{
	return 1;
}

static int virtual_receive_one_pkt(struct rte_mbuf **pkt)
{
	int size;

	if (!sock)
		sock = raw_socket_init("eth0");

	size = recv(sock, buffer, sizeof(buffer), 0);
	if (!memcmp(buffer, &cfg_mac, ETH_ALEN) || !memcmp(buffer, broadcast_mac, ETH_ALEN)) {
		pkt[0] = &mypkt;
		mypkt.pkt.data = buffer;
		mypkt.pkt.data_len = size;
		return 1;
	}

	return 0;
}

static int virtual_tx_one_pkt(struct rte_mbuf *pkt)
{
	struct sockaddr_ll sa;

	sa.sll_family = PF_PACKET;
	sa.sll_halen = ETH_ALEN;
	memcpy(sa.sll_addr, pkt[0].pkt.data, ETH_ALEN);
	sa.sll_ifindex = iface_index;
	sendto(sock, pkt[0].pkt.data, pkt[0].pkt.data_len, 0, (struct sockaddr *) &sa, sizeof(sa));
	return 1;
}

static void virtual_free_pkt(struct rte_mbuf *pkt)
{
}

static struct rte_mbuf *virtual_alloc_pkt()
{
	return 0;
}

struct nic_operations virtual_nic_ops = {
  .has_pending_pkts = virtual_has_pending_pkts,
  .receive_one_pkt  = virtual_receive_one_pkt,
  .tx_one_pkt       = virtual_tx_one_pkt,
  .free_pkt         = virtual_free_pkt,
  .alloc_pkt        = virtual_alloc_pkt,
};
