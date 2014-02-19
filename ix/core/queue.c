#include <ix/queue.h>

static void queue_alloc_at(struct queue *queue, int index)
{
	queue->item[index].data_offset = (unsigned long long) queue->queue_item_alloc();
}

void queue_init(struct queue *queue, queue_item_alloc_t queue_item_alloc)
{
	int i;

	queue->enqueuep = 1;
	queue->dequeuep = 0;
	queue->queue_item_alloc = queue_item_alloc;

	for (i = 0; i < QUEUE_SIZE; i++)
		queue->item[i].done = 0;
}

void queue_populate(struct queue *queue)
{
	int i;

	for (i = 0; i < QUEUE_SIZE; i++)
		queue_alloc_at(queue, i);
}

int queue_empty(struct queue *queue)
{
	int next;
	next = (queue->dequeuep + 1) % QUEUE_SIZE;
	return queue->enqueuep == next;
}

int queue_enqueue(struct queue *queue, void *data)
{
	struct queue_item *item;

	if (queue->enqueuep == queue->dequeuep)
		return 0;
	item = &queue->item[queue->enqueuep];
	item->data_offset = (unsigned long long) data;
	item->done = 0;
	queue->enqueuep = (queue->enqueuep + 1) % QUEUE_SIZE;
	return 1;
}

void *queue_dequeue(struct queue *queue)
{
	int next;
	void *ret;

	next = (queue->dequeuep + 1) % QUEUE_SIZE;
	if (queue->enqueuep == next)
		return (void *)0;
	queue->dequeuep = next;
	ret = (void *) queue->item[next].data_offset;
	queue_alloc_at(queue, next);
	return ret;
}
