/*
 * UDP echo server program -- echo-server-udp.c
 * http://www.cs.rpi.edu/~goldsd/docs/spring2012-csci4220/echo-server-udp.c.txt
 */

#include <stdio.h>      /* standard C i/o facilities */
#include <stdlib.h>     /* needed for atoi() */
#include <unistd.h>     /* defines STDIN_FILENO, system calls,etc */
#include <sys/types.h>  /* system data type definitions */
#include <sys/socket.h> /* socket specific definitions */
#include <netinet/in.h> /* INET constants and stuff */
#include <arpa/inet.h>  /* IP address conversion stuff */
#include <netdb.h>      /* gethostbyname */



/* this routine echos any messages (UDP datagrams) received */

#define MAXBUF 1024*1024

void echo(int sd)
{
	size_t n;
	socklen_t len;
	char bufin[MAXBUF];
	struct sockaddr_in remote;

	len = sizeof(remote);

	while (1) {
		/* read a datagram from the socket (put result in bufin) */
		n = recvfrom(sd, bufin, MAXBUF, 0,
			     (struct sockaddr *)&remote, &len);

		/* print out the address of the sender */
//		printf("Got a datagram from %s port %d\n",
//		       inet_ntoa(remote.sin_addr), ntohs(remote.sin_port));

		if (n<0) {
			perror("Error receiving data");
		} else {
//			printf("GOT %ld BYTES\n",n);
			/* Got something, just send it back */
			sendto(sd, bufin, n, 0,
			       (struct sockaddr *)&remote, len);
		}
	}
}

int main(int argc, char *argv[])
{
	int ld;
	struct sockaddr_in skaddr;
	socklen_t length;

  /* create a socket
     IP protocol family (PF_INET)
     UDP protocol (SOCK_DGRAM)
  */

	if ((ld = socket( PF_INET, SOCK_DGRAM, 0 )) < 0) {
		printf("Problem creating socket\n");
		exit(1);
	}

  /* establish our address
     address family is AF_INET
     our IP address is INADDR_ANY (any of our IP addresses)
     the port number is assigned by the kernel
  */
	skaddr.sin_family = AF_INET;
	skaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	skaddr.sin_port = htons(10013);

	if (bind(ld, (struct sockaddr *) &skaddr, sizeof(skaddr))<0) {
		printf("Problem binding\n");
		exit(0);
	}

  /* find out what port we were assigned and print it out */

	length = sizeof( skaddr );
	if (getsockname(ld, (struct sockaddr *) &skaddr, &length)<0) {
		printf("Error getsockname\n");
		exit(1);
	}

  /* port number's are network byte order, we have to convert to
     host byte order before printing !
  */
	printf("The server UDP port number is %d\n",ntohs(skaddr.sin_port));

  /* Go echo every datagram we get */
	echo(ld);
	return(0);
}

