#ifndef _NMINX_SOCKET_H
#define _NMINX_SOCKET_H

#include <nminx/nminx.h>

#include <arpa/inet.h>

typedef struct io_ctx_s io_ctx_t;

typedef struct socket_ctx_s socket_ctx_t;
typedef int (*accpet_handler)(struct socket_ctx_s*);

///@todo	think abount io_s and hold pointer to it
///			io_s - mtcp io data mtcp_ctx and mtcp_epoll descriptor
struct socket_ctx_s
{
	int fd;
	int flags;

	io_ctx_t* io;

	accpet_handler read;
	accpet_handler write;
	accpet_handler close;
};


socket_ctx_t* socket_create(io_ctx_t* io);
int socket_destroy(socket_ctx_t* sock);

int socket_bind(socket_ctx_t* socket, in_addr_t ip, in_port_t port);
int socket_listen(socket_ctx_t* socket);

socket_ctx_t* socket_accept(socket_ctx_t* socket);

int socket_close(socket_ctx_t* socket);

int socket_get_option(socket_ctx_t* sock, int level, int opt, void* data, socklen_t* len);
int socket_set_option(socket_ctx_t* sock, int level, int opt, void* data, socklen_t* len);

int socket_read(socket_ctx_t* socket);
int socket_write(socket_ctx_t* socket);

// macros for passing socket to self handlers
#define socket_read_action(sock) sock->read(sock)
#define socket_write_action(sock) sock->write(sock)
#define socket_close_action(sock) sock->close(sock)

#endif //_NMINX_SOCKET_H
