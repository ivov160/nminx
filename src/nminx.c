#include <stdio.h>
#include <stdlib.h>

#include <error.h>
#include <errno.h>
#include <string.h>

#include <signal.h>
#include <assert.h>

#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <mtcp_api.h>
#include <mtcp_epoll.h>

#include <nminx/nminx.h>
#include <nminx/watchdog.h>

#include <nminx/server.h>

#define MAX_FLOW_NUM  (10000)

#define RCVBUF_SIZE (2*1024)
#define SNDBUF_SIZE (8*1024)

#define CBUF_SIZE (RCVBUF_SIZE*2)

#define MAX(a, b) 			((a)>(b)?(a):(b))
#define MIN(a, b) 			((a)<(b)?(a):(b))

#define MAX_EVENTS (MAX_FLOW_NUM * 3)

#define WAIT_TIMEOUT_MS 1000

static int is_active = NULL;

static char* default_mtcp_config_path = "config/nminx.conf";
static int default_backlog_size = 4096;

///@todo implement ring buffer
struct connection_ctx
{
	char buffer[CBUF_SIZE];
	uint32_t size;

	uint32_t offset;
	uint8_t flushed;
};

struct server_ctx
{
	mctx_t mctx;
	int ep;
	int socket;

	struct connection_ctx* connections;
};

struct server_ctx* ctx_init(int core)
{
	struct server_ctx* ctx;

	mtcp_core_affinitize(core);
	
	ctx = (struct server_ctx*) calloc(sizeof(struct server_ctx), 1);
	if(!ctx)
	{
		printf("Can't allocate server_ctx struct!\n");
		return NULL;
	}

	// create mtcp context: this will spawn an mtcp thread 
	ctx->mctx = mtcp_create_context(core);
	if (!ctx->mctx) 
	{
		printf("Failed to create mtcp context!\n");
		free(ctx);
		return NULL;
	}

	// create epoll descriptor 
	ctx->ep = mtcp_epoll_create(ctx->mctx, MAX_EVENTS);
	if (ctx->ep < 0) 
	{
		mtcp_destroy_context(ctx->mctx);
		free(ctx);
		printf("Failed to create epoll descriptor!\n");
		return NULL;
	}

	ctx->connections = (struct connection_ctx*) calloc(MAX_FLOW_NUM, sizeof(struct connection_ctx));
	if(!ctx->connections)
	{
		mtcp_close(ctx->mctx, ctx->ep);
		mtcp_destroy_context(ctx->mctx);
		free(ctx);
		printf("Failed allocation memmory for connections pool!\n");
		return NULL;
	}

	return ctx;
}

void ctx_destroy(struct server_ctx* ctx)
{
	if(ctx)
	{
		if(ctx->connections)
		{	/// @todo need checking for not closed connections
			/// or loop with force connection_close
			free(ctx->connections);
		}

		if(ctx->socket >= 0)
		{
			mtcp_epoll_ctl(ctx->mctx, ctx->ep, MTCP_EPOLL_CTL_DEL, ctx->socket, NULL);
			mtcp_close(ctx->mctx, ctx->socket);
		}

		if(ctx->mctx)
		{
			mtcp_destroy_context(ctx->mctx);
		}
		free(ctx);
	}
}

int listen_socket(struct server_ctx* ctx, int queue_size, in_addr_t ip, in_port_t port)
{
	struct sockaddr_in saddr;
	struct mtcp_epoll_event ev;

	ctx->socket = mtcp_socket(ctx->mctx, AF_INET, SOCK_STREAM, 0);
	if(ctx->socket < 0) 
	{
		printf("Failed to create listening socket!\n");
		return -1;
	}

	int result = mtcp_setsock_nonblock(ctx->mctx, ctx->socket);
	if(result < 0) 
	{
		printf("Failed to set socket in nonblocking mode.\n");
		return -1;
	}

	//saddr.sin_addr.s_addr = INADDR_ANY;
	//saddr.sin_port = htons(80);
	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = ip;
	saddr.sin_port = port;

	result = mtcp_bind(ctx->mctx, ctx->socket, 
			(struct sockaddr *)&saddr, sizeof(struct sockaddr_in));
	if (result < 0) 
	{
		printf("Failed to bind to the listening socket!\n");
		return -1;
	}

	result = mtcp_listen(ctx->mctx, ctx->socket, queue_size);
	if(result < 0)
	{
		printf("mtcp_listen() failed!\n");
		return -1;
	}
	
	// wait for incoming accept events 
	//ev.events = MTCP_EPOLLIN;
	ev.events = MTCP_EPOLLIN|MTCP_EPOLLRDHUP|MTCP_EPOLLET;
	ev.data.sockid = ctx->socket;
	mtcp_epoll_ctl(ctx->mctx, ctx->ep, MTCP_EPOLL_CTL_ADD, ctx->socket, &ev);

	return 0;
}

