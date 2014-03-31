# Makefile for the core system

SRC = ethdev.c cpu.c dyncore.c init.c log.c mbuf.c mem.c mempool.c page.c pci.c syscall.c tcpecho.c timer.c vm.c kstats.c
$(eval $(call register_dir, core, $(SRC)))

