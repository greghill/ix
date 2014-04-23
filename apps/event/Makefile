CC	= gcc
CFLAGS	= -Wall -g -MD -O3 -I../../libix -I../../ix/inc

all: pingpong stream

pingpong: pingpong.o ../../libix/libix.a
	$(CC) $(CFLAGS) $(<) -o pingpong -L../../libix -lix -lpthread
stream: stream.o ../../libix/libix.a
	$(CC) $(CFLAGS) $(<) -o stream -L../../libix -lix -lpthread

clean:
	rm -f *.o *.d pingpong stream

-include *.d

