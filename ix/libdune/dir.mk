# Makefile for libdune module

SRC = entry.c elf.c vm.c util.c page.c procmap.c
$(eval $(call register_dir, libdune, $(SRC)))

ASM_SRC = dune.S vsyscall.S
$(eval $(call register_dir_asm, libdune, $(ASM_SRC)))

NOFPU_SRC = trap.c
$(eval $(call register_dir, libdune, $(NOFPU_SRC)))

libdune/trap.o: CFLAGS := $(CFLAGS) -mno-sse -mno-mmx -mno-sse2 -mno-3dnow -msoft-float
