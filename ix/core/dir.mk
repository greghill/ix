# Makefile for the core system

SRC = ethdev.c ethfg.c ethqueue.c cfg.c control_plane.c cpu.c init.c log.c mbuf.c mem.c mempool.c page.c pci.c queue.c syscall.c toeplitz.c timer.c vm.c

ifneq ($(ENABLE_KSTATS),)
SRC += kstats.c tailqueue.c 
endif

$(eval $(call register_dir, core, $(SRC)))

