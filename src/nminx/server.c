#include <nminx/server.h>
//#include <nminx/connection.h>
#include <nminx/http_connection.h>

#include <stdio.h>
#include <stdlib.h>

#include <error.h>
#include <errno.h>
#include <string.h>

#include <assert.h>

// Temporary solve, no multithreading but no memory leak. 
// In plan for continue need custom memory allocation
static server_ctx_t server_ctx = { 0 };

//static int server_accept_socket_handler(socket_ctx_t* socket);
static int server_stub_socket_handler(socket_ctx_t* socket);
static int server_close_socket_handler(socket_ctx_t* socket);

static int server_close_socket(server_ctx_t* s_ctx, socket_ctx_t* socket);

server_ctx_t* server_init(nminx_config_t* m_cfg)
{
	server_ctx_t* s_ctx = &server_ctx;
	if(s_ctx->io_ctx)
	{
		return s_ctx;
	}

	io_ctx_t* io_ctx = io_init(m_cfg);
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

	if(socket_bind(l_sock, m_cfg->ip, m_cfg->port) == NMINX_ERROR)
	{
		printf("Failed bind socket!\n");
		socket_destroy(l_sock);
		io_destroy(io_ctx);
		return NULL;
	}

	if(socket_listen(l_sock, m_cfg->backlog) == NMINX_ERROR)
	{
		printf("Failed start listen socket!\n");
		socket_destroy(l_sock);
		io_destroy(io_ctx);
		return NULL;
	}

	if(io_poll_ctl(io_ctx, IO_CTL_ADD, IO_EVENT_READ, l_sock) == NMINX_ERROR)
	{
		printf("Failed attach read event to socket!\n");
		socket_destroy(l_sock);
		io_destroy(io_ctx);
		return NULL;
	}
	
	l_sock->data = (void*) s_ctx;
	l_sock->read_handler = http_connection_accept;
	l_sock->write_handler = server_stub_socket_handler;
	l_sock->close_handler = server_stub_socket_handler;
	//l_sock->close = server_close_socket_handler;

	s_ctx->m_cfg = m_cfg;
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

///@todo need add errors handling
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
			printf("IO_EVENT_ERROR\n");
			
			int err = 0;
			socklen_t len = sizeof(err);

			int result = socket_get_option(sock, SOL_SOCKET, SO_ERROR, (void*) &err, &len);
			if(result == NMINX_OK)
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
		else if(sock->flags & IO_EVENT_READ)
		{
			printf("IO_EVENT_READ\n");
			int result = socket_read_action(sock);
			printf("socket_read_action: %d\n", result);

			if(result < NMINX_OK)
			{
				if(result != NMINX_AGAIN)
				{
					printf("Failed read date from socket %d, err: %s\n", 
							sock->fd, strerror(errno));

					server_close_socket(s_ctx, sock);
				}
			}
		}
		else if(sock->flags & IO_EVENT_WRITE)
		{
			printf("IO_EVENT_WRITE\n");
			int result = socket_write_action(sock);
			printf("socket_write_action: %d\n", result);
			// result handling
		}
		else
		{	// unexpected flag state
			assert(0);
		}
	}

	return NMINX_OK;
}

int server_add_socket(server_ctx_t* s_ctx, socket_ctx_t* socket)
{
	///@todo collision checking
	s_ctx->sockets[socket->fd] = socket;

	return NMINX_OK;
}

int server_close_socket(server_ctx_t* s_ctx, socket_ctx_t* socket)
{
	socket_close_action(socket);
	s_ctx->sockets[socket->fd] = NULL;
	return NMINX_OK;
}

int server_stub_socket_handler(socket_ctx_t* socket)
{
	return NMINX_OK;
}

/*
int server_accept_socket_handler(socket_ctx_t* socket)
{
	server_ctx_t* s_ctx = (server_ctx_t*) socket->data;
	nminx_config_t* m_cfg = s_ctx->m_cfg;

	socket_ctx_t* c_socket = socket_accept(socket);
	if(!c_socket)
	{
		printf("Failed accept client!\n");
		return NMINX_ERROR;
	}


	http_connection_ctx_t* hc = http_connection_create(m_cfg);
	if(!hc)
	{
		printf("Failed create http_connection!\n");
		socket_close(c_socket);
		return NMINX_ERROR;
	}

	c_socket->data = hc;
	c_socket->cleanup_handler = http_connection_cleanup_handler;

	c_socket->read_handler = http_connection_read_handler;
	c_socket->write_handler = http_connection_write_handler;
	c_socket->close_handler = http_connection_close_handler;

	if(io_poll_ctl(c_socket->io, IO_CTL_ADD, IO_EVENT_READ, c_socket) == NMINX_ERROR)
	{
		printf("Failed attach read event to socket!\n");
		free(conn);
		socket_close_action(c_socket);
		socket_destroy(c_socket);
		return NMINX_ERROR;
	}

	server_ctx_t* s_ctx = (server_ctx_t*) socket->data;
	s_ctx->sockets[c_socket->fd] = c_socket;

	return NMINX_OK;
}

int server_close_socket_handler(socket_ctx_t* socket)
{
	server_ctx_t* s_ctx = (server_ctx_t*) socket->data;
	server_destroy(s_ctx);
	return NMINX_OK;
}
*/


