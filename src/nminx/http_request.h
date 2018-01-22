#ifndef __NMINX_HTTP_REQUEST_H
#define __NMINX_HTTP_REQUEST_H

#include <nminx/nminx.h>
#include <nminx/http_connection.h>

#include <nginx/ngx_config.h>
#include <nginx/ngx_core.h>

#include <nginx/ngx_buf.h>
#include <nginx/ngx_http_request.h>

typedef struct http_large_buffer_s http_large_buffer_t;
struct http_large_buffer_s
{
    ngx_chain_t                      *busy;
    ngx_int_t                         nbusy;

    ngx_chain_t                      *free;
};

struct http_connection_ctx_s
{
	// structured fields
	nminx_config_t* m_cfg;
	socket_ctx_t* socket;

	ngx_pool_t* pool;
	ngx_buf_t* buf;

	// application layer
	ngx_http_request_t* request;
	http_large_buffer_t* lb;

	// flow control flags
	int need_close;
	int error;
};

ngx_http_request_t* http_request_create(http_connection_ctx_t* ctx);
int http_request_init_events(http_connection_ctx_t* ctx);

//int http_request_read_handler(socket_ctx_t* socket);
//int http_request_write_handler(socket_ctx_t* socket);
//int http_request_close_handler(socket_ctx_t* socket);
//int http_connection_cleanup_handler(void* data);

#endif //__NMINX_HTTP_REQUEST_H
