#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include <ix/profiler.h>
#include <ix/timer.h>

#define PROFILER_INTERVAL 5 * ONE_SECOND
#define FUNCTIONS (sizeof(profiler_funcinfo) / sizeof(profiler_funcinfo[0]))
#define STACK_DEPTH 1024

const struct {
	const char * const funcname;
} profiler_funcinfo[] = {
#include "profiler_funcinfo_init.h"
};

struct {
	unsigned int count;
	unsigned long duration;
} profiler_funcdata[FUNCTIONS];

struct {
	unsigned int funcid;
	unsigned long start;
} profiler_stack[STACK_DEPTH];

unsigned int profiler_stack_pointer;

static struct output_struct {
	unsigned int index;
	unsigned int count;
	unsigned long duration;
} output[FUNCTIONS];

static struct timer timer;

static int output_compar(const void *x, const void *y)
{
	const struct output_struct *a = x;
	const struct output_struct *b = y;
	long av = a->duration;
	long bv = b->duration;

	if (av == bv)
		return 0;
	if (av < bv)
		return 1;
	return -1;
}

static void profiler_print(struct timer *t)
{
	int i, output_index;

	output_index = 0;
	for (i = 0; i < FUNCTIONS; i++) {
		if (!profiler_funcdata[i].count)
			continue;
		output[output_index].index = i;
		output[output_index].count = profiler_funcdata[i].count;
		output[output_index].duration = profiler_funcdata[i].duration;
		output_index++;
	}
	qsort(output, output_index, sizeof(output[0]), output_compar);
	printf("\033[1m%-50s %-12s %-5s %-10s\033[m\n",
		"function",
		"total",
		"count",
		"per call"
	);
	for (i = 0; i < output_index; i++) {
		printf("%-50s %-12ld %-5d %-10ld\n",
			profiler_funcinfo[output[i].index].funcname,
			output[i].duration,
			output[i].count,
			output[i].duration / output[i].count
			);
	}
	puts("");
	bzero(profiler_funcdata, sizeof(profiler_funcdata));
	timer_add(&timer, PROFILER_INTERVAL);
}

void profiler_init(void)
{
	timer_init_entry(&timer, profiler_print);
	timer_add(&timer, PROFILER_INTERVAL);
}
