# Temporary makefile for LWIP support code

SRC = inet_chksum.c ip4_addr.c mem.c memp.c misc.c pbuf.c
$(eval $(call register_dir, lwip, $(SRC)))

