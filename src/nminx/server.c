#include <nminx/server.h>

#include <nminx/io.h>
#include <nminx/socket.h>

#include <stdio.h>
#include <stdlib.h>

#include <error.h>
#include <errno.h>
#include <string.h>

#include <assert.h>

// Temporary solve, no multithreading but no memory leak. 
// In plan for continue need custom memory allocation
static server_ctx_t server_ctx = { 0 };

static int server_close_listener(socket_ctx_t* sock);
static int server_close_socket(server_ctx_t* s_ctx, socket_ctx_t* sock);

static void server_io_event_handler(server_ctx_t* s_ctx, socket_ctx_t* sock, int rc);
static void server_error_event_handler(server_ctx_t* s_ctx, socket_ctx_t* sock);

server_ctx_t* server_init(config_t* conf)
{
	mtcp_config_t* io_conf = get_io_conf(conf);
	server_config_t* serv_conf = get_serv_conf(conf);

	server_ctx_t* s_ctx = &server_ctx;
	if(s_ctx->io_ctx)
	{
		return s_ctx;
	}

	io_ctx_t* io_ctx = io_init(conf);
	if(!io_ctx)
	{
		printf("Failed initialize io system!\n");
		return NULL;
	}

	socket_ctx_t* l_sock = socket_create(io_ctx);
	if(!l_sock)
	{
		printf("Failed open socket!\n");
		io_destroy(io_ctx);
		return NULL;
	}
	l_sock->data = (void*) s_ctx;
	l_sock->close_handler = server_close_listener;

	in_addr_t port = serv_conf->port == 0 ? htons(DEFAULT_PORT) : serv_conf->port;
	if(socket_bind(l_sock, serv_conf->ip, port) == NMINX_ERROR)
	{
		printf("Failed bind socket!\n");
		socket_destroy(l_sock);
		io_destroy(io_ctx);
		return NULL;
	}

	if(socket_listen(l_sock, serv_conf->backlog) == NMINX_ERROR)
	{
		printf("Failed start listen socket!\n");
		socket_destroy(l_sock);
		io_destroy(io_ctx);
		return NULL;
	}

	if(serv_conf->listener_init_handler(conf, l_sock) == NMINX_ERROR)
	{
		printf("Failed initilize listner socket handlers!\n");
		socket_destroy(l_sock);
		io_destroy(io_ctx);
		return NULL;
	}

	s_ctx->conf = conf;
	s_ctx->io_ctx = io_ctx;
	s_ctx->sockets[0] = l_sock;

	return s_ctx;
}

int server_destroy(server_ctx_t* s_ctx)
{
	if(s_ctx == &server_ctx)
	{
		// from back, because index 0 is contains a listen socket
		for(int i = MAX_CONNECTIONS; i >= 0; --i)
		{
			if(s_ctx->sockets[i])
			{
				socket_ctx_t* sock = s_ctx->sockets[i];

				// finilize socket and destory 
				socket_close_action(sock);
				socket_destroy(sock);

				s_ctx->sockets[i] = NULL;
			}
		}

		if(s_ctx->io_ctx)
		{
			io_destroy(s_ctx->io_ctx);
			s_ctx->io_ctx = NULL;
		}
	}
	return NMINX_OK;
}

int server_process_events(server_ctx_t* s_ctx)
{
	io_ctx_t* io = s_ctx->io_ctx;

	socket_ctx_t* sb[MAX_CONNECTIONS] = { 0 };
	int nc = io_poll_events(io, sb, MAX_CONNECTIONS);

	if (nc < 0) 
	{
		return errno != EINTR ? NMINX_ERROR : NMINX_ABORT;
	}

	for(int i = 0; i < nc; ++i)
	{
		socket_ctx_t* sock = sb[i];
		if(sock->flags & IO_EVENT_ERROR)
		{
			//printf("IO_EVENT_ERROR\n");
			server_error_event_handler(s_ctx, sock);
		}
		else if(sock->flags & IO_EVENT_READ)
		{
			//printf("IO_EVENT_READ\n");
			//printf("socket_read_action: %d\n", result);
			int rc = socket_read_action(sock);
			server_io_event_handler(s_ctx, sock, rc);
		}
		else if(sock->flags & IO_EVENT_WRITE)
		{
			//printf("IO_EVENT_WRITE\n");
			//printf("socket_write_action: %d\n", result);
			int rc = socket_write_action(sock);
			server_io_event_handler(s_ctx, sock, rc);
		}
		else
		{	// unexpected flag state
			assert(0);
		}
	}

	return NMINX_OK;
}

int server_add_socket(server_ctx_t* s_ctx, socket_ctx_t* sock)
{
	socket_ctx_t* cs = s_ctx->sockets[sock->fd];
	if(cs != NULL)
	{
		printf("Duplicate socket fd: %d\n", sock->fd);
		return NMINX_ERROR;
	}

	s_ctx->sockets[sock->fd] = sock;
	return NMINX_OK;
}

static int server_close_listener(socket_ctx_t* sock)
{
	server_ctx_t* s_ctx = (server_ctx_t*) sock->data;
	s_ctx->is_run = FALSE;

	sock->close_handler = socket_destroy;
	return server_close_socket(s_ctx, sock);
}

int server_close_socket(server_ctx_t* s_ctx, socket_ctx_t* sock)
{
	socket_close_action(sock);

	s_ctx->sockets[sock->fd] = NULL;
	return NMINX_OK;
}

static void server_io_event_handler(server_ctx_t* s_ctx, socket_ctx_t* sock, int rc)
{
	if(rc == NMINX_ERROR)
	{
		printf("Event failed socket %d, err: %s\n", 
				sock->fd, strerror(errno));
	}
	
	if(rc != NMINX_AGAIN)
	{
		server_close_socket(s_ctx, sock);
	}
}

static void server_error_event_handler(server_ctx_t* s_ctx, socket_ctx_t* sock)
{
	int err = 0;
	socklen_t len = sizeof(err);

	if(socket_get_option(sock, SOL_SOCKET, SO_ERROR, (void*) &err, &len) == NMINX_OK)
	{
		if(err != ETIMEDOUT)
		{
			printf("Error on socket %d, error: %s\n", 
					sock->fd, strerror(err));
		}
		else
		{
			printf("Timeout on socket %d\n", sock->fd);
		}
	}
	else
	{
		printf("socket_get_option error\n");
	}
	server_close_socket(s_ctx, sock);
}


