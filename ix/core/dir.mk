# Makefile for the core system

SRC = ethdev.c init.c log.c mem.c mempool.c pci.c timer.c dyncore.c
$(eval $(call register_dir, core, $(SRC)))

