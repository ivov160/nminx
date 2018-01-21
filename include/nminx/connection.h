#ifndef __NMINX_CONNECTION_H
#define __NMINX_CONNECTION_H

#include <nminx/nminx.h>
#include <nminx/socket.h>

#define MAX_CONNECTIONS 10000
//#define MAX_CONNECTIONS 2

#define RCVBUF_SIZE (2*1024)
#define SNDBUF_SIZE (8*1024)

#define CBUF_SIZE (RCVBUF_SIZE*2)

typedef struct 
{
	char buffer[CBUF_SIZE];
	uint32_t size;

	uint32_t offset;
	uint8_t flushed;
} connection_ctx_t;

int connection_read_handler(socket_ctx_t* socket);
int connection_write_handler(socket_ctx_t* socket);
int connection_close_handler(socket_ctx_t* socket);

#endif // __NMINX_CONNECTION_H
