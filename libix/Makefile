# A Makefile for libIX.

INC	= -I. -I../ix/inc
CC 	= gcc
CFLAGS	= -g -Wall -O3 $(INC)
AR	= ar

SRCS	= main.c mem.c mempool.c ixev.c
OBJS	= $(subst .c,.o,$(SRCS))

all: libix.a

depend: .depend

.depend: $(SRCS)
	rm -f ./.depend
	$(foreach SRC,$(SRCS),$(CC) $(CFLAGS) -MM -MT $(SRC:.c=.o) $(SRC) >> .depend;)

include .depend

libix.a: $(OBJS)
	$(AR) cru $(@) $(OBJS)

clean:
	rm $(OBJS) libix.a .depend

dist-clean: clean
	rm *~

