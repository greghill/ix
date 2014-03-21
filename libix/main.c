#include <stddef.h>
#include <errno.h>
#include <stdio.h>

#include "syscall.h"
#include "ix.h"

#define NUM_KARR_DESCS	1024

static struct bsys_arr *uarr;
struct bsys_arr *karr;
static bsysfn_t usys_tbl[USYS_NR];

/**
 * ix_poll - flush pending commands and check for new commands
 */
void ix_poll(void)
{
	int i;
	sys_bpoll(&karr->descs[0], karr->len);
	karr->len = 0;

	for (i = 0; i < uarr->len; i++) {
		struct bsys_desc d = uarr->descs[i];
		usys_tbl[d.sysnr](d.arga, d.argb, d.argc, d.argd);
	}
}

/**
 * ix_flush - flush pending commands
 */
void ix_flush(void)
{
	sys_bcall(&karr->descs[0], karr->len);
	karr->len = 0;
}

/**
 * ix_init - initializes libIX
 * @ops: user-provided event handlers
 *
 * Returns 0 if successful, otherwise fail.
 */
int ix_init(struct ix_ops *ops)
{
	if (!ops)
		return -EINVAL;

	if (!ops->udp_recv)
		return -EINVAL;
	if (!ops->udp_send_ret)
		return -EINVAL;

	/* unpack the ops into a more efficient representation */
	usys_tbl[USYS_UDP_RECV]		= (bsysfn_t) &ops->udp_recv;
	usys_tbl[USYS_UDP_SEND_RET]	= (bsysfn_t) &ops->udp_send_ret;

	uarr = sys_baddr();
	if (!uarr)
		return -EFAULT;

	karr = malloc(sizeof(struct bsys_arr) +
		      sizeof(struct bsys_desc) * NUM_KARR_DESCS);
	if (!karr)
		return -ENOMEM;
	
	karr->len = 0;
	karr->max_len = NUM_KARR_DESCS;

	return 0;
}
