#define _GNU_SOURCE 

#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <time.h> 
#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <sched.h>
#include <sys/prctl.h>
#include <netinet/tcp.h>

struct upload_thread_arg
{
	int sock;
	int cpu_index;
	pthread_t tid;
};

#define SEND_BUFFER_SIZE	(2048)

static void *upload_thread(void *p)
{
	struct upload_thread_arg *arg = (struct upload_thread_arg *)p;
	cpu_set_t cpu_set;
	int flag;
	char data[SEND_BUFFER_SIZE];
	ssize_t status;

	flag = 1;

	CPU_ZERO(&cpu_set);
	CPU_SET(arg->cpu_index, &cpu_set);
	if (sched_setaffinity(0, sizeof(cpu_set_t), &cpu_set) != 0)
	{
		perror("sched_setaffinity");
		exit(1);
	}

	if (setsockopt(arg->sock, IPPROTO_TCP, TCP_NODELAY, (void *) &flag, sizeof(flag)))
	{
		perror("setsockopt(TCP_NODELAY)");
		exit(1);
	}

	memset(data, 0, sizeof(data));

	printf("starting thread %u bound to cpu %d\n", (unsigned int)arg->tid, arg->cpu_index);

	while(1)
	{
		status = send(arg->sock, data, sizeof(data), 0);
		
		if(status < 0)
		{
			break;
		}
	}

	close(arg->sock);

	printf("exiting thread %u\n", (unsigned int)arg->tid);

	arg = NULL;
	free(p);
	p = NULL;

	return NULL;
}

void start_thread(int sock, int cpu_index)
{	
	struct upload_thread_arg *arg = calloc(1, sizeof(struct upload_thread_arg));
	
	assert(arg);
	arg->sock = sock;
	arg->cpu_index = cpu_index;
	arg->tid = 0;

	pthread_create(&arg->tid, NULL, upload_thread, arg);
}

int main(int argc, char *argv[])
{
    int listenfd = 0, connfd = 0;
    struct sockaddr_in serv_addr;
    int cpu_index = 0;
    int one;
    int cpu_count;

	cpu_count = sysconf(_SC_NPROCESSORS_CONF);

    listenfd = socket(AF_INET, SOCK_STREAM, 0);

    one = 1;
	if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEPORT, (void *) &one, sizeof(one))) {
		perror("setsockopt(SO_REUSEPORT)");
		return -1;
	}


    memset(&serv_addr, '0', sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(8000); 

    bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)); 

    listen(listenfd, 10); 

    while(1)
    {
        connfd = accept(listenfd, (struct sockaddr*)NULL, NULL);

        assert(connfd >= 0);

        start_thread(connfd, cpu_index);

        cpu_index = (cpu_index + 1) % cpu_count;
     }
}