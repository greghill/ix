static inline unsigned long rdtsc(void)
{
        unsigned int a, d;
        asm volatile("rdtsc" : "=a" (a), "=d" (d));
        return ((unsigned long) a) | (((unsigned long) d) << 32);
}

extern int cycles_per_us;

int timer_calibrate_tsc(void);
