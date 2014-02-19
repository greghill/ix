#define QUEUE_SIZE 16

struct queue_item {
	int done;
	unsigned long long data_offset;
};

typedef void *(*queue_item_alloc_t) (void);

struct queue {
	int enqueuep;
	int dequeuep;
	queue_item_alloc_t queue_item_alloc;
	struct queue_item item[QUEUE_SIZE];
};

void queue_init(struct queue *queue, queue_item_alloc_t queue_item_alloc);

void queue_populate(struct queue *queue);

int queue_empty(struct queue *queue);

int queue_enqueue(struct queue *queue, void *data);

void *queue_dequeue(struct queue *queue);
