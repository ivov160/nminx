#ifndef __NMINX_HTTP_CONNECTION_H
#define __NMINX_HTTP_CONNECTION_H

#include <nminx/nminx.h>
#include <nminx/socket.h>

typedef struct http_connection_ctx_s http_connection_ctx_t;

http_connection_ctx_t* create_http_connection(nminx_config_t* m_cfg);
int destroy_http_connection(http_connection_ctx_t* conn);

int http_connection_read_handler(socket_ctx_t* socket);
int http_connection_write_handler(socket_ctx_t* socket);
int http_connection_close_handler(socket_ctx_t* socket);

#endif //__NMINX_HTTP_CONNECTION_H
