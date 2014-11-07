#define	LWIP_STATS	0
#define	LWIP_TCP	1
#define	NO_SYS		1
#define LWIP_RAW	0
#define LWIP_UDP	0
#define IP_REASSEMBLY	0
#define IP_FRAG		0
#define LWIP_NETCONN	0

#define MEM_LIBC_MALLOC 1
#define MEMP_MEM_MALLOC 1

//#define	LWIP_DEBUG		LWIP_DBG_OFF
#undef LWIP_DEBUG
#define	TCP_CWND_DEBUG		LWIP_DBG_OFF
#define	TCP_DEBUG		LWIP_DBG_OFF
#define	TCP_FR_DEBUG		LWIP_DBG_OFF
#define	TCP_INPUT_DEBUG		LWIP_DBG_OFF
#define	TCP_OUTPUT_DEBUG	LWIP_DBG_OFF
#define	TCP_QLEN_DEBUG		LWIP_DBG_OFF
#define	TCP_RST_DEBUG		LWIP_DBG_OFF
#define	TCP_RTO_DEBUG		LWIP_DBG_OFF
#define	TCP_WND_DEBUG		LWIP_DBG_OFF

#include <ix/stddef.h>
#include <ix/byteorder.h>

#define LWIP_IX

#define LWIP_PLATFORM_BYTESWAP	1
#define LWIP_PLATFORM_HTONS(x) hton16(x)
#define LWIP_PLATFORM_NTOHS(x) ntoh16(x)
#define LWIP_PLATFORM_HTONL(x) hton32(x)
#define LWIP_PLATFORM_NTOHL(x) ntoh32(x)

#define LWIP_WND_SCALE 1
#define TCP_RCV_SCALE 7
#define TCP_SND_BUF 65536
#define TCP_MSS 1460
#define TCP_WND (2048 * TCP_MSS)

#define CHECKSUM_CHECK_IP               0
#define CHECKSUM_CHECK_TCP              0
#define TCP_ACK_DELAY (1 * ONE_MS)
#define RTO_UNITS (500 * ONE_MS)

/* EdB 2014-11-07 */
#define LWIP_NOASSERT
