#ifndef _NMINX_SERVER_H
#define _NMINX_SERVER_H

#include <nminx/nminx.h>
#include <nminx/socket.h>

typedef struct
{
	nminx_cfg* m_cfg;
	listen_socket_config_t* l_socket;

	int ep;
	mctx_t mctx;

	//struct connection_ctx* connections;
} server_config_t;

server_config_t* server_init(nminx_cfg* cfg);
int server_destroy(server_config_t* srv);

int server_process_events(server_config_t* cfg);

#endif //_NMINX_SERVER_H
