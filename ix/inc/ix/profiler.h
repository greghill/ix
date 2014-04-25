#ifdef ENABLE_PROFILER

void profiler_init(void);

#else /* ENABLE_PROFILER */

static inline void profiler_init(void) { }

#endif /* ENABLE_PROFILER */
