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
#include <sys/fcntl.h>

#if 0
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

	//fcntl(arg->sock, F_SETFL, O_NONBLOCK);  // set to non-blocking
	//fcntl(arg->sock, F_SETFL, O_ASYNC);     // set to asynchronous I/O

	CPU_ZERO(&cpu_set);
	CPU_SET(arg->cpu_index, &cpu_set);

	if (sched_setaffinity(0, sizeof(cpu_set_t), &cpu_set) != 0)
	{
		perror("sched_setaffinity");
		exit(1);
	}

#if 1
	if (setsockopt(arg->sock, IPPROTO_TCP, TCP_NODELAY, (void *) &flag, sizeof(flag)))
	{
		perror("setsockopt(TCP_NODELAY)");
		exit(1);
	}
#endif
	memset(data, 0, sizeof(data));

	printf("starting thread %u bound to cpu %d with fd %d\n", (unsigned int)arg->tid, arg->cpu_index, arg->sock);

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

	//pthread_create(&arg->tid, NULL, upload_thread, arg);
}

#endif

#define MAX(a,b) (a > b)?a:b

#define SEND_BUFFER_SIZE	(2 * 1024 * 1024)
char data[SEND_BUFFER_SIZE];

int main(int argc, char *argv[])
{
    int listenfd = 0, connfd = 0;
    struct sockaddr_in serv_addr;
    int cpu_index = 0;
    int one;
    int cpu_count;


    fd_set write_fd_set;
	fd_set read_fd_set;
	int max_fd = 0;



	cpu_set_t cpu_set;

	CPU_ZERO(&cpu_set);
	CPU_SET(0, &cpu_set);

	#if 0
	if (sched_setaffinity(0, sizeof(cpu_set_t), &cpu_set) != 0)
	{
		perror("sched_setaffinity");
		exit(1);
	}

	printf("bind to cpu 0\n");
	#endif

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

	FD_ZERO(&read_fd_set);
	FD_ZERO(&write_fd_set);

	FD_SET(listenfd, &read_fd_set);

    max_fd = MAX(max_fd,listenfd);

    while(1)
    {
       	int status;
       	fd_set rset;
       	fd_set wset;
       	int send_status;
       	int fd;
       	int flags;

		FD_ZERO(&rset);
		FD_ZERO(&wset);

       	rset = read_fd_set;
       	wset = write_fd_set;

       	status = select(max_fd + 1, &rset, &wset, NULL, NULL);

		if(status <= 0)
		{
			printf("select returned %d\n", status);
			continue;
		}


		if(FD_ISSET(listenfd, &rset))
		{
			int flag;

			flag = 1;
		
			connfd = accept(listenfd, (struct sockaddr*)NULL, NULL);
			assert(connfd >= 0);
			FD_SET(connfd, &write_fd_set);
			max_fd = MAX(max_fd, connfd);
			flags = fcntl(connfd, F_GETFL, 0);
			status = fcntl(connfd, F_SETFL, O_ASYNC|O_NONBLOCK|flags);
			//printf("status: %d\n", status);
			//printf("accepted connection at fd %d\n", connfd);

			if (setsockopt(connfd, IPPROTO_TCP, TCP_NODELAY, (void *) &flag, sizeof(flag)))
			{
				perror("setsockopt(TCP_NODELAY)");
				exit(1);
			}
		}


		for(fd = 0; fd <= max_fd; fd++)
		{
			if(!FD_ISSET(fd, &wset))
				continue;

			//printf("writing connection at fd %d\n", fd);

			while(1)
			{
				send_status = send(fd, data, sizeof(data), 0);

				if(send_status <= 0)
					break;
			}
		}
     }
}