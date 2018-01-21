#ifndef _NMINX_SOCKET_H
#define _NMINX_SOCKET_H

#include <nminx/nminx.h>

#include <arpa/inet.h>

typedef struct io_ctx_s io_ctx_t;

typedef struct socket_ctx_s socket_ctx_t;
typedef int (*accpet_handler)(struct socket_ctx_s*);

struct socket_ctx_s
{
	int fd;
	int flags;

	io_ctx_t* io;
	void* data;

	accpet_handler read;
	accpet_handler write;
	accpet_handler close;
};


socket_ctx_t* socket_create(io_ctx_t* io);
int socket_destroy(socket_ctx_t* sock);

int socket_bind(socket_ctx_t* socket, in_addr_t ip, in_port_t port);
int socket_listen(socket_ctx_t* socket, int backlog);

socket_ctx_t* socket_accept(socket_ctx_t* socket);
int socket_close(socket_ctx_t* socket);

int socket_get_option(socket_ctx_t* sock, int level, int opt, void* data, socklen_t* len);
int socket_set_option(socket_ctx_t* sock, int level, int opt, const void* data, socklen_t len);

ssize_t
socket_read(socket_ctx_t* socket, char *buf, size_t len);

ssize_t
socket_recv(socket_ctx_t* socket, char *buf, size_t len, int flags);

/* readv should work in atomic */
int
socket_readv(socket_ctx_t* socket, const struct iovec *iov, int numIOV);

ssize_t
socket_write(socket_ctx_t* socket, const char *buf, size_t len);

/* writev should work in atomic */
int
socket_writev(socket_ctx_t* socket, const struct iovec *iov, int numIOV);

// macros for passing socket to self handlers
#define socket_read_action(sock) sock->read(sock)
#define socket_write_action(sock) sock->write(sock)
#define socket_close_action(sock) sock->close(sock)

#endif //_NMINX_SOCKET_H
