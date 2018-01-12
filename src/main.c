#include <stdio.h>
#include <stdlib.h>

#include <error.h>
#include <errno.h>
#include <string.h>

#include <signal.h>
#include <assert.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <mtcp_api.h>
#include <mtcp_epoll.h>

#define MAX_FLOW_NUM  (10000)

#define RCVBUF_SIZE (2*1024)
#define SNDBUF_SIZE (8*1024)

#define MAX_EVENTS (MAX_FLOW_NUM * 3)

#define TRUE 1;
#define FALSE 0;

static int is_active = TRUE;
static char *conf_file = "config/nminx.conf";

struct server_ctx
{
	mctx_t mctx;
	int ep;
	int socket;

	//struct server_vars *vars;
};

/*----------------------------------------------------------------------------*/
void signal_handler(int signum)
{
	is_active = FALSE;
	//int i;

	//for (i = 0; i < core_limit; i++) {
		//if (app_thread[i] == pthread_self()) {
			////TRACE_INFO("Server thread %d got SIGINT\n", i);
			//done[i] = TRUE;
		//} else {
			//if (!done[i]) {
				//pthread_kill(app_thread[i], signum);
			//}
		//}
	//}
}

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

	/* create mtcp context: this will spawn an mtcp thread */
	ctx->mctx = mtcp_create_context(core);
	if (!ctx->mctx) 
	{
		printf("Failed to create mtcp context!\n");
		free(ctx);
		return NULL;
	}

	/* create epoll descriptor */
	ctx->ep = mtcp_epoll_create(ctx->mctx, MAX_EVENTS);
	if (ctx->ep < 0) 
	{
		mtcp_destroy_context(ctx->mctx);
		free(ctx);
		printf("Failed to create epoll descriptor!\n");
		return NULL;
	}

	/* allocate memory for server variables */
	//ctx->vars = (struct server_vars*) calloc(MAX_FLOW_NUM, sizeof(struct server_vars));
	//if (!ctx->vars) 
	//{
		//mtcp_close(ctx->mctx, ctx->ep);
		//mtcp_destroy_context(ctx->mctx);
		//free(ctx);
		//printf("Failed to create server_vars struct!\n");
		//return NULL;
	//}

	return ctx;
}

void ctx_destroy(struct server_ctx* ctx)
{
	if(ctx)
	{
		//if(ctx->vars)
		//{
		//}

		//if(ctx->ep >= 0)
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
	int result;
	int socket;

	struct sockaddr_in saddr;
	struct mtcp_epoll_event ev;

	socket = mtcp_socket(ctx->mctx, AF_INET, SOCK_STREAM, 0);
	if(socket < 0) 
	{
		printf("Failed to create listening socket!\n");
		return -1;
	}

	result = mtcp_setsock_nonblock(ctx->mctx, socket);
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

	result = mtcp_bind(ctx->mctx, socket, 
			(struct sockaddr *)&saddr, sizeof(struct sockaddr_in));
	if (result < 0) 
	{
		printf("Failed to bind to the listening socket!\n");
		return -1;
	}


	result = mtcp_listen(ctx->mctx, socket, queue_size);
	if(result < 0)
	{
		printf("mtcp_listen() failed!\n");
		return -1;
	}
	
	/* wait for incoming accept events */
	ev.events = MTCP_EPOLLIN;
	ev.data.sockid = socket;
	mtcp_epoll_ctl(ctx->mctx, ctx->ep, MTCP_EPOLL_CTL_ADD, socket, &ev);

	return socket;
}

int accept_connection(struct server_ctx *ctx)
{
	//struct server_vars *sv;
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

		//sv = &ctx->svars[c];
		//CleanServerVariable(sv);
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

	ctx->socket = listen_socket(ctx, 4096, INADDR_ANY, htons(8080));
	//ctx->socket = listen_socket(ctx, 4096, inet_addr("127.0.0.1"), htons(8080));
	if (ctx->socket < 0) 
	{
		ctx_destroy(ctx);
		printf("Failed to create listening socket.\n");
		return 3;
	}

	while(is_active)
	{
		printf("in worker cycle\n");
		nevents = mtcp_epoll_wait(ctx->mctx, ctx->ep, events, MAX_EVENTS, -1);
		if (nevents < 0) 
		{
			if (errno != EINTR) 
			{
				perror("mtcp_epoll_wait");
			}
			break;
		}

		do_accept = FALSE;
		for (int i = 0; i < nevents; i++) 
		{
			if (events[i].data.sockid == ctx->socket) 
			{
				/* if the event is for the listener, accept connection */
				do_accept = TRUE;
			} 
			else if (events[i].events & MTCP_EPOLLERR) 
			{
				int err;
				socklen_t len = sizeof(err);

				/* error on the connection */
				printf("[CPU %d] Error on socket %d\n", core, events[i].data.sockid);


				int result = mtcp_getsockopt(ctx->mctx, events[i].data.sockid, SOL_SOCKET, SO_ERROR, (void *)&err, &len);
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
				//CloseConnection(ctx, events[i].data.sockid, 
						//&ctx->svars[events[i].data.sockid]);
			} 
			else if (events[i].events & MTCP_EPOLLIN)
			{
				printf("Read handler in this place\n");
				//ret = HandleReadEvent(ctx, events[i].data.sockid, 
						//&ctx->svars[events[i].data.sockid]);

				//if (ret == 0) {
					//[> connection closed by remote host <]
					//CloseConnection(ctx, events[i].data.sockid, 
							//&ctx->svars[events[i].data.sockid]);
				//} else if (ret < 0) {
					//[> if not EAGAIN, it's an error <]
					//if (errno != EAGAIN) {
						//CloseConnection(ctx, events[i].data.sockid, 
								//&ctx->svars[events[i].data.sockid]);
					//}
				//}

			} 
			else if (events[i].events & MTCP_EPOLLOUT) 
			{
				printf("Write handler in this place\n");
				//struct server_vars *sv = &ctx->svars[events[i].data.sockid];
				//if (sv->rspheader_sent) {
					//SendUntilAvailable(ctx, events[i].data.sockid, sv);
				//} else {
					//TRACE_APP("Socket %d: Response header not sent yet.\n", 
							//events[i].data.sockid);
				//}

			} else {
				assert(0);
			}
		}

		if(do_accept)
		{
			while (1)
			{
				int result = accept_connection(ctx);
				if (result < 0) break;
			}
		}
	}

	printf("Worker is stoped!\n");
	ctx_destroy(ctx);

	return 0;
}


int main(int argc, char* argv[]) 
{
	struct mtcp_conf mcfg;

	int result = mtcp_init(conf_file);
	if(result) 
	{
		printf("Failed to initialize mtcp\n");
		return 0;
	}

	//mtcp_getconf(&mcfg);
		//mcfg.num_cores = 1;
	//mtcp_setconf(&mcfg);

	/* register signal handler to mtcp */
	mtcp_register_signal(SIGINT, signal_handler);

	result = worker(0);
	if(result != 0)
	{
		printf("somthing wrong, app stoped, result: %d\n", result);
	}

	//atexit(mtcp_destroy());
	mtcp_destroy();

	return 0;
}
