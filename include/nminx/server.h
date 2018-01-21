#ifndef _NMINX_SERVER_H
#define _NMINX_SERVER_H

#include <nminx/nminx.h>

#include <nminx/io.h>
#include <nminx/socket.h>

typedef struct
{
	nminx_config_t* m_cfg;

	io_ctx_t* io_ctx;

	socket_ctx_t* sockets[MAX_CONNECTIONS];
} server_ctx_t;

server_ctx_t* server_init(nminx_config_t* cfg);
int server_destroy(server_ctx_t* srv);

int server_process_events(server_ctx_t* s_cfg);

int server_add_socket(server_ctx_t* s_ctx, socket_ctx_t* socket);

#endif //_NMINX_SERVER_H
