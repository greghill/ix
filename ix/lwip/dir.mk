# Temporary makefile for LWIP support code

SRC = inet_chksum.c ip4_addr.c memp_min.c misc.c pbuf.c
$(eval $(call register_dir, lwip, $(SRC)))

