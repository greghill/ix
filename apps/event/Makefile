CC	= gcc
CFLAGS	= -Wall -g -MD -O3 -I../../libix -I../../ix/inc

all: pingpong stream client_test

pingpong: pingpong.o ../../libix/libix.a
	$(CC) $(CFLAGS) $(<) -o pingpong -L../../libix -lix -lpthread
stream: stream.o ../../libix/libix.a
	$(CC) $(CFLAGS) $(<) -o stream -L../../libix -lix -lpthread
client_test: client_test.o ../../libix/libix.a
	$(CC) $(CFLAGS) $(<) -o client_test -L../../libix -lix -lpthread

clean:
	rm -f *.o *.d pingpong stream client_test

-include *.d

