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

static void server_close_listener(socket_ctx_t* sock);
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
	s_ctx->conf = conf;
	s_ctx->io_ctx = io_ctx;

	socket_ctx_t* l_sock = socket_create(io_ctx);
	if(!l_sock)
	{
		printf("Failed open socket!\n");
		io_destroy(io_ctx);
		return NULL;
	}
	l_sock->data = (void*) s_ctx;
	l_sock->error_hanler = server_close_listener;

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

	s_ctx->sockets[0] = l_sock;

	return s_ctx;
}

void server_destroy(server_ctx_t* s_ctx)
{
	if(s_ctx == &server_ctx)
	{
		// from back, because index 0 is contains a listen socket
		for(int i = MAX_CONNECTIONS; i >= 0; --i)
		{
			if(s_ctx->sockets[i])
			{
				server_rm_socket(s_ctx, s_ctx->sockets[i]);
			}
		}

		if(s_ctx->io_ctx)
		{
			io_destroy(s_ctx->io_ctx);
			s_ctx->io_ctx = NULL;
		}
	}
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

	// timeout handling 
	for(int i = 0; i < nc; ++i)
	{
		socket_ctx_t* sock = sb[i];
		if(sock == NULL)
		{	// bad close logic
			assert(0);
		}

		if(sock->flags & IO_EVENT_ERROR)
		{
			printf("IO_EVENT_ERROR\n");
			socket_error_action(sock);
		}
		else if(sock->flags & IO_EVENT_READ)
		{
			printf("IO_EVENT_READ\n");
			socket_read_action(sock);
		}
		else if(sock->flags & IO_EVENT_WRITE)
		{
			printf("IO_EVENT_WRITE\n");
			socket_write_action(sock);
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

void server_rm_socket(server_ctx_t* s_ctx, socket_ctx_t* sock)
{
	s_ctx->sockets[sock->fd] = NULL;
	socket_close(sock);
	socket_destroy(sock);
}

static void server_close_listener(socket_ctx_t* sock)
{
	server_ctx_t* s_ctx = (server_ctx_t*) sock->data;
	s_ctx->is_run = FALSE;
	server_error_event_handler(s_ctx, sock);
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
	server_rm_socket(s_ctx, sock);
}


