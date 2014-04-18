#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#include <ix.h>
#include <ix/stddef.h>

#include <net/ip.h>

#define BATCH_DEPTH	512

#define BUFFER_POOL_2MB_PAGES	1
#define BUFFER_POOL_SIZE	(BUFFER_POOL_2MB_PAGES * PGSIZE_2MB)

#define MAX_CORES 128

struct worker {
	unsigned long total_connections;
	unsigned long total_messages;
	int printer;
} worker[MAX_CORES];

struct sock {
	long message_count;
	char *send_buffer;
	char *recv_buffer;
	int recv_ofs;
	struct worker *worker;
	struct ip_tuple id;
};

static int nr_cpu;
static uint32_t dst_ip;
static uint16_t dst_port;
static int msg_size;
static long messages_per_connection;
static int connections_per_thread;

static __thread struct sg_entry *ents;
static __thread char *buffer_pool;

static void sock_send_message(hid_t handle, struct sock *sock)
{
	struct sg_entry *ent;

	ent = &ents[ix_bsys_idx()];
	ent->base = sock->send_buffer;
	ent->len = msg_size;
	ix_tcp_sendv(handle, ent, 1);
}

static void sock_connect(struct sock *sock)
{
	ix_tcp_connect(&sock->id, (unsigned long) sock);
}

static void tcp_connect_ret(hid_t handle, unsigned long cookie, int ret)
{
	struct sock *sock;

	if (ret)
		return;

	sock = (struct sock *) cookie;
	sock->worker->total_connections++;
	sock->message_count = 0;
	sock_send_message(handle, sock);
}

static void tcp_recv(hid_t handle, unsigned long cookie,
		     void *addr, size_t len)
{
	struct sock *sock;

	sock = (struct sock *) cookie;

	if (len > msg_size - sock->recv_ofs) {
		fprintf(stderr, "Error: Received more data than expected.\n");
		exit(1);
	}

	memcpy(&sock->recv_buffer[sock->recv_ofs], addr, len);
	sock->recv_ofs += len;
	if (sock->recv_ofs < msg_size)
		goto out;

	sock->recv_ofs = 0;
	if (memcmp(sock->send_buffer, sock->recv_buffer, msg_size)) {
		fprintf(stderr, "Send and recv data do not match.\nSend: '%.*s'\nRecv: '%.*s'\n", msg_size, sock->send_buffer, msg_size, sock->recv_buffer);
		exit(1);
	}
	sock->message_count++;
	sock->worker->total_messages++;

	if (sock->message_count == messages_per_connection) {
		ix_tcp_recv_done(handle, len);
		ix_tcp_close(handle);
		sock_connect(sock);
		return;
	}

	sock_send_message(handle, sock);

out:
	ix_tcp_recv_done(handle, len);
}

static void tcp_dead(hid_t handle, unsigned long cookie)
{
	struct sock *sock;

	sock = (struct sock *) cookie;
	sock_connect(sock);
}

static void
tcp_send_ret(hid_t handle, unsigned long cookie, ssize_t ret)
{
}

static void
tcp_sent(hid_t handle, unsigned long cookie, size_t win_size)
{
}

static struct ix_ops ops = {
	.tcp_connect_ret	= tcp_connect_ret,
	.tcp_recv		= tcp_recv,
	.tcp_dead		= tcp_dead,
	.tcp_send_ret		= tcp_send_ret,
	.tcp_sent		= tcp_sent,
};

static void print_stats(void)
{
	int i, ret;
	unsigned long total_connections;
	unsigned long total_messages;
	char buf;

	ret = read(STDIN_FILENO, &buf, 1);
	if (ret == 0) {
		fprintf(stderr, "Error: EOF on STDIN.\n");
		exit(1);
	} else if (ret == -1 && errno == EAGAIN) {
		return;
	} else if (ret == -1) {
		perror("read");
		exit(1);
	}
	total_connections = 0;
	total_messages = 0;
	for (i = 0; i < nr_cpu; i++) {
		total_connections += worker[i].total_connections;
		total_messages += worker[i].total_messages;
	}
	printf("%ld %ld %d %d %d ", total_connections, total_messages, 0, 0, 0);
	printf("%d %d %d %d ", 0, 0, 0, 0);
	printf("%d ", 0);
	puts("");
	fflush(stdout);
}

