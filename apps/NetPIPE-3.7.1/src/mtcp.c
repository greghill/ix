/*****************************************************************************/
/* "NetPIPE" -- Network Protocol Independent Performance Evaluator.          */
/* Copyright 1997, 1998 Iowa State University Research Foundation, Inc.      */
/*                                                                           */
/* This program is free software; you can redistribute it and/or modify      */
/* it under the terms of the GNU General Public License as published by      */
/* the Free Software Foundation.  You should have received a copy of the     */
/* GNU General Public License along with this program; if not, write to the  */
/* Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.   */
/*                                                                           */
/*     * tcp.c              ---- TCP calls source                            */
/*     * tcp.h              ---- Include file for TCP calls and data structs */
/*****************************************************************************/
#include    "netpipe.h"

#include <mtcp_api.h>
#include <mtcp_epoll.h>

#if defined (MPLITE)
#include "mplite.h"
#endif


#define NEVENTS 10

int doing_reset = 0;
mctx_t mctx = NULL;
int ep = -1;


int mtcp_accept_helper(mctx_t mctx, int sockid, struct sockaddr *addr, socklen_t *addrlen)
{
    int nevents, i;
    struct mtcp_epoll_event events[NEVENTS];
    struct mtcp_epoll_event evctl;
    
    evctl.events = MTCP_EPOLLIN;
    evctl.data.sockid = sockid;
    if (mtcp_epoll_ctl(mctx, ep, MTCP_EPOLL_CTL_ADD, sockid, &evctl) < 0) {
        perror("epoll_ctl");
        exit(1);
    }
    
    while (1) {
        nevents = mtcp_epoll_wait(mctx, ep, events, NEVENTS, -1);
        if (nevents < 0) {
			if (errno != EINTR)
				perror("mtcp_epoll_wait");
			exit(1);
		}
        
        for (i = 0; i < nevents; ++i) {
            if (events[i].data.sockid == sockid) {
                mtcp_epoll_ctl(mctx, ep, MTCP_EPOLL_CTL_DEL, sockid, NULL);                
                return mtcp_accept(mctx, sockid, addr, addrlen);
            } else {
                printf("Socket error!\n");
                exit(1);
            }
        }
    }
}

int mtcp_read_helper(mctx_t mctx, int sockid, char *buf, int len)
{
    int nevents, i;
    struct mtcp_epoll_event events[NEVENTS];
    struct mtcp_epoll_event evctl;
    
    evctl.events = MTCP_EPOLLIN;
    evctl.data.sockid = sockid;
    if (mtcp_epoll_ctl(mctx, ep, MTCP_EPOLL_CTL_ADD, sockid, &evctl) < 0) {
        perror("epoll_ctl");
        exit(1);
    }
    
    while (1) {
        nevents = mtcp_epoll_wait(mctx, ep, events, NEVENTS, -1);
        if (nevents < 0) {
			if (errno != EINTR)
				perror("mtcp_epoll_wait");
			exit(1);
		}
        
        for (i = 0; i < nevents; ++i) {
            if (events[i].data.sockid == sockid) {
                mtcp_epoll_ctl(mctx, ep, MTCP_EPOLL_CTL_DEL, sockid, NULL);                
                return mtcp_read(mctx, sockid, buf, len);
            } else {
                printf("Socket error!\n");
                exit(1);
            }
        }
    }
}

int mtcp_write_helper(mctx_t mctx, int sockid, char *buf, int len)
{
    int nevents, i;
    struct mtcp_epoll_event events[NEVENTS];
    struct mtcp_epoll_event evctl;
    
    evctl.events = MTCP_EPOLLOUT;
    evctl.data.sockid = sockid;
    if (mtcp_epoll_ctl(mctx, ep, MTCP_EPOLL_CTL_ADD, sockid, &evctl) < 0) {
        perror("epoll_ctl");
        exit(1);
    }
    
    while (1) {
        nevents = mtcp_epoll_wait(mctx, ep, events, NEVENTS, -1);
        if (nevents < 0) {
			if (errno != EINTR)
				perror("mtcp_epoll_wait");
			exit(1);
		}
        
        for (i = 0; i < nevents; ++i) {
            if (events[i].data.sockid == sockid) {
                mtcp_epoll_ctl(mctx, ep, MTCP_EPOLL_CTL_DEL, sockid, NULL);                
                return mtcp_write(mctx, sockid, buf, len);
            } else {
                printf("Socket error!\n");
                exit(1);
            }
        }
    }
}

