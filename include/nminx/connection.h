#ifndef __NMINX_CONNECTION_H
#define __NMINX_CONNECTION_H

#include <nminx/nminx.h>
#include <nminx/socket.h>

#define MAX_CONNECTIONS 10000

typedef struct
{
	uint32_t size;
	socket_ctx_t sock;
} connection_ctx_t;

//int conne


#endif // __NMINX_CONNECTION_H
