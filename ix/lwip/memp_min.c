#include "lwip/memp.h"
#include "lwip/tcp_impl.h"
#include "lwip/timers.h"

#define MEMP_ALIGN_SIZE(x) (LWIP_MEM_ALIGN_SIZE(x))

const u16_t memp_sizes[MEMP_MAX] = {
#define LWIP_MEMPOOL(name,num,size,desc)  LWIP_MEM_ALIGN_SIZE(size),
#include <lwip/memp_std.h>
};
