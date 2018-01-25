#include <nminx/socket.h>
// not good hack, but now all mtcp data hidden in modules
// io implementation (mtcp) can be replaced without big refactoring
#include "io_ctx.h"


void socket_stub_action(socket_ctx_t* socket)
{
	return;
}

socket_ctx_t* socket_create(io_ctx_t* io)
{
	if(io)
	{
		int s_fd = mtcp_socket(io->mctx, AF_INET, SOCK_STREAM, 0);
		if(s_fd < 0) 
		{
			printf("Failed create socket fd!\n");
			return NULL;
		}

		if(mtcp_setsock_nonblock(io->mctx, s_fd) < 0) 
		{
			printf("Failed to set socket in nonblocking mode.\n");
			mtcp_close(io->mctx, s_fd);
			return NULL;
		}

		socket_ctx_t* sock = 
			(socket_ctx_t*) calloc(1, sizeof(socket_ctx_t));

		if(!sock)
		{
			printf("Failed allocation memmory for socket!\n");
			mtcp_close(io->mctx, s_fd);
			return NULL;
		}

		sock->fd = s_fd;
		sock->flags = 0x00;
		sock->io = io;

		sock->read_handler = socket_stub_action;
		sock->write_handler = socket_stub_action;
		sock->error_hanler = socket_stub_action;

		return sock;
	}
	return NULL;
}

void socket_destroy(socket_ctx_t* sock)
{
	if(sock)
	{	
		free(sock);
	}
}

int socket_close(socket_ctx_t* socket)
{
	if(socket)
	{
		///@note may be best choice will be a use io api
		mtcp_epoll_ctl(socket->io->mctx, socket->io->ep, MTCP_EPOLL_CTL_DEL, socket->fd, NULL);
		mtcp_close(socket->io->mctx, socket->fd);
	}
	return NMINX_OK;
}

int socket_bind(socket_ctx_t* socket, in_addr_t ip, in_port_t port)
{
	if(socket)
	{
		struct sockaddr_in saddr;

		saddr.sin_family = AF_INET;
		saddr.sin_addr.s_addr = ip;
		saddr.sin_port = port;

		int result = mtcp_bind(socket->io->mctx, socket->fd, 
				(struct sockaddr *)&saddr, sizeof(struct sockaddr_in));

		return result < 0 ? NMINX_ERROR : NMINX_OK;
	}
	return NMINX_ERROR;
}

int socket_listen(socket_ctx_t* socket, int backlog)
{
	if(socket)
	{
		return (mtcp_listen(socket->io->mctx, socket->fd, backlog) < 0)
			? NMINX_ERROR : NMINX_OK;
	}
	return NMINX_ERROR;
}

socket_ctx_t* socket_accept(socket_ctx_t* socket)
{
	if(socket)
	{
		io_ctx_t* io = socket->io;

		int c_fd = mtcp_accept(io->mctx, socket->fd, NULL, NULL);
		if(c_fd >= 0) 
		{
			if(mtcp_setsock_nonblock(io->mctx, c_fd) < 0) 
			{
				printf("Failed to set socket in nonblocking mode.\n");
				mtcp_close(io->mctx, c_fd);
				return NULL;
			}

			socket_ctx_t* c_socket = 
				(socket_ctx_t*) calloc(1, sizeof(socket_ctx_t));

			if(!c_socket)
			{
				printf("Failed allocation memmory for socket!\n");
				mtcp_close(io->mctx, c_fd);
				return NULL;
			}

			c_socket->io = io;
			c_socket->fd = c_fd;
			c_socket->flags = 0x00;

			c_socket->read_handler = socket_stub_action;
			c_socket->write_handler = socket_stub_action;
			c_socket->error_hanler = socket_stub_action;

			return c_socket;
		} 
		return NULL;
	}
	return NULL;
}

int socket_get_option(socket_ctx_t* sock, int level, int opt, void* data, socklen_t* len)
{
	if(sock)
	{
		int result = mtcp_getsockopt(
			sock->io->mctx, sock->fd, level, opt, data, len);

		return result != 0 ? NMINX_ERROR : NMINX_OK;
	}
	return NMINX_ERROR;
}

int socket_set_option(socket_ctx_t* sock, int level, int opt, const void* data, socklen_t len)
{
	if(sock)
	{
		int result = mtcp_setsockopt(
			sock->io->mctx, sock->fd, level, opt, data, len);

		return result != 0 ? NMINX_ERROR : NMINX_OK;
	}
	return NMINX_ERROR;
}

ssize_t
socket_read(socket_ctx_t* socket, char *buf, size_t len)
{
	if(socket)
	{
		return mtcp_read(socket->io->mctx, socket->fd, buf, len);
	}
	return 0;
}

ssize_t
socket_recv(socket_ctx_t* socket, char *buf, size_t len, int flags)
{
	if(socket)
	{
		return mtcp_recv(socket->io->mctx, socket->fd, buf, len, flags);
	}
	return 0;
}

/* readv should work in atomic */
int
socket_readv(socket_ctx_t* socket, const struct iovec *iov, int numIOV)
{
	if(socket)
	{
		return mtcp_readv(socket->io->mctx, socket->fd, iov, numIOV);
	}
	return 0;
}

ssize_t
socket_write(socket_ctx_t* socket, const char *buf, size_t len)
{
	if(socket)
	{
		return mtcp_write(socket->io->mctx, socket->fd, buf, len);
	}
	return 0;
}

/* writev should work in atomic */
int
socket_writev(socket_ctx_t* socket, const struct iovec *iov, int numIOV)
{
	if(socket)
	{
		return mtcp_writev(socket->io->mctx, socket->fd, iov, numIOV);
	}
	return 0;
}


