#ifndef _NMINX_H
#define _NMINX_H

#include <nginx/ngx_config.h>
#include <nginx/ngx_core.h>

typedef struct io_ctx_s io_ctx_t;
typedef struct socket_ctx_s socket_ctx_t;
typedef struct server_ctx_s server_ctx_t;
typedef struct http_connection_ctx_s http_connection_ctx_t;

typedef struct process_ctx_s process_ctx_t;
typedef struct process_state_s process_state_t;

typedef struct watchdog_process_ctx_s watchdog_process_ctx_t;
typedef struct watchdog_pool_ctx_s watchdog_pool_ctx_t;

#define  NMINX_OK         NGX_OK			// 
#define  NMINX_ERROR      NGX_ERROR
#define  NMINX_AGAIN      NGX_AGAIN
#define  NMINX_BUSY       NGX_BUSY
#define  NMINX_DONE       NGX_DONE
#define  NMINX_DECLINED   NGX_DECLINED
#define  NMINX_ABORT      NGX_ABORT

#define TRUE 1
#define FALSE 0

#define MAX_CONNECTIONS 10000

#define UNDERSCORES_IN_HEADERS (1 << 1)
#define IGNORE_INVALID_HEADERS (1 << 2)
#define URI_MERGE_SLASHES (1 << 3)

#endif	//_NMINX_H

