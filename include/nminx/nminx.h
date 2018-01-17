#ifndef _NMINX_H
#define _NMINX_H

#include <stdint.h>
#include <arpa/inet.h>

#define  NMINX_OK          0
#define  NMINX_ERROR      -1
#define  NMINX_AGAIN      -2
#define  NMINX_BUSY       -3
#define  NMINX_DONE       -4
#define  NMINX_DECLINED   -5
#define  NMINX_ABORT      -6

#define TRUE 1
#define FALSE 0


typedef struct
{
	char* mtcp_config_path;
	uint32_t wdt_timeout_ms;

	uint32_t worker_pool_size;

	in_addr_t ip;
	in_port_t port;
	uint32_t backlog;

	int mtcp_cpu;
	int mtcp_max_events;

} nminx_config_t;

#endif	//_NMINX_H

