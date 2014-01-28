# Makefile for the core system

SRC = log.c mempool.c pci.c timer.c dyncore.c
$(eval $(call register_dir, core, $(SRC)))

