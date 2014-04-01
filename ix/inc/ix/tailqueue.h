
#pragma once


#define MIN_NINES 1 /* 90% */
#define MAX_NINES 5 /* 99.999 */ 

const char *tailqueue_nines[] = 
  {"","90%","99%","99.9%","99.99%","99.999"};


struct tailqueue;

struct taildistr {
  uint64_t count,min,max;
  uint64_t nines[MAX_NINES+1];   /* nb: nines[0] is unused */
};

extern void tailqueue_addsample(struct tailqueue *tq,
                         uint64_t t_us);

extern void tailqueue_calcnines(struct tailqueue *tq,
				struct taildistr *td,
				int reset);


