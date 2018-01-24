#ifndef __H_NMINX_CONFIG_H
#define __H_NMINX_CONFIG_H

#include <nminx/nminx.h>

typedef struct config_s config_t;
typedef struct mtcp_config_s mtcp_config_t;
typedef struct watchdog_config_s watchdog_config_t;
typedef struct server_config_s server_config_t;
typedef struct connection_config_s connection_config_t;
typedef struct http_request_config_s http_request_config_t;

#define CONFIG_SLOTS_COUNT 8
#define CONFIG_SLOT_1 0
#define CONFIG_SLOT_2 0
#define CONFIG_SLOT_3 0
#define CONFIG_SLOT_4 0
#define CONFIG_SLOT_5 0
#define CONFIG_SLOT_6 0
#define CONFIG_SLOT_7 0
#define CONFIG_SLOT_8 0

#define get_io_conf(mc) (mtcp_config_t*) mc->slots[CONFIG_SLOT_1]
#define get_wdt_conf(mc) (watchdog_config_t*) mc->slots[CONFIG_SLOT_2]
#define get_serv_conf(mc) (server_config_t*) mc->slots[CONFIG_SLOT_3]
#define get_conn_conf(mc) (connection_config_t*) mc->slots[CONFIG_SLOT_4]
#define get_http_req_conf(mc) (http_request_config_t*) mc->slots[CONFIG_SLOT_5]

#define set_io_conf(mc, conf) mc->slots[CONFIG_SLOT_1] = (uintptr_t) conf
#define set_wdt_conf(mc, conf) mc->slots[CONFIG_SLOT_2] = (uintptr_t) conf
#define set_serv_conf(mc, conf) mc->slots[CONFIG_SLOT_3] = (uintptr_t) conf
#define set_conn_conf(mc, conf) mc->slots[CONFIG_SLOT_4] = (uintptr_t) conf
#define set_http_req_conf(mc, conf) mc->slots[CONFIG_SLOT_5] = (uintptr_t) conf

struct config_s
{
	uintptr_t slots[CONFIG_SLOTS_COUNT];

	// pool for application
	ngx_pool_t* pool;

	// temporary pool for iteration
	// after iteration pool reseted pointers is not valid
	ngx_pool_t* temp_pool;
};

struct mtcp_config_s
{
	int cpu;
	int max_events;
	char* config_path;
};

struct connection_config_s
{
	uint32_t pool_size;
	uint32_t buffer_size;
};

struct http_request_config_s
{
	uint32_t pool_size;
	uint32_t large_buffer_chunk_size;
	uint32_t large_buffer_chunk_count;
	
	uint32_t headers_flags;
	//uint32_t postpone_output;
	//uint32_t sendfile_max_chunk;

	ngx_hash_t headers_in_hash;
};

struct watchdog_config_s
{
	uint32_t poll_timeout;
	uint32_t workers;
};

typedef int (*listner_init_hanler_t)(config_t*, socket_ctx_t*);
struct server_config_s
{
	in_addr_t ip;
	in_port_t port;
	uint32_t backlog;

	listner_init_hanler_t listener_init_handler;
};

#endif //__H_NMINX_CONFIG_H
