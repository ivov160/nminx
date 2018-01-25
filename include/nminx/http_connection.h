#ifndef __NMINX_HTTP_CONNECTION_H
#define __NMINX_HTTP_CONNECTION_H

#include <nminx/nminx.h>
#include <nminx/config.h>

#include <nginx/ngx_config.h>
#include <nginx/ngx_core.h>

#include <nginx/ngx_buf.h>
#include <nginx/ngx_http_request.h>


/**
 * @defgroup connection Connection 
 * @brief Работа с сетевым подключением
 *
 * @addtogroup connection
 * @{
 */

#define NGX_LOWLEVEL_BUFFERED  0x0f
#define NGX_SSL_BUFFERED       0x01
#define NGX_HTTP_V2_BUFFERED   0x02

typedef struct http_large_buffer_s http_large_buffer_t;
/**
 * @brief Структура расширенного буфера
 */
struct http_large_buffer_s
{
    ngx_chain_t                      *busy;
    ngx_int_t                         nbusy;

    ngx_chain_t                      *free;
};

/**
 * @brief Контекст подключения
 */
struct http_connection_ctx_s
{
	// structured fields
	config_t* conf;
	server_ctx_t* serv;
	socket_ctx_t* socket;

	ngx_pool_t* pool;
	ngx_buf_t* buf;

	// application layer
	ngx_http_request_t* request;
	http_large_buffer_t* lb;

    off_t               sent;

	// flow control flags
    unsigned            close:1;
    unsigned            error:1;
    unsigned            destroyed:1;
    unsigned            buffered:8;

    unsigned            need_last_buf:1;
};


http_connection_ctx_t* http_connection_create(config_t* conf);
void http_connection_destroy(http_connection_ctx_t* conn);
void http_connection_close(http_connection_ctx_t* conn);

//listener initialize function
int http_listner_init_handler(config_t* conf, socket_ctx_t* socket);

// socket API handlers
void http_connection_accept(socket_ctx_t* l_socket);
void http_connection_request_handler(socket_ctx_t* socket);
void http_connection_errror_handler(socket_ctx_t* socket);

/**
 * @}
 */

#endif //__NMINX_HTTP_CONNECTION_H
