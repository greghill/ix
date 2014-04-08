CC	= gcc
CFLAGS	= -Wall -g -MD -O3 -I../../libix -I../../ix/inc

all: ix_recv

ix_recv: ix_recv.o ../../libix/libix.a
	$(CC) $(CFLAGS) $(<) -o ix_recv -L../../libix -lix

clean:
	rm -f *.o *.d ix_recv

-include *.d

