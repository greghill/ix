/*
 * queue.h - support for multiqueue and perqueue data.
 */

#pragma once

#error "can't use queue.h anymore"

#include <ix/types.h>
#include <ix/cpu.h>

#define NQUEUE	64

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
 * After the loop, queue is >= percpu_get(eth_rx_count).
 */
#define for_each_queue(queue) for ((queue) = -1; !set_current_queue_by_index(++(queue));)

struct eth_rx_queue;

int queue_init_one(struct eth_rx_queue *rx_queue);

void set_current_queue(struct eth_rx_queue *rx_queue);

int set_current_queue_by_index(unsigned int index);

void unset_current_queue(void);