void Init(ArgStruct *p, int* pargc, char*** pargv)
{
   p->reset_conn = 0; /* Default to not resetting connection */
   p->prot.sndbufsz = p->prot.rcvbufsz = 0;
   p->tr = 0;     /* The transmitter will be set using the -h host flag. */
   p->rcv = 1;
   mtcp_init("NPmtcp.conf");
}

void Setup(ArgStruct *p)
{

 int one = 1;
 int sockfd;
 struct sockaddr_in *lsin1, *lsin2;      /* ptr to sockaddr_in in ArgStruct */
 char *host;
 struct hostent *addr;
 struct protoent *proto;
 int send_size, recv_size, sizeofint = sizeof(int);
 int socket_family = AF_INET;


 host = p->host;                           /* copy ptr to hostname */ 

 if (p->use_sdp){
	 printf("Using AF_INET_SDP (27) socket family\n");
	 socket_family = 27;
 }
 
 lsin1 = &(p->prot.sin1);
 lsin2 = &(p->prot.sin2);

 bzero((char *) lsin1, sizeof(*lsin1));
 bzero((char *) lsin2, sizeof(*lsin2));
 
 if (!mctx) {
   mtcp_core_affinitize(mtcp_get_hyperthread_sibling(0));
   mctx = mtcp_create_context(0);
   ep = mtcp_epoll_create(mctx, NEVENTS);
 }
 
 if (!mctx) {
   printf("NetPIPE: can't create mTCP socket! errno=%d\n", errno);
   exit(-4);
 }

 if ( (sockfd = mtcp_socket(mctx, socket_family, MTCP_SOCK_STREAM, 0)) < 0){ 
   printf("NetPIPE: can't open stream socket! errno=%d\n", errno);
   exit(-4);
 }
 
 mtcp_setsock_nonblock(mctx, sockfd);

 if(!(proto = getprotobyname("tcp"))){
   printf("NetPIPE: protocol 'tcp' unknown!\n");
   exit(555);
 }

 if( p->tr ) {                             /* Primary transmitter */

   if (atoi(host) > 0) {                   /* Numerical IP address */
     lsin1->sin_family = AF_INET;
     lsin1->sin_addr.s_addr = inet_addr(host);

   } else {
      
     if ((addr = gethostbyname(host)) == NULL){
       printf("NetPIPE: invalid hostname '%s'\n", host);
       exit(-5);
     }

     lsin1->sin_family = addr->h_addrtype;
     bcopy(addr->h_addr, (char*) &(lsin1->sin_addr.s_addr), addr->h_length);
   }

   lsin1->sin_port = htons(p->port);

   p->commfd = sockfd;

 } else if( p->rcv ) {                     /* we are the receiver */
   
   bzero((char *) lsin1, sizeof(*lsin1));
   lsin1->sin_family      = AF_INET;
   lsin1->sin_addr.s_addr = htonl(INADDR_ANY);
   lsin1->sin_port        = htons(p->port);
   
   if (mtcp_bind(mctx, sockfd, (struct sockaddr *) lsin1, sizeof(*lsin1)) < 0){
     printf("NetPIPE: server: bind on local address failed! errno=%d", errno);
     exit(-6);
   }

   p->servicefd = sockfd;
 }
 p->upper = send_size + recv_size;

 establish(p);                               /* Establish connections */

}   

static int
readFully(int fd, void *obuf, int len)
{
  int bytesLeft = len;
  char *buf = (char *) obuf;
  int bytesRead = 0;
  
  while (bytesLeft > 0 &&
         (bytesRead = mtcp_read_helper(mctx, fd, (void *) buf, bytesLeft)) != 0)
    {
      if (bytesRead < 0) {
        if (errno == EAGAIN)
          continue;
        else
          break;
      }
      
      bytesLeft -= bytesRead;
      buf += bytesRead;
    }
  if (bytesRead <= 0) return bytesRead;
  return len;
}

