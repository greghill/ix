all: client server latency rexec pingpongs simple_client

server: server.c server_common.c server.h
	gcc -O2 -Wall -I/home/prekas/libevent/include -I/home/prekas/libevent/build/include -g server.c server_common.c -o server -L/home/prekas/libevent/build/lib -levent -pthread

pingpongs: pingpongs.c server_common.c server.h
	gcc -O2 -Wall -I/home/prekas/libevent/include -I/home/prekas/libevent/build/include -g pingpongs.c server_common.c -o pingpongs -L/home/prekas/libevent/build/lib -levent -pthread

client: client.c timer.c timer.h client_common.c
	gcc -O2 -Wall -I/home/prekas/libevent/include -I/home/prekas/libevent/build/include -g client.c timer.c client_common.c -o client -L/home/prekas/libevent/build/lib -levent -pthread -lrt

simple_client: simple_client.c client_common.c
	gcc -O2 -Wall simple_client.c client_common.c -o simple_client -pthread

latency: latency.c timer.c timer.h
	gcc -O2 -Wall latency.c timer.c -o latency -pthread -lrt

rexec: rexec.c
	gcc -O2 -Wall -o rexec rexec.c

clean:
	rm -f server client latency rexec pingpongs simple_client
