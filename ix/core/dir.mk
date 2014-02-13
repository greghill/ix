# Makefile for the core system

SRC = dyncore.c log.c mem.c mempool.c pci.c timer.c queue.c
$(eval $(call register_dir, core, $(SRC)))

