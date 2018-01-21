#ifndef _NMINX_H
#define _NMINX_H

#include <nminx/config.h>

typedef struct ngx_pool_s ngx_pool_t;
typedef struct ngx_buf_s  ngx_buf_t;
typedef struct ngx_chain_s ngx_chain_t;

#define  NMINX_OK          0
#define  NMINX_ERROR      -1
#define  NMINX_AGAIN      -2
#define  NMINX_BUSY       -3
#define  NMINX_DONE       -4
#define  NMINX_DECLINED   -5
#define  NMINX_ABORT      -6

#define LF     (u_char) '\n'
#define CR     (u_char) '\r'
#define CRLF   "\r\n"

#define TRUE 1
#define FALSE 0

#define MAX_CONNECTIONS 10000


typedef struct
{
	uint32_t wdt_timeout_ms;
	uint32_t worker_pool_size;

	in_addr_t ip;
	in_port_t port;
	uint32_t backlog;

	int mtcp_cpu;
	int mtcp_max_events;
	char* mtcp_config_path;

} nminx_config_t;



#endif	//_NMINX_H