int accept_connection(struct server_ctx *ctx)
{
	struct connection_ctx* conn_ctx;
	struct mtcp_epoll_event ev;
	int c_socket;

	c_socket = mtcp_accept(ctx->mctx, ctx->socket, NULL, NULL);
	if(c_socket >= 0) 
	{
		if(c_socket >= MAX_FLOW_NUM) 
		{
			printf("Invalid socket id %d.\n", c_socket);
			return -1;
		}

		conn_ctx = &ctx->connections[c_socket];
		memset(conn_ctx, 0, sizeof(struct connection_ctx));
		conn_ctx->flushed = 1;

		printf("New connection %d accepted.\n", c_socket);
		ev.events = MTCP_EPOLLIN;
		ev.data.sockid = c_socket;

		mtcp_setsock_nonblock(ctx->mctx, c_socket);
		mtcp_epoll_ctl(ctx->mctx, ctx->ep, MTCP_EPOLL_CTL_ADD, c_socket, &ev);

		printf("Socket %d registered.\n", c_socket);
	} 
	else 
	{
		if (errno != EAGAIN) 
		{
			printf("mtcp_accept() error %s\n", strerror(errno));
		}
	}
	return c_socket;
}

void close_connection(struct server_ctx* ctx, int c_socket)
{
	mtcp_epoll_ctl(ctx->mctx, ctx->ep, MTCP_EPOLL_CTL_DEL, c_socket, NULL);
	mtcp_close(ctx->mctx, c_socket);
}

int write_handler(struct server_ctx* ctx, int c_socket)
{
	uint32_t sent = 0;

	struct mtcp_epoll_event ev;
	struct connection_ctx* conn_ctx;

	conn_ctx = &ctx->connections[c_socket];
	if(!conn_ctx->flushed)
	{
		uint32_t w_size = MIN(SNDBUF_SIZE, (conn_ctx->size <= conn_ctx->offset ? 0 : conn_ctx->size - conn_ctx->offset));
		if(w_size > 0) 
		{
			sent = mtcp_write(ctx->mctx, c_socket, conn_ctx->buffer + conn_ctx->offset, w_size);
			if (sent < 0) 
			{
				return sent;
			}

			printf("Socket %d: mtcp_write try: %d, ret: %d\n", c_socket, w_size, sent);
			conn_ctx->offset += sent;
		}
		else
		{	//nothing to write, buffer sended to client, disable EPOLLOUT event
			memset(conn_ctx, 0, sizeof(struct connection_ctx));
			conn_ctx->flushed = 1;

			ev.events = MTCP_EPOLLIN;
			ev.data.sockid = c_socket;
			mtcp_epoll_ctl(ctx->mctx, ctx->ep, MTCP_EPOLL_CTL_MOD, c_socket, &ev);
		}
	}
	return sent;
}

int read_handler(struct server_ctx* ctx, int c_socket)
{
	struct mtcp_epoll_event ev;
	struct connection_ctx* conn_ctx;

	uint32_t size = 0;
	conn_ctx = &ctx->connections[c_socket];

	if(conn_ctx->flushed)
	{
		uint32_t read_size = MIN(RCVBUF_SIZE, CBUF_SIZE);
		size = mtcp_read(ctx->mctx, c_socket, conn_ctx->buffer, read_size);
		if(size <= 0) 
		{	
			return size;
		}
		conn_ctx->flushed = 0;
		conn_ctx->size += size;

		ev.events = MTCP_EPOLLOUT;
		ev.data.sockid = c_socket;
		mtcp_epoll_ctl(ctx->mctx, ctx->ep, MTCP_EPOLL_CTL_MOD, c_socket, &ev);
	}
	return size;
}

