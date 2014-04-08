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

#define	LWIP_DEBUG		LWIP_DBG_OFF
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

#define LWIP_PLATFORM_BYTESWAP	1
#define LWIP_PLATFORM_HTONS(x) hton16(x)
#define LWIP_PLATFORM_NTOHS(x) ntoh16(x)
#define LWIP_PLATFORM_HTONL(x) hton32(x)
#define LWIP_PLATFORM_NTOHL(x) ntoh32(x)

#define LWIP_WND_SCALE 1
#define TCP_RCV_SCALE 6
#define TCP_SND_BUF 65536
#define TCP_MSS 1460
#define TCP_WND (16 * TCP_MSS)
