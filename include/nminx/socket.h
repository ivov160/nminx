#ifndef _NMINX_SOCKET_H
#define _NMINX_SOCKET_H

#include <nminx/nminx.h>

#include <mtcp_api.h>
#include <arpa/inet.h>
//#include <netinet/in.h>

typedef struct listen_socket_s* listen_socket_config_t;
typedef int (*accpet_handler)(listen_socket_config_t*);

struct listen_socket_config_s
{
	int socket;
	void* data;

	accpet_handler handler;
};

listen_socket_config_t* listen_socket_open(int backlog, in_addr_t ip, in_port_t port);
int listen_socket_close(listen_socket_config_t* socket);


#endif //_NMINX_SOCKET_H
