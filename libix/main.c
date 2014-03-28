#include <stddef.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "syscall.h"
#include "ix.h"

static struct bsys_arr *uarr;
struct bsys_arr *karr;
static bsysfn_t usys_tbl[USYS_NR];

/**
 * ix_poll - flush pending commands and check for new commands
 *
 * Returns the number of new commands received.
 */
int ix_poll(void)
{
	int i;
	int ret;

	ret = sys_bpoll(karr->descs, karr->len);
	if (ret) {
		printf("libix: encountered a fatal memory fault\n");
		exit(-1);
	}

	karr->len = 0;

	for (i = 0; i < uarr->len; i++) {
		struct bsys_desc d = uarr->descs[i];
		usys_tbl[d.sysnr](d.arga, d.argb, d.argc, d.argd);
	}

	return uarr->len;
}

/**
 * ix_flush - flush pending commands
 */
void ix_flush(void)
{
	if (sys_bcall(karr->descs, karr->len)) {
		printf("libix: encountered a fatal memory fault\n");
		exit(-1);
	}

	karr->len = 0;
}

/**
 * ix_init - initializes libIX
 * @ops: user-provided event handlers
 * @batch_depth: the maximum number of outstanding requests to the kernel
 *
 * Returns 0 if successful, otherwise fail.
 */
int ix_init(struct ix_ops *ops, int batch_depth)
{
	if (!ops)
		return -EINVAL;

	if (!ops->udp_recv)
		return -EINVAL;
	if (!ops->udp_send_ret)
		return -EINVAL;

	/* unpack the ops into a more efficient representation */
	usys_tbl[USYS_UDP_RECV]		= (bsysfn_t) ops->udp_recv;
	usys_tbl[USYS_UDP_SEND_RET]	= (bsysfn_t) ops->udp_send_ret;

	uarr = sys_baddr();
	if (!uarr)
		return -EFAULT;

	karr = malloc(sizeof(struct bsys_arr) +
		      sizeof(struct bsys_desc) * batch_depth);
	if (!karr)
		return -ENOMEM;
	
	karr->len = 0;
	karr->max_len = batch_depth;

	return 0;
}