int worker(int core)
{
	struct mtcp_epoll_event *events;
	struct server_ctx* ctx;
	int socket;

	int nevents;
	int do_accept;

	ctx = ctx_init(core);
	if(!ctx)
	{
		printf("Initialize of server_ctx failed!\n");
		return 1;
	}

	events = (struct mtcp_epoll_event *)
			calloc(MAX_EVENTS, sizeof(struct mtcp_epoll_event));

	if (!events)
	{
		ctx_destroy(ctx);
		printf("Failed to create event struct!\n");
		return 2;
	}

	int result = listen_socket(ctx, 4096, INADDR_ANY, htons(8080));
	if (result != 0) 
	{
		free(events);
		ctx_destroy(ctx);
		printf("Failed to create listening socket.\n");
		return 3;
	}

	//while(is_active)
	while(*state_flag)
	{
		nevents = mtcp_epoll_wait(ctx->mctx, ctx->ep, events, MAX_EVENTS, WAIT_TIMEOUT_MS);
		if (nevents < 0) 
		{
			if (errno != EINTR) 
			{
				perror("mtcp_epoll_wait");
			}
			printf("loop break\n");
			break;
		}

		do_accept = FALSE;
		for (int i = 0; i < nevents; i++) 
		{
			if (events[i].data.sockid == ctx->socket) 
			{
				//if the event is for the listener, accept connection
				do_accept = TRUE;
			} 
			else if (events[i].events & MTCP_EPOLLERR) 
			{
				int err;
				socklen_t len = sizeof(err);

				//error on the connection
				printf("[CPU %d] Error on socket %d\n", core, events[i].data.sockid);

				result = mtcp_getsockopt(ctx->mctx, events[i].data.sockid, SOL_SOCKET, SO_ERROR, (void *)&err, &len);
				if (result == 0) 
				{
					if (err != ETIMEDOUT) 
					{
						fprintf(stderr, "Error on socket %d: %s\n", 
								events[i].data.sockid, strerror(err));
					}
				} 
				else 
				{
					printf("mtcp_getsockopt");
				}
				close_connection(ctx, events[i].data.sockid);
			} 
			else if (events[i].events & MTCP_EPOLLIN)
			{
				printf("Read handler in this place\n");
				read_handler(ctx, events[i].data.sockid);
			} 
			else if (events[i].events & MTCP_EPOLLOUT) 
			{
				printf("Write handler in this place\n");
				write_handler(ctx, events[i].data.sockid);
			} 
			else 
			{	//shit happens
				assert(0);
			}
		}

		if(do_accept)
		{
			result = accept_connection(ctx);
			if(result < 0)
			{
				printf("Accept connection failed, err: %d!\n", result);
			}
		}
	}

	printf("Worker is stoped!\n");

	free(events);
	ctx_destroy(ctx);

	return 0;
}

//void mtcp_loop()
//{
	//struct mtcp_conf mcfg;

	//int result = mtcp_init(conf_file);
	//if(result) 
	//{
		//printf("Failed to initialize mtcp\n");
		//return;
	//}

	////mtcp_getconf(&mcfg);
		////mcfg.num_cores = 1;
	////mtcp_setconf(&mcfg);

	////mtcp_register_signal(SIGINT, signal_handler);
	////mtcp_register_signal(SIGTERM, signal_handler);
	////signal(SIGALRM, sys_timer);

	//result = worker(0);
	//if(result != 0)
	//{
		//printf("somthing wrong, app stoped, result: %d\n", result);
	//}

	//mtcp_destroy();
//}

static int watchdog_worker(void* data);
static int run_server_worker(void* data);
static int watchdog_state_handler(process_state_t* state, void* data);

static process_config_t watchdog_process = { watchdog_worker, NULL };
static process_config_t server_process = { run_server_worker, NULL };

static watchdog_process_config_t workers[] = {
	{ &server_process, watchdog_state_handler /* state_handler */, NULL /* state_handler data */ },
	{ &server_process, watchdog_state_handler /* state_handler */, NULL /* state_handler data */ },
	{ &server_process, watchdog_state_handler /* state_handler */, NULL /* state_handler data */ }
};

