#include <nminx/server.h>

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
	if(s_ctc->io_ctx)
	{
		return &s_ctx;
	}

	s_cfg->m_cfg = m_cfg;
	s_cfg->io_ctx = io_init(m_cfg);
	if(!s_cfg->io_ctx)
	{
		printf("Failed initialize io system!\n");
		return NULL;
	}


	//listen_socket_open(

	///@todo initialize connections set flags by defailt and etc.
	//s_cfg->connections = (struct connection_ctx*) calloc(MAX_FLOW_NUM, sizeof(struct connection_ctx));
	//if(!s_cfg->connections)
	//{
		//mtcp_destroy_context(s_cfg->mctx);
		//free(ctx);
		//printf("Failed allocation memmory for connections pool!\n");
		//return NULL;
	//}
	return s_ctx;
}

int server_destroy(server_ctx_t* s_ctx)
{
	if(s_ctx == &server_ctx)
	{
		///@todo finilize connextion
		//if(s_cfg->connections)
		//{	
			///// or loop with force connection_close
			//free(s_cfg->connections);
		//}

		//if(s_cfg->socket >= 0)
		//{
			//mtcp_epoll_ctl(s_cfg->mctx, s_cfg->ep, MTCP_EPOLL_CTL_DEL, s_cfg->socket, NULL);
			////mtcp_close(s_cfg->mctx, s_cfg->socket);
		//}

		if(s_ctx->io_ctx)
		{
			io_destroy(s_ctx->io_ctx);
			s_ctx->io_ctx = NULL;
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
			connection_ctx_t* c_connect = &s_cfg->connections[c_socket];
		}
	}
	return NMINX_OK;
}