void Sync(ArgStruct *p)
{
    char s[] = "SyncMe", response[] = "      ";
    int ret;

    ret = mtcp_write_helper(mctx, p->commfd, s, strlen(s));
    while (ret < 0 && errno == EAGAIN) {
        ret = mtcp_write_helper(mctx, p->commfd, s, strlen(s));
    }
    
    if (ret < 0 ||           /* Write to nbor */
        readFully(p->commfd, response, strlen(s)) < 0)  /* Read from nbor */
      {
        perror("NetPIPE: error writing or reading synchronization string");
        exit(3);
      }
    if (strncmp(s, response, strlen(s)))
      {
        fprintf(stderr, "NetPIPE: Synchronization string incorrect! |%s|\n", response);
        exit(3);
      }
}

void PrepareToReceive(ArgStruct *p)
{
        /*
            The Berkeley sockets interface doesn't have a method to pre-post
            a buffer for reception of data.
        */
}

void SendData(ArgStruct *p)
{
    int bytesWritten, bytesLeft;
    char *q;

    bytesLeft = p->bufflen;
    bytesWritten = 0;
    q = p->s_ptr;
    while (bytesLeft > 0 &&
           (bytesWritten = mtcp_write_helper(mctx, p->commfd, q, bytesLeft)) != 0)
      {
        if (bytesWritten < 0) {
          if (errno == EAGAIN)
            continue;
          else
            break;
        }
        
        bytesLeft -= bytesWritten;
        q += bytesWritten;
      }
    if (bytesWritten < 0)
      {
        printf("NetPIPE: write: error encountered, errno=%d\n", errno);
        exit(401);
      }
}

void RecvData(ArgStruct *p)
{
    int bytesLeft;
    int bytesRead;
    char *q;

    bytesLeft = p->bufflen;
    bytesRead = 0;
    q = p->r_ptr;
    while (bytesLeft > 0 &&
           (bytesRead = mtcp_read_helper(mctx, p->commfd, q, bytesLeft)) != 0)
      {
        if (bytesRead < 0) {
          if (errno == EAGAIN)
            continue;
          else
            break;
        }
        
        bytesLeft -= bytesRead;
        q += bytesRead;
      }
    if (bytesLeft > 0 && bytesRead == 0)
      {
        printf("NetPIPE: \"end of file\" encountered on reading from socket\n");
      }
    else if (bytesRead == -1)
      {
        printf("NetPIPE: read: error encountered, errno=%d\n", errno);
        exit(401);
      }
}

/* uint32_t is used to insure that the integer size is the same even in tests 
 * between 64-bit and 32-bit architectures. */

void SendTime(ArgStruct *p, double *t)
{
    uint32_t ltime, ntime;
    int ret;

    /*
      Multiply the number of seconds by 1e8 to get time in 0.01 microseconds
      and convert value to an unsigned 32-bit integer.
      */
    ltime = (uint32_t)(*t * 1.e8);

    /* Send time in network order */
    ntime = htonl(ltime);
    ret = mtcp_write_helper(mctx, p->commfd, (char *)&ntime, sizeof(uint32_t));
    while (ret < 0 && errno == EAGAIN) {
        ret = mtcp_write_helper(mctx, p->commfd, (char *)&ntime, sizeof(uint32_t));
    }
    
    if (ret < 0)
      {
        printf("NetPIPE: write failed in SendTime: errno=%d\n", errno);
        exit(301);
      }
}

void RecvTime(ArgStruct *p, double *t)
{
    uint32_t ltime, ntime;
    int bytesRead;

    bytesRead = readFully(p->commfd, (void *)&ntime, sizeof(uint32_t));
    if (bytesRead < 0)
      {
        printf("NetPIPE: read failed in RecvTime: errno=%d\n", errno);
        exit(302);
      }
    else if (bytesRead != sizeof(uint32_t))
      {
        fprintf(stderr, "NetPIPE: partial read in RecvTime of %d bytes\n",
                bytesRead);
        exit(303);
      }
    ltime = ntohl(ntime);

        /* Result is ltime (in microseconds) divided by 1.0e8 to get seconds */

    *t = (double)ltime / 1.0e8;
}

