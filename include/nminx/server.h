#ifndef _NMINX_SERVER_H
#define _NMINX_SERVER_H

#include <nminx/nminx.h>
#include <nminx/config.h>

#define DEFAULT_PORT 8080

struct server_ctx_s
{
	config_t* conf;
	io_ctx_t* io_ctx;
	socket_ctx_t* sockets[MAX_CONNECTIONS];

	int is_run;
};

server_ctx_t* server_init(config_t* conf);
int server_destroy(server_ctx_t* srv);

int server_process_events(server_ctx_t* s_cfg);

int server_add_socket(server_ctx_t* s_ctx, socket_ctx_t* socket);

#endif //_NMINX_SERVER_H
