#ifndef __NMINX_HTTP_CONNECTION_H
#define __NMINX_HTTP_CONNECTION_H

#include <nminx/nminx.h>
#include <nminx/config.h>


http_connection_ctx_t* http_connection_create(config_t* conf);
int http_connection_destroy(http_connection_ctx_t* conn);

//listener initialize function
int http_listner_init_handler(config_t* conf, socket_ctx_t* socket);

// socket API handlers
int http_connection_read_handler(socket_ctx_t* socket);
int http_connection_write_handler(socket_ctx_t* socket);
int http_connection_close_handler(socket_ctx_t* socket);
int http_connection_cleanup_handler(void* data);

int http_connection_accept(socket_ctx_t* l_socket);

#endif //__NMINX_HTTP_CONNECTION_H
