#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <ix/errno.h>
#include <ix/ethdev.h>
#include <ix/types.h>
#include <net/ethernet.h>
#include <net/pcap.h>

typedef struct pcap_hdr_s {
        uint32_t magic_number;   /* magic number */
        uint16_t version_major;  /* major version number */
        uint16_t version_minor;  /* minor version number */
        int32_t  thiszone;       /* GMT to local correction */
        uint32_t sigfigs;        /* accuracy of timestamps */
        uint32_t snaplen;        /* max length of captured packets, in octets */
        uint32_t network;        /* data link type */
} pcap_hdr_t;

typedef struct pcaprec_hdr_s {
        uint32_t ts_sec;         /* timestamp seconds */
        uint32_t ts_usec;        /* timestamp microseconds */
        uint32_t incl_len;       /* number of octets of packet saved in file */
        uint32_t orig_len;       /* actual length of packet */
} pcaprec_hdr_t;

static int fd = -1;
static int write_mode;
static struct eth_addr filter_mac = { .addr = {0,0,0,0,0,0} };

static int pcap_replay_filter(struct mbuf *pkt)
{
	struct eth_hdr *ethhdr;

	ethhdr = mbuf_mtod(pkt, struct eth_hdr *);
	if (!memcmp(&filter_mac, &ethhdr->dhost, ETH_ADDR_LEN))
		return 0;

	return 1;
}

static int pcap_write_filter(struct mbuf *pkt)
{
	struct eth_hdr *ethhdr;

	if (eth_addr_is_zero(&filter_mac))
		return 0;

	ethhdr = mbuf_mtod(pkt, struct eth_hdr *);
	if (!memcmp(&filter_mac, &ethhdr->shost, ETH_ADDR_LEN))
		return 0;
	if (!memcmp(&filter_mac, &ethhdr->dhost, ETH_ADDR_LEN))
		return 0;

	return 1;
}

static int pcap_read(struct mbuf *pkt)
{
	pcaprec_hdr_t hdr;

	if (fd == -1)
		return 0;

	if (read(fd, &hdr, sizeof(hdr)) != sizeof(hdr))
		goto fail;

	if (hdr.incl_len > MBUF_DATA_LEN)
		goto fail;

	if (read(fd, mbuf_mtod(pkt, void *), hdr.incl_len) != hdr.incl_len)
		goto fail;

	pkt->len = hdr.incl_len;

	return 0;

fail:
	close(fd);
	fd = -1;
	return 1;
}

static int fake_xmit(struct eth_tx_queue *tx, int nr, struct mbuf **mbufs)
{
	int i;

	for (i = 0; i < nr; i++)
		mbuf_free(mbufs[i]);

	return nr;
}

int pcap_open_read(const char *pathname, struct eth_addr *mac)
{
	pcap_hdr_t hdr;

	fd = open(pathname, O_RDONLY);
	if (fd == -1)
		return 1;

	if (read(fd, &hdr, sizeof(hdr)) != sizeof(hdr)) {
		close(fd);
		return 1;
	}

	filter_mac = *mac;

	eth_dev = malloc(sizeof(struct rte_eth_dev));
	eth_dev->data = malloc(sizeof(struct rte_eth_dev_data));
	eth_dev->data->mac_addrs = malloc(ETH_ADDR_LEN);
	eth_dev->data->mac_addrs[0] = *mac;

	eth_tx = malloc(sizeof(struct eth_tx_queue));
	eth_tx->xmit = fake_xmit;

	return 0;
}

int pcap_open_write(const char *pathname)
{
	pcap_hdr_t hdr;

	fd = open(pathname, O_CREAT | O_WRONLY | O_TRUNC, 0666);
	if (fd == -1)
		return 1;

	hdr.magic_number = 0xa1b2c3d4;
	hdr.version_major = 2;
	hdr.version_minor = 4;
	hdr.thiszone = 0;
	hdr.sigfigs = 0;
	hdr.snaplen = 65535;
	hdr.network = 1;

	if (write(fd, &hdr, sizeof(hdr)) != sizeof(hdr)) {
		close(fd);
		return 1;
	}

	write_mode = 1;

	return 0;
}

int pcap_write(struct mbuf *pkt)
{
	pcaprec_hdr_t hdr;
	struct timeval tv;

	if (!write_mode)
		return 0;

	if (pcap_write_filter(pkt))
		return 0;

	if (gettimeofday(&tv, NULL))
		goto fail;

        hdr.ts_sec = tv.tv_sec;
        hdr.ts_usec = tv.tv_usec;
        hdr.incl_len = pkt->len;
        hdr.orig_len = pkt->len;

	if (write(fd, &hdr, sizeof(hdr)) != sizeof(hdr))
		goto fail;

	if (write(fd, mbuf_mtod(pkt, void *), pkt->len) != pkt->len)
		goto fail;

	return 0;

fail:
	close(fd);
	fd = -1;
	return 1;
}

int pcap_replay()
{
	int ret;
	struct mbuf *pkt;

	while (1) {
		pkt = mbuf_alloc_local();
		if (unlikely(!pkt))
			return -ENOMEM;

		ret = pcap_read(pkt);
		if (ret) {
			mbuf_free(pkt);
			return ret;
		}

		if (pcap_replay_filter(pkt)) {
			mbuf_free(pkt);
			continue;
		}

		eth_input(pkt);
	}

	return 0;
}
