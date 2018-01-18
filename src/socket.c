#include <nminx/socket.h>

#include <mtcp_api.h>
#include <mtcp_epoll.h>


static int listen_socket_open_fd(io_ctx_t* io, nminx_config_t* m_cfg);
static int listen_socket_close_fd(io_ctx_t* io, int s_fd);

//listen_socket_config_t* listen_socket_open(int backlog, in_addr_t ip, in_port_t port)
socket_ctx_t* socket_open(io_ctx_t* io, nminx_config_t* m_cfg)
{
	int socket_fd = listen_socket_open_fd(io, m_cfg);
	if(socket_fd < 0)
	{
		printf("Failed open socket fd, error: %s\n", strerror(errno));
		return NULL;
	}

	listen_socket_config_t* l_socket = (listen_socket_config_t*)
		calloc(1, sizeof(listen_socket_config_t));

	if(!l_socket)
	{
		printf("Failed allocation listen_socket_config_t\n");
		listen_socket_close_fd(io, socket_fd);
		return NULL;
	}

	l_socket->socket = socket_fd;
	l_socket->handler

	return 0;
}

int listen_socket_open_fd(io_ctx_t* io, nminx_config_t* m_cfg)
{
	struct sockaddr_in saddr;
	struct mtcp_epoll_event ev;

	int s_fd = mtcp_socket(io->mctx, AF_INET, SOCK_STREAM, 0);
	if(s_fd < 0) 
	{
		printf("Failed create listen socket fd!\n");
		return NMINX_ERROR;
	}

	int result = mtcp_setsock_nonblock(io->mctx, s_fd);
	if(result < 0) 
	{
		printf("Failed to set socket in nonblocking mode.\n");
		listen_socket_close_fd(io, s_fd);
		return NMINX_ERROR;
	}

	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = m_cfg->ip;
	saddr.sin_port = m_cfg->port;

	result = mtcp_bind(s_cfg->mctx, s_fd, 
			(struct sockaddr *)&saddr, sizeof(struct sockaddr_in));

	if (result < 0) 
	{
		printf("Failed to bind to the listening socket!\n");
		listen_socket_close_fd(s_cfg, s_fd);
		return NMINX_ERROR;
	}

	result = mtcp_listen(s_cfg->mctx, s_fd, m_cfg->backlog);
	if(result < 0)
	{
		printf("mtcp_listen() failed!\n");
		listen_socket_close_fd(s_cfg, s_fd);
		return NMINX_ERROR;
	}
	
	// wait for incoming accept events 
	//ev.events = MTCP_EPOLLIN;
	ev.events = MTCP_EPOLLIN|MTCP_EPOLLRDHUP|MTCP_EPOLLET;
	ev.data.sockid = s_fd
	mtcp_epoll_ctl(s_cfg->mctx, s_cfg->ep, MTCP_EPOLL_CTL_ADD, s_fd, &ev);

	return s_fd;
}

int listen_socket_close(listen_socket_config_t* socket)
{
}

int listen_socket_close_fd(io_ctx_t* io, int s_fd);
{
	mtcp_epoll_ctl(io->mctx, io->ep, MTCP_EPOLL_CTL_DEL, s_fd, NULL);
	mtcp_close(io->mctx, s_fd);

	return NMINX_OK;
}