static void *thread_main(void *arg)
{
	int ret;
	int cpu, tmp;
	int i, j, ofs, step;
	struct worker *worker;
	struct sock *sock;
	time_t now, before;

	worker = arg;

	ret = ix_init(&ops, BATCH_DEPTH);
	if (ret) {
		printf("unable to init IX\n");
		return NULL;
	}

	ents = malloc(sizeof(struct sg_entry) * BATCH_DEPTH);
	if (!ents)
		return NULL;

	ret = syscall(SYS_getcpu, &cpu, &tmp, NULL);
	if (ret) {
		printf("unable to get current CPU\n");
		return NULL;
	}

	buffer_pool = (char *) MEM_USER_IOMAPM_BASE_ADDR + cpu * BUFFER_POOL_SIZE;
	ret = sys_mmap(buffer_pool, BUFFER_POOL_2MB_PAGES, PGSIZE_2MB, VM_PERM_R | VM_PERM_W);
	if (ret) {
		printf("unable to allocate memory for zero-copy\n");
		return NULL;
	}

	sock = malloc(sizeof(*sock) * connections_per_thread);

	for (i = 0; i < connections_per_thread; i++) {
		sock[i].id.dst_ip = hton32(dst_ip);
		sock[i].id.dst_port = dst_port;
		sock[i].send_buffer = buffer_pool;
		buffer_pool += align_up(msg_size, 64);
		sock[i].recv_buffer = malloc(msg_size);
		sock[i].recv_ofs = 0;
		sock[i].worker = worker;
		ofs = rand();
		step = rand() % 4 + 1;
		for (j = 0; j < msg_size; j++)
			sock[i].send_buffer[j] = 'A' + (j * step + ofs) % 26;
		sock_connect(&sock[i]);
	}

	if (worker->printer) {
		before = 0;
		while (1) {
			ix_poll();
			now = time(NULL);
			if (now != before) {
				before = now;
				print_stats();
			}
		}
	} else {
		while (1)
			ix_poll();
	}

	return NULL;
}

static int parse_ip_addr(const char *str, uint32_t *addr)
{
	unsigned char a, b, c, d;

	if (sscanf(str, "%hhu.%hhu.%hhu.%hhu", &a, &b, &c, &d) != 4)
		return -EINVAL;

	*addr = MAKE_IP_ADDR(a, b, c, d);
	return 0;
}

int main(int argc, char *argv[])
{
	int i, flags;
	pthread_t tid;

	prctl(PR_SET_PDEATHSIG, SIGHUP, 0, 0, 0);
	srand(time(NULL));

	if (argc != 6) {
		fprintf(stderr, "Usage: %s IP PORT CONNECTIONS_PER_THREAD MSG_SIZE MESSAGES_PER_CONNECTION\n", argv[0]);
		return 1;
	}

	if (parse_ip_addr(argv[1], &dst_ip)) {
		fprintf(stderr, "Bad IP address '%s'", argv[1]);
		exit(1);
	}
	dst_port = atoi(argv[2]);
	connections_per_thread = atoi(argv[3]);
	msg_size = atoi(argv[4]);
	messages_per_connection = strtol(argv[5], NULL, 10);

	nr_cpu = sys_nrcpus();
	if (nr_cpu < 1) {
		printf("got invalid cpu count %d\n", nr_cpu);
		exit(-1);
	}

	sys_spawnmode(true);

	for (i = 0; i < nr_cpu - 1; i++) {
		if (pthread_create(&tid, NULL, thread_main, &worker[i])) {
			printf("failed to spawn thread %d\n", i);
			exit(-1);
		}
	}

	flags = fcntl(STDIN_FILENO, F_GETFL, 0);
	fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

	puts("ok");
	fflush(stdout);

	worker[nr_cpu - 1].printer = 1;
	thread_main(&worker[nr_cpu - 1]);

	return 0;
}
