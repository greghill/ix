
/*
 * kstats.c -- "kstats" (tree-based) statistics
 *    reports min/avg/max latency and occupancy for the various IX services
 *    printfs at regular intervals, which reset the counters.
 */



#include <strings.h>
#include <ix/kstats.h>
#include <ix/log.h>
#include <ix/timer.h>

#define KSTATS_INTERVAL 5 * ONE_SECOND

DEFINE_PERCPU(kstats, _kstats);
DEFINE_PERCPU(kstats_accumulate, _kstats_accumulate);

static DEFINE_PERCPU(struct timer, _kstats_timer);

void kstats_enter(kstats_distr *n, kstats_accumulate *saved_accu)
{
  kstats_distr *old = percpu_get(_kstats_accumulate).cur;
  uint64_t now = rdtsc();
  if (old && saved_accu) {
    *saved_accu = percpu_get(_kstats_accumulate);
    saved_accu->cur = old;
    saved_accu->accum_time = now - saved_accu->start_occ;
  }

  percpu_get(_kstats_accumulate).cur = n;
  percpu_get(_kstats_accumulate).start_lat = now;
  percpu_get(_kstats_accumulate).start_occ = now;
  percpu_get(_kstats_accumulate).accum_time = 0;
}

void kstats_leave(kstats_accumulate *saved_accu)
{
  kstats_accumulate *acc = &percpu_get(_kstats_accumulate);
  uint64_t now = rdtscp(NULL);
  uint64_t diff_lat = now - acc->start_lat;
  uint64_t diff_occ = now - acc->start_occ + acc->accum_time;
  kstats_distr *cur = acc->cur;
  
  if (cur)  {
    cur->tot_lat += diff_lat;
    cur->tot_occ += diff_occ;
    if (cur->count==0) {
      cur->min_lat = diff_lat;
      cur->max_lat = diff_lat;
      cur->min_occ = diff_occ;
      cur->max_occ = diff_occ;
    } else {
      if (cur->min_lat >diff_lat) cur->min_lat = diff_lat;
      if (cur->max_lat <diff_lat) cur->max_lat = diff_lat;
      if (cur->min_occ >diff_occ) cur->min_occ = diff_occ;
      if (cur->max_occ <diff_occ) cur->max_occ = diff_occ;
    }
    cur->count++;
    if (saved_accu) {
      *acc = *saved_accu;
      acc->start_occ = now;
    }
  }
}

static void kstats_printone(kstats_distr *d, const char *name)
{
  if (d->count) { 
    log_info("kstat: %2d %-19s %7lu latency %7lu | %7lu | %7lu "
	     "occupancy %6lu | %6lu | %6lu\n",
	   percpu_get(cpu_id),
	   name,
	   d->count,
	   d->min_lat,
	   d->tot_lat/d->count,
	   d->max_lat,
	   d->min_occ,
	   d->tot_occ/d->count,
	   d->max_occ);
  }
}

/*
 * print and reinitialize
 */
static void kstats_print(struct timer *t)
{
  kstats *ks = &(percpu_get(_kstats));
#undef DEF_KSTATS
#define DEF_KSTATS(_c)  kstats_printone(&ks->_c, # _c);
#include <ix/kstatvectors.h>
  log_info("\n");

  KSTATS_VECTOR(print_kstats);
  bzero(ks,sizeof(*ks));

  timer_add(&percpu_get(_kstats_timer), KSTATS_INTERVAL);
}

void kstats_init_cpu(void)
{
  timer_init_entry(&percpu_get(_kstats_timer), kstats_print);
  timer_add(&percpu_get(_kstats_timer), KSTATS_INTERVAL);
}

