# A Makefile for IX.
#
# path for this Makefile to work properly.

INC	= -I./inc -I./libdune -I./inc/lwip -I./inc/lwip/ipv4 -I./inc/lwip/ipv6 -include../dune/dune.h
CC	= gcc
CFLAGS	= -g -Wall -fno-dwarf2-cfi-asm -fno-asynchronous-unwind-tables -O3 $(INC) -D__KERNEL__
LD	= gcc
LDFLAGS	= -T ix.ld
LDLIBS	= -lrt -lpthread -lm -lnuma -ldl

ifneq ($(DEBUG),)
CFLAGS += -DDEBUG
endif

ifneq ($(ENABLE_PCAP),)
CFLAGS += -DENABLE_PCAP
endif

ifneq ($(DYNCORE_DEBUG),)
CFLAGS += -DDYNCORE_DEBUG
endif

ifneq ($(ENABLE_KSTATS),)
CFLAGS += -DENABLE_KSTATS
endif

SRCS =
DIRS = core ixgbe libdune lwip net sandbox vnic

define register_dir
SRCS += $(patsubst %, $(1)/%, $(2))
endef

define register_dir_asm
ASM_SRCS += $(patsubst %, $(1)/%, $(2))
endef

include $(patsubst %, %/dir.mk, $(DIRS))

%.generated.S: %.c
	$(CC) $(CFLAGS) -o $@ -S $<

all: ix

ifneq ($(ENABLE_PROFILER),)
CFLAGS += -DENABLE_PROFILER
OBJS=$(subst .c,.generated.S,$(SRCS))
DEPENDENCY = core/profiler_funcinfo_init.h
core/profiler_funcinfo_init.h: $(OBJS)
	python core/profiler_instrument.py -o core/profiler_funcinfo_init.h $(subst .c,.generated.S,$(SRCS))
core/profiler.o: core/profiler.c core/profiler_funcinfo_init.h
OBJS+=core/profiler.o
else
OBJS=$(subst .c,.o,$(SRCS))
endif
OBJS+=$(subst .S,.o,$(ASM_SRCS))

depend: .depend

.depend: $(SRCS)
	bash -c "cat $(foreach SRC,$(SRCS),<($(CC) $(CFLAGS) -MM -MT $(SRC:.c=.o) $(SRC))) > ./.depend"

ifneq ($(MAKECMDGOALS),clean)
include .depend
endif

ix: $(DEPENDENCY) $(OBJS) ix.ld
	$(LD) $(LDFLAGS) -o ix $(OBJS) $(LDLIBS)

clean:
	rm -f $(OBJS) ix .depend

dist-clean: clean
	rm *~

