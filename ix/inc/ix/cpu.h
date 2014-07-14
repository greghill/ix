/*
 * cpu.h - support for multicore and percpu data.
 */

#pragma once

#include <ix/stddef.h>
#include <asm/cpu.h>

#define NCPU	128
extern int cpu_count; /* the number of available CPUs */
extern int cpus_active; /* the number of in-use CPUs */

/* used to define percpu variables */
#define DEFINE_PERCPU(type, name) \
	typeof(type) name __attribute__((section(".percpu,\"\",@nobits#")))

/* used to make percpu variables externally available */
#define DECLARE_PERCPU(type, name) \
	extern DEFINE_PERCPU(type, name)

extern void *percpu_offsets[NCPU];

/**
 * percpu_get_remote - get a percpu variable on a specific core
 * @var: the percpu variable
 * @cpu: the cpu core number
 *
 * Returns a percpu variable.
 */
#define percpu_get_remote(var, cpu)				\
	(*((typeof(var) *) ((uintptr_t) &var +			\
			    (uintptr_t) percpu_offsets[(cpu)])))

static inline void * __percpu_get(void *key)
{
	void *offset;

	asm("mov %%gs:0, %0" : "=r" (offset));

	return (void *) ((uintptr_t) key + (uintptr_t) offset);
}

/**
 * percpu_get - get the local percpu variable
 * @var: the percpu variable
 *
 * Returns a percpu variable.
 */
#define percpu_get(var)						\
	(*((typeof(var) *) (__percpu_get(&var))))

/**
 * cpu_is_active - is the CPU being used?
 * @cpu: the cpu number
 *
 * Returns true if yes, false if no.
 */
#define cpu_is_active(cpu)					\
	(percpu_offsets[(cpu)] != NULL)

static inline unsigned int __cpu_next_active(unsigned int cpu)
{
	while (cpu < cpu_count) {
		cpu++;

		if (cpu_is_active(cpu))
			return cpu;
	}

	return cpu;
}

/**
 * for_each_active_cpu - iterates over each active (used by IX) CPU
 * @cpu: an integer to store the cpu
 */
#define for_each_active_cpu(cpu)				\
	for ((cpu) = -1; (cpu) = __cpu_next_active(cpu); (cpu) < cpu_count)

DECLARE_PERCPU(unsigned int, cpu_numa_node);
DECLARE_PERCPU(unsigned int, cpu_id);

extern void cpu_do_bookkeeping(void);

typedef void (*cpu_func_t)(void *data);
extern int cpu_run_on_one(cpu_func_t func, void *data,
			  unsigned int cpu);

extern int cpu_init_one(unsigned int cpu);
extern int cpu_init(void);

