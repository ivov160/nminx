#ifndef __NMINX_HTTP_REQUEST_H
#define __NMINX_HTTP_REQUEST_H

#include <nminx/nminx.h>
#include <nminx/config.h>

/**
 * @defgroup http_request HTTP Request 
 * @brief Работа с http запросом
 * 
 * Публичное API для обработки подключения как HTTP запрос
 * Внутри куски nginx.
 * 
 * ngx_http_request_t - структура для обработки запроса в nginx
 * 
 * @addtogroup http_request
 * @{
 */

ngx_http_request_t* http_request_create(http_connection_ctx_t* ctx);
void http_request_destroy(ngx_http_request_t* r);
void http_request_close(ngx_http_request_t* r);

// initialize functions
int http_request_init_config(config_t* conf);
int http_request_init_events(http_connection_ctx_t* ctx);

/**
 * @}
 */

#endif //__NMINX_HTTP_REQUEST_H
