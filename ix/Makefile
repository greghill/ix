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

SRCS =
DIRS = core ixgbe libdune lwip net sandbox vnic

define register_dir
SRCS += $(patsubst %, $(1)/%, $(2))
endef

define register_dir_asm
ASM_SRCS += $(patsubst %, $(1)/%, $(2))
endef

include $(patsubst %, %/dir.mk, $(DIRS))

OBJS=$(subst .c,.o,$(SRCS))
OBJS+=$(subst .S,.o,$(ASM_SRCS))

all: ix

depend: .depend

.depend: $(SRCS)
	rm -f ./.depend
	$(foreach SRC,$(SRCS),$(CC) $(INC) -MM -MT $(SRC:.c=.o) $(SRC) >> .depend;)

include .depend

ix: $(OBJS) ix.ld
	$(LD) $(LDFLAGS) -o ix $(OBJS) $(LDLIBS)

clean:
	rm $(OBJS) ix .depend

dist-clean: clean
	rm *~

