#include <stddef.h>
#include <errno.h>
#include <stdio.h>

#include "syscall.h"
#include "ix.h"

static struct bsys_arr *uarr, *karr;
static bsysfn_t usys_tbl[USYS_NR];

void ix_poll(void)
{
	int i;
	sys_bpoll(NULL, 0);

	for (i = 0; i < uarr->len; i++) {
		struct bsys_desc d = uarr->descs[i];
		usys_tbl[d.sysnr](d.arga, d.argb, d.argc, d.argd);
	}
}

int ix_init(struct ix_ops *ops)
{
	if (!ops)
		return -EINVAL;

	if (!ops->udp_recv)
		return -EINVAL;
	if (!ops->udp_send_done)
		return -EINVAL;

	/* unpack the ops into a more efficient representation */
	usys_tbl[USYS_UDP_RECV]		= (bsysfn_t) &ops->udp_recv;
	usys_tbl[USYS_UDP_SEND_DONE]	= (bsysfn_t) &ops->udp_send_done;

	uarr = sys_baddr();
	if (!uarr)
		return -EFAULT;

	return 0;
}

