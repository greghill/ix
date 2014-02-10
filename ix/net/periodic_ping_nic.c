#include <string.h>

#include "nic.h"

static char original_pkt_data[] = {0xb8,0xca,0x3a,0xf7,0x4d,0x86,0x00,0x08,0xe3,0xff,0xfc,0x50,0x08,0x00,0x45,0x00,0x00,0x3c,0x68,0xb0,0x00,0x00,0x7e,0x01,0xe6,0x46,0x80,0xb2,0xb7,0xec,0x80,0xb2,0x34,0x79,0x08,0x00,0x4d,0x4c,0x00,0x01,0x00,0x0f,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69};

static char pkt_data[sizeof(original_pkt_data)];

static struct rte_mbuf mypkt = {
  .pkt = {
    .data = pkt_data,
    .data_len = sizeof(original_pkt_data),
  },
};

static int pping_has_pending_pkts(void)
{
	return 1;
}

static int pping_receive_one_pkt(struct rte_mbuf **pkt)
{
	static time_t prv;
	time_t now;

	now = time(NULL);
	if (now - prv >= 2) {
		memcpy(pkt_data, original_pkt_data, sizeof(original_pkt_data));
		pkt[0] = &mypkt;
		prv = now;
		return 1;
	}
	return 0;
}

static int pping_tx_one_pkt(struct rte_mbuf *pkt)
{
	int i;
	unsigned char *data;

	printf("Transmit packet: ");
	data = pkt->pkt.data;
	for (i = 0; i < pkt->pkt.data_len; i++) {
		printf("%02x ", data[i]);
	}
	printf("\n");
	return 1;
}

static void pping_free_pkt(struct rte_mbuf *pkt)
{
}

static struct rte_mbuf *pping_alloc_pkt()
{
	return 0;
}

struct nic_operations pping_nic_ops = {
  .has_pending_pkts = pping_has_pending_pkts,
  .receive_one_pkt  = pping_receive_one_pkt,
  .tx_one_pkt       = pping_tx_one_pkt,
  .free_pkt	    = pping_free_pkt,
  .alloc_pkt 	    = pping_alloc_pkt,
};
