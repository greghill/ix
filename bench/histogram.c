#include <stdio.h>
#include <stdlib.h>

#include "histogram.h"

#define MAX_COUNT (16 * 1024 * 1024)

static int data[MAX_COUNT];
static int index;

static int compare_int(const void *x, const void *y)
{
	int a = *((int *)x);
	int b = *((int *)y);
	if (a < b)
		return -1;
	if (a > b)
		return 1;
	return 0;
}

static double calc_percentile(int *array, int size, double percentile)
{
	double index;
	int floor, ceiling;

	if (!size)
		return 0;

	index = percentile * size - 1;
	floor = (int) index;
	ceiling = floor + 1;
	if (floor < 0)
		return array[0];
	if (ceiling > size - 1)
		return array[size - 1];
	return array[floor] + (array[ceiling] - array[floor]) * (index - floor);
}

int histogram_get_count(void)
{
	return index;
}

double histogram_get_percentile(double percentile)
{
	qsort(data, index, sizeof(data[0]), compare_int);

	return calc_percentile(data, index, percentile);
}

void histogram_record(int latency_us)
{
	if (index >= MAX_COUNT) {
		fprintf(stderr, "error: cannot record more than %d samples in histogram.\n", MAX_COUNT);
		exit(1);
	}

	data[index] = latency_us;
	index++;
}

void histogram_init()
{
}
