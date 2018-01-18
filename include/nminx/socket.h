#ifndef _NMINX_SOCKET_H
#define _NMINX_SOCKET_H

#include <nminx/nminx.h>
#include <nminx/io.h>
#include <nminx/server.h>

#include <arpa/inet.h>
//#include <netinet/in.h>

//struct socket_config_s;
typedef struct socket_config_s socket_ctx_t;
typedef int (*accpet_handler)(struct socket_config_s*);

///@todo	think abount io_s and hold pointer to it
///			io_s - mtcp io data mtcp_ctx and mtcp_epoll descriptor
struct socket_config_s
{
	int fd;
	void* data;

	accpet_handler handler;

	accpet_handler read_handler;
	accpet_handler write_handler;
};

socket_ctx_t* socket_open(io_ctx_t* io, nminx_config_t* m_cfg);
int socket_close(socket_ctx_t* socket);

int socket_get_option(socket_ctx_t* sock, int level, int opt, void* data, socklen_t* len);
int socket_set_option(socket_ctx_t* sock, int level, int opt, void* data, socklen_t* len);

int socket_accept(socket_ctx_t* socket);
int socket_read(socket_ctx_t* socket);
int socket_write(socket_ctx_t* socket);


#endif //_NMINX_SOCKET_H
