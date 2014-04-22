CC	= gcc
CFLAGS	= -Wall -g -MD -O3 -I../../libix -I../../ix/inc

all: pingpong

pingpong: pingpong.o ../../libix/libix.a
	$(CC) $(CFLAGS) $(<) -o pingpong -L../../libix -lix -lpthread

clean:
	rm -f *.o *.d pingpong

-include *.d

