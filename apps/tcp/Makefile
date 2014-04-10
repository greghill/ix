CC	= gcc
CFLAGS	= -Wall -g -MD -O3 -I../../libix -I../../ix/inc

all: ix_recv ix_server ix_pingpongs

ix_recv: ix_recv.o ../../libix/libix.a
	$(CC) $(CFLAGS) $(<) -o ix_recv -L../../libix -lix
ix_server: ix_server.o ../../libix/libix.a
	$(CC) $(CFLAGS) $(<) -o ix_server -L../../libix -lix -lpthread
ix_pingpongs: ix_pingpongs.o ../../libix/libix.a
	$(CC) $(CFLAGS) $(<) -o ix_pingpongs -L../../libix -lix -lpthread

clean:
	rm -f *.o *.d ix_recv ix_server

-include *.d

