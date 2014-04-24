
#pragma once

#include <ix/stddef.h>
#include <ix/cpu.h>

typedef struct kstats_distr {
  uint64_t count;
  uint64_t slow_count;
  uint64_t min_occ;
  uint64_t max_occ;
  uint64_t tot_occ;
  uint64_t min_lat;
  uint64_t max_lat;
  uint64_t tot_lat;

} kstats_distr;


typedef struct kstats_accumulate { 
  kstats_distr *cur;
  uint64_t start_lat;
  uint64_t start_occ;
  uint64_t accum_time;
} kstats_accumulate;


typedef struct kstats { 
#define DEF_KSTATS(_c) kstats_distr  _c
#include "kstatvectors.h"
} kstats;

#ifdef ENABLE_KSTATS

DECLARE_PERCPU(kstats,_kstats);
DECLARE_PERCPU(kstats_accumulate, _kstats_accumulate);

extern void kstats_enter(kstats_distr *n, kstats_accumulate *saved_accu);
extern void kstats_leave(kstats_accumulate *saved_accu);

static inline void kstats_vector(kstats_distr *n)
{
  percpu_get(_kstats_accumulate).cur = n; 
}

#define KSTATS_PUSH(TYPE,_save) \
	kstats_enter(&(percpu_get(_kstats)).TYPE,_save)
#define KSTATS_VECTOR(TYPE)     \
	kstats_vector(&(percpu_get(_kstats)).TYPE)
#define KSTATS_POP(_save)       \
	kstats_leave(_save)
//#define KSTATS_CURRENT_IS(TYPE)	(percpu_get(_kstats_accumulate).cur==&kstatsCounters.TYPE)

extern void kstats_init_cpu(void);

#else /* ENABLE_KSTATS */

#define KSTATS_PUSH(TYPE,_save)     
#define KSTATS_VECTOR(TYPE)         
#define KSTATS_POP(_save)           
#define KSTATS_CURRENT_IS(TYPE)     0

static inline void kstats_init_cpu(void) { }

#endif /* ENABLE_KSTATS */

