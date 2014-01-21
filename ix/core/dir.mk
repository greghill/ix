# Makefile for the core system

SRC = log.c mempool.c timer.c 
$(eval $(call register_dir, core, $(SRC)))

