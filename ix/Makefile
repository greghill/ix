# A Makefile for IX.
#
# path for this Makefile to work properly.

INC	= -I./inc -I./libdune
CC	= gcc
CFLAGS	= -g -Wall -O3 $(INC)
LD	= gcc
LDFLAGS	=
LDLIBS	= -lrt -lpthread -lm -lnuma -ldl

ifneq ($(DYNCORE_DEBUG),)
CFLAGS += -DDYNCORE_DEBUG
endif

SRCS =
DIRS = core ixgbe libdune net sandbox

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

ix: $(OBJS)
	$(LD) $(LDFLAGS) -o ix $(OBJS) $(LDLIBS)

clean:
	rm $(OBJS) ix .depend

dist-clean: clean
	rm *~

