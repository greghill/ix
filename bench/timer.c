#include <stdint.h>
#include <time.h>

#include "timer.h"

int cycles_per_us;

int timer_calibrate_tsc(void)
{
        struct timespec sleeptime = {.tv_nsec = 5E8 }; /* 1/2 second */
        struct timespec t_start, t_end;

        if (clock_gettime(CLOCK_MONOTONIC_RAW, &t_start) == 0) {
                uint64_t ns, end, start;
                double secs;

                start = rdtsc();
                nanosleep(&sleeptime,NULL);
                clock_gettime(CLOCK_MONOTONIC_RAW, &t_end);
                end = rdtsc();
                ns = ((t_end.tv_sec - t_start.tv_sec) * 1E9);
                ns += (t_end.tv_nsec - t_start.tv_nsec);

                secs = (double)ns / 1000;
                cycles_per_us = (uint64_t)((end - start)/secs);
                return 0;
        }

        return -1;
}