static void watchdog_signal_handler(int sig)
{
	//printf("watchdog_signal_handler called, sig: %d\n", sig);
	is_active = FALSE;
	if(watchdog_signal_all(sig) != NMINX_OK)
	{
		printf("Failed pass signal: %i to childrens\n", sig);
	}
}

static void server_signal_handler(int sig)
{
	is_active = FALSE;
}

static int server_worker(void* data)
{
	printf("server_worker started\n");
	if(!data)
	{
		printf("Main config is NULL\n");
		return NMINX_ERROR;
	}
	nminx_config_t* n_cfg = (nminx_config_t*) data;

	server_config_t* s_cfg = server_init(n_cfg);
	if(!s_cfg)
	{
		printf("Failed create server!\n");
		return NMINX_ERROR;
	}

	listen_socket_config_t* ls_cfg = listen_socket_open(n_cfg->backlog, n_cfg->ip, n_cfg->port);
	if(!ls_cfg)
	{
		printf("Failed create listen socket!\n");
		server_destroy(s_cfg);
		return NMINX_ERROR;
	}

	s_cfg->l_socket = ls_cfg;
	while(is_active)
	{
		int result = server_process_events(s_cfg);
		if(result == NMINX_ERROR)
		{	// somthing wrong break loop, worker exit
			printf("Server process events error!\n");
			break;
		}
	}

	listen_socket_close(ls_cfg);
	server_destroy(s_cfg);

	return NMINX_OK;
}

static int run_server_worker(void* data)
{
	sigset_t sig_set;
	if(sigfillset(&sig_set) != 0)
	{
		printf("Failed get signal set, error: %s\n", strerror(errno));
		return NMINX_ERROR;
	}

	// condfigure signals
	sigdelset(&sig_set, SIGTERM);
	sigprocmask(SIG_SETMASK, &sig_set, NULL);
	signal(SIGTERM, server_signal_handler);

	return server_worker(data);
}

static int watchdog_state_handler(process_state_t* state, void* data)
{
	if(is_active)
	{
		return NMINX_AGAIN;
	}
	return NMINX_ABORT;
}

static int watchdog_worker(void* data)
{	
	printf("watchdog_worker started\n");

	if(!data)
	{
		printf("Main config is NULL\n");
		return NMINX_ERROR;
	}
	nminx_config_t* n_cfg = (nminx_config_t*) data;

	sigset_t sig_set;
	if(sigfillset(&sig_set) != 0)
	{
		printf("Failed get signal set, error: %s\n", strerror(errno));
		return NMINX_ERROR;
	}

	signal(SIGTERM, watchdog_signal_handler);
	signal(SIGINT, watchdog_signal_handler);

	if(watchdog(workers) != NMINX_OK)
	{
		printf("Failed initialize watchdog!\n");
		return NMINX_ERROR;
	}

	int result = mtcp_init(n_cfg->mtcp_config_path);
	if(result) 
	{
		printf("Failed to initialize mtcp\n");
		return NMINX_ERROR;
	}

	//struct mtcp_conf mcfg = { 0 };
	////mtcp_getconf(&mcfg);
		////mcfg.num_cores = 1;
	////mtcp_setconf(&mcfg);

	while(is_active)
	{
		watchdog_exec(&is_active, n_cfg->wdt_timeout_ms);
	}
	watchdog_stop();

	mtcp_destroy();
	return NMINX_OK;
}

int main(int argc, char* argv[]) 
{
	// initialize application config
	nminx_config_t nminx_cfg = { 0 };
	nminx_cfg.wdt_timeout_ms = 1000;
	nminx_cfg.mtcp_config_path = default_mtcp_config_path;
	nminx_cfg.backlog = default_backlog_size;

	// initialize wdt_process
	//process_config_t wdt_process = { 0 };
	//wdt_process.process = watchdog_worker;
	//wdt_process.data = (void*) &nminx_cfg;
	
	// initialize wdt_process
	process_config_t* wdt_process = &watchdog_process;
	wdt_process->data = (void*) &nminx_cfg;

	// initialize server_process
	process_config_t* srv_process = &server_process;
	srv_process->data = (void*) &nminx_cfg;

	// initialize daemon 
	daemon_config_t cfg = { 0 };
	cfg.process = &wdt_process;

	if(process_daemon(&cfg) != NMINX_OK)
	{
		printf("Failed start daemon process\n");
		return -1;
	}

	return 0;
}
