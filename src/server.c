#include <nminx/server.h>
#inc

#include <stdio.h>
#include <stdlib.h>

#include <error.h>
#include <errno.h>
#include <string.h>

// Temporary solve, no multithreading but no memory leak. 
// In plan for continue need custom memory allocation
static server_ctx_t server_ctx = { 0 };

server_ctx_t* server_init(nminx_config_t* m_cfg)
{
	server_ctx_t* s_ctx = &server_ctx;
	if(s_ctx->io_ctx)
	{
		return &s_ctx;
	}

	io_ctx_t* io_ctx = io_init(m_cfg);
	if(!s_ctx->io_ctx)
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

	if(socket_listen(l_sock) == NMINX_ERROR)
	{
		printf("Failed start listen socket!\n");
		socket_destroy(l_sock);
		io_destroy(io_ctx);
		return NULL;
	}
	
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
				socket_ctx_t* sock = &s_ctx->sockets[i];

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
int server_process_events(server_ctx_t* s_cfg)
{
	io_ctx_t* io = s_cfg->io;

	socket_ctx_t sb[MAX_CONNECTIONS] = { 0 };
	int nc = io_process_events(io, &sb, MAX_CONNECTIONS);

	if (nc < 0) 
	{
		return errno != EINTR ? NMINX_ERROR : NMINX_ABORT;
	}

	for(int i = 0; i < nc; ++i)
	{
		socket_ctx_t* sock = &sb[i];
		if(sock->flags & SOCK_ERROR)
		{
			int result = socket_close_action(sock);
		}
		else if(sock->flags & SOCK_READ)
		{
			int result = socket_read_action(sock);
			// result handling
		}
		else if(sock->flags & SOCK_WRITE)
		{
			int result = socket_write_action(sock);
			// result handling
		}
		else
		{	// unexpected flag state
			assert(0);
		}
	}

	return NMINX_OK;
}


//int server_process_events(server_ctx_t* cfg)
int server_process_events(server_ctx_t* s_cfg, struct mtcp_epoll_event* eb, int eb_size)
{
	nminx_config_t* m_cfg = s_cfg->m_cfg;
	listen_socket_config_t* l_socket = s_cfg->l_socket;

	/// somthing like that: io_wait
	///@todo replace wdt_timeout_ms for event_poll_timeout
	int nevents = mtcp_epoll_wait(s_cfg->mctx, s_cfg->ep, eb, eb_size, m_cfg->wdt_timeout_ms);
	if (nevents < 0) 
	{
		return errno != EINTR ? NMINX_ERROR : NMINX_ABORT;
	}

	int do_accept = FALSE;
	for (int i = 0; i < nevents; i++) 
	{
		if (eb[i].data.sockid == l_socket->fd)
		{
			//if the event is for the listener, accept connection
			do_accept = TRUE;
		} 
		else if (eb[i].events & MTCP_EPOLLERR) 
		{
			int err;
			socklen_t len = sizeof(err);

			//error on the connection
			//printf("[CPU %d] Error on socket %d\n", core, events[i].data.sockid);
			printf("Error on socket %d\n", eb[i].data.sockid);

			int result = mtcp_getsockopt(s_cfg->mctx, eb[i].data.sockid, SOL_SOCKET, SO_ERROR, (void *)&err, &len);
			if (result == 1) 
			{
				if (err != ETIMEDOUT) 
				{
					printf("Error on socket %d: %s\n", 
							eb[i].data.sockid, strerror(err));
				}
				else
				{
					printf("Timeout on socket %d: %s\n", 
							eb[i].data.sockid, strerror(err));
				}
			} 
			//close_connection(ctx, eb[i].data.sockid);
		} 
		else if (eb[i].events & MTCP_EPOLLIN)
		{
			printf("Read handler in this place\n");
			//read_handler(ctx, eb[i].data.sockid);
		} 
		else if (eb[i].events & MTCP_EPOLLOUT) 
		{
			printf("Write handler in this place\n");
			//write_handler(ctx, eb[i].data.sockid);
		} 
		else 
		{	//shit happens
			assert(0);
		}
	}

	if(do_accept)
	{
		int c_socket = l_socket->handler(l_socket);
		if(c_socket < 0)
		{
			printf("Accept connection failed, err: %d!\n", c_socket);
		}
		else 
		{
			connection_ctx_t* c_connect = &s_ctx->connections[c_socket];
		}
	}
	return NMINX_OK;
}
