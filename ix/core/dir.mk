# Makefile for the core system

SRC = ethdev.c dyncore.c init.c log.c mbuf.c mem.c mempool.c pci.c tcpecho.c timer.c
$(eval $(call register_dir, core, $(SRC)))

