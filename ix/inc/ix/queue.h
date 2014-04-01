/*
 * queue.h - support for multiqueue and perqueue data.
 */

#pragma once

#include <ix/types.h>
#include <ix/cpu.h>

#define NQUEUE	16

/* used to define perqueue variables */
#define DEFINE_PERQUEUE(type, name) \
	typeof(type) perqueue_##name __attribute__((section(".perqueue,\"\",@nobits#")))

/* used to make perqueue variables externally available */
#define DECLARE_PERQUEUE(type, name) \
	extern DEFINE_PERQUEUE(type, name)

DECLARE_PERCPU(void *, current_perqueue);

static inline void * __perqueue_get(void *key)
{
	void *offset;

	offset = percpu_get(current_perqueue);

	return (void *) ((uintptr_t) key + (uintptr_t) offset);
}

/**
 * perqueue_get - get the perqueue variable of the current processed queue
 * @var: the perqueue variable
 *
 * Returns a perqueue variable.
 */
#define perqueue_get(var)						\
	(*((typeof(perqueue_##var) *) (__perqueue_get(&perqueue_##var))))


/**
 * for_each_queue - iterate over every queue
 * @queue: the (optionally unsigned) integer iterator
 *
 * After the loop, queue is >= NQUEUE or the index of the first unitialized queue.
 */
#define for_each_queue(queue) for ((queue) = -1; !set_current_queue(++(queue));)

int queue_init_one(unsigned int queue);

int set_current_queue(unsigned int queue);

void unset_current_queue(void);
