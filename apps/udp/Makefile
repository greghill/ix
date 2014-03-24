CC	= gcc
CFLAGS	= -Wall -g -MD -O3 -I../../libix -I../../ix/inc

all: linux_server ix_server

linux_server: linux_server.o
	$(CC) $(CFLAGS) $(<) -o linux_server
ix_server: ix_server.o ../../libix/libix.a
	$(CC) $(CFLAGS) $(<) -o ix_server -L../../libix -lix

clean:
	rm -f *.o *.d linux_server ix_server

-include *.d

