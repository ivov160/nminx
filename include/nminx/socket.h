#ifndef _NMINX_SOCKET_H
#define _NMINX_SOCKET_H

#include <nminx/nminx.h>

typedef void (*event_handler_t)(struct socket_ctx_s*);
typedef void (*cleanup_handler_t)(void*);

struct socket_ctx_s
{
	int fd;
	int flags;

	io_ctx_t* io;

	void* data;
	//cleanup_handler_t cleanup_handler;

	/**
	 * @todo	Implement event handlers as chain.
	 * 
	 *			When logic grow in depth every new layer 
	 *			must copy prev handler or reimplement him.
	 *			This is first step to copy&paste, it is bad idea
	 *			If event handling wil be a chain every new layer of logic 
	 *			just add self handler to a chain head. 
	 *			And do not touch prev event handlers.
	 */
	event_handler_t read_handler;
	event_handler_t write_handler;
	event_handler_t error_hanler;
};


socket_ctx_t* socket_create(io_ctx_t* io);
void socket_destroy(socket_ctx_t* sock);

void socket_stub_action(socket_ctx_t* socket);

int socket_bind(socket_ctx_t* socket, in_addr_t ip, in_port_t port);
int socket_listen(socket_ctx_t* socket, int backlog);

socket_ctx_t* socket_accept(socket_ctx_t* socket);
int socket_close(socket_ctx_t* socket);

int socket_get_option(socket_ctx_t* sock, int level, int opt, void* data, socklen_t* len);
int socket_set_option(socket_ctx_t* sock, int level, int opt, const void* data, socklen_t len);

/**
 * @todo	incapsulate io error handling to socket io functions
 *			example in socket_read
 */
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
#define socket_read_action(sock) sock->read_handler(sock)
#define socket_write_action(sock) sock->write_handler(sock)
#define socket_error_action(sock) sock->error_hanler(sock)

#endif //_NMINX_SOCKET_H