void SendRepeat(ArgStruct *p, int rpt)
{
  uint32_t lrpt, nrpt;
  int ret;

  lrpt = rpt;
  /* Send repeat count as a long in network order */
  nrpt = htonl(lrpt);
  
  ret = mtcp_write_helper(mctx, p->commfd, (void *) &nrpt, sizeof(uint32_t));
  while (ret < 0 && errno == EAGAIN) {
    ret = mtcp_write_helper(mctx, p->commfd, (void *) &nrpt, sizeof(uint32_t));
  }
  
  if (ret < 0)
    {
      printf("NetPIPE: write failed in SendRepeat: errno=%d\n", errno);
      exit(304);
    }
}

void RecvRepeat(ArgStruct *p, int *rpt)
{
  uint32_t lrpt, nrpt;
  int bytesRead;

  bytesRead = readFully(p->commfd, (void *)&nrpt, sizeof(uint32_t));
  if (bytesRead < 0)
    {
      printf("NetPIPE: read failed in RecvRepeat: errno=%d\n", errno);
      exit(305);
    }
  else if (bytesRead != sizeof(uint32_t))
    {
      fprintf(stderr, "NetPIPE: partial read in RecvRepeat of %d bytes\n",
              bytesRead);
      exit(306);
    }
  lrpt = ntohl(nrpt);

  *rpt = lrpt;
}

void establish(ArgStruct *p)
{
  int one = 1;
  socklen_t clen;
  struct protoent *proto;

  clen = (socklen_t) sizeof(p->prot.sin2);

  if( p->tr ){

    while( mtcp_connect(mctx, p->commfd, (struct sockaddr *) &(p->prot.sin1),
                   sizeof(p->prot.sin1)) < 0  && errno != EINPROGRESS) {

      /* If we are doing a reset and we get a connection refused from
       * the connect() call, assume that the other node has not yet
       * gotten to its corresponding accept() call and keep trying until
       * we have success.
       */
      if(!doing_reset || errno != ECONNREFUSED) {
        printf("Client: Cannot Connect! errno=%d\n",errno);
        exit(-10);
      } 
        
    }

  } else if( p->rcv ) {

    /* SERVER */
    mtcp_listen(mctx, p->servicefd, 5);
    p->commfd = mtcp_accept_helper(mctx, p->servicefd, (struct sockaddr *) &(p->prot.sin2), &clen);
    while (p->commfd < 0 && errno == EAGAIN) {
      p->commfd = mtcp_accept_helper(mctx, p->servicefd, (struct sockaddr *) &(p->prot.sin2), &clen);
    }
    
    if(p->commfd < 0){
      printf("Server: Accept Failed! errno=%d\n",errno);
      perror("accept");
      exit(-12);
    }

    if(!(proto = getprotobyname("tcp"))){
      printf("unknown protocol!\n");
      exit(555);
    }
  }
}

void CleanUp2(ArgStruct *p)
{
   char quit[5];
   quit[0] = 'Q';
   quit[1] = 'U';
   quit[2] = 'I';
   quit[3] = 'T';
   quit[4] = '\0';

   if (p->tr) {

      mtcp_write_helper(mctx, p->commfd,quit, 5);
      mtcp_read_helper(mctx, p->commfd, quit, 5);
      mtcp_close(mctx, p->commfd);

   } else if( p->rcv ) {

      mtcp_read_helper(mctx, p->commfd,quit, 5);
      mtcp_write_helper(mctx, p->commfd,quit,5);
      mtcp_close(mctx, p->commfd);
      mtcp_close(mctx, p->servicefd);

   }
}


void CleanUp(ArgStruct *p)
{
}


void Reset(ArgStruct *p)
{
  
  /* Reset sockets */

  if(p->reset_conn) {

    doing_reset = 1;

    /* Close the sockets */

    CleanUp2(p);

    /* Now open and connect new sockets */

    Setup(p);

  }

}

void AfterAlignmentInit(ArgStruct *p)
{

}

