all_used: client pingpongs # pingpongs_mtcp NPtcp NPmtcp

NPtcp:
	$(MAKE) -C ../apps/NetPIPE-3.7.1/ tcp
	mv ../apps/NetPIPE-3.7.1/NPtcp .

NPmtcp:
	$(MAKE) -C ../apps/NetPIPE-3.7.1/ mtcp
	mv ../apps/NetPIPE-3.7.1/NPmtcp .

all: client server latency pingpongs simple_client

all_mtcp: all server_mtcp pingpongs_mtcp client_mtcp

server: server.c server_common.c server.h
	gcc -O2 -Wall -g server.c server_common.c -o server -levent -pthread

server_mtcp: server_mtcp.c server_common_mtcp.c server_mtcp.h
	gcc -O2 -Wall -g server_mtcp.c server_common_mtcp.c -o server_mtcp -lmtcp -lnuma -lpthread -lps -lrt

pingpongs: pingpongs.c server_common.c server.h
	gcc -O2 -Wall -g pingpongs.c server_common.c -o pingpongs -levent -pthread

pingpongs_mtcp: pingpongs_mtcp.c server_common_mtcp.c server_mtcp.h
	gcc -O2 -Wall -g pingpongs_mtcp.c server_common_mtcp.c -o pingpongs_mtcp -lmtcp -lnuma -lpthread -lps -lrt

client: client.c timer.c timer.h client_common.c
	gcc -O2 -Wall -g client.c timer.c client_common.c -o client -levent -pthread -lrt

client_mtcp: client_mtcp.c timer.c timer.h client_common.c
	gcc -O2 -Wall -g client_mtcp.c timer.c client_common.c -o client_mtcp -lmtcp -lnuma -pthread -lps -lrt

simple_client: simple_client.c client_common.c histogram.c histogram.h timer.c timer.h
	gcc -O2 -Wall simple_client.c client_common.c histogram.c timer.c -o simple_client -pthread -lrt

latency: latency.c timer.c timer.h histogram.c histogram.h
	gcc -O2 -Wall latency.c timer.c histogram.c -o latency -pthread -lrt

clean:
	rm -f server server_mtcp client client_mtcp latency pingpongs pingpongs_mtcp simple_client NPtcp NPmtcp
