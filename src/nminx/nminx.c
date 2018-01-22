#include <nminx/nminx.h>
#include <nminx/server.h>
#include <nminx/process.h>
#include <nminx/watchdog.h>

/** def configs */
static mtcp_config_t mtcp_conf = {
	0, /* cpu */
	MAX_CONNECTIONS, /* max_events */
};

static connection_config_t conn_conf = {
	NGX_DEFAULT_POOL_SIZE, /* pool_size */
	1024, /* buffer_size */
};

static http_request_config_t http_req_conf = {
	NGX_DEFAULT_POOL_SIZE, /* pool_size */
	8192, /* large_buffer_chunk_size */
	4, /* large_biffer_chunk_count */
	UNDERSCORES_IN_HEADERS | IGNORE_INVALID_HEADERS | URI_MERGE_SLASHES, /* headers_flags */
	NULL, /* headers_in_hash */
};

static watchdog_config_t wdt_conf = {
	1000, /* poll_timeout in ms*/
	3, /* workers */
};

static server_config_t serv_conf = {
	//inet_addr("0.0.0.0"), [> ip <]
	//htons(8080), [> port <]
	INADDR_ANY, /* ip */
	0, /* port */
	4096, /* backlog */
};

static int is_active = TRUE;

static void worker_signal_handler(int sig)
{
	is_active = FALSE;
}

static void watchdog_signal_handler(int sig)
{
	//printf("watchdog_signal_handler called, sig: %d\n", sig);
	is_active = FALSE;
	if(watchdog_signal_all(sig) != NMINX_OK)
	{
		printf("Failed pass signal: %i to childrens\n", sig);
	}
}

static int watchdog_state_handler(process_state_t* state, void* data)
{
	if(is_active)
	{
		return NMINX_AGAIN;
	}
	return NMINX_ABORT;
}

static int worker_process(void* data)
{
	printf("server_worker started\n");
	if(!data)
	{
		printf("Main config is NULL\n");
		return NMINX_ERROR;
	}
	server_ctx_t* s_ctx = (server_ctx_t*) data;

	while(is_active)
	{
		int result = server_process_events(s_ctx);
		if(result != NMINX_OK)
		{	// somthing wrong break loop, worker exit
			printf("Worker process events stoped!\n");
			break;
		}
	}
	return NMINX_OK;
}

static int run_worker_process(void* data)
{
	sigset_t sig_set;
	if(sigfillset(&sig_set) != 0)
	{
		printf("Failed get signal set, error: %s\n", strerror(errno));
		return NMINX_ERROR;
	}

	// condigure signals
	sigdelset(&sig_set, SIGTERM);
	sigprocmask(SIG_SETMASK, &sig_set, NULL);
	signal(SIGTERM, worker_signal_handler);
	signal(SIGINT, worker_signal_handler);

	return worker_process(data);
}

static int main_process(void* data)
{	
	printf("main_process started\n");

	if(!data)
	{
		printf("Main config is NULL\n");
		return NMINX_ERROR;
	}
	config_t* conf = (config_t*) data;
	watchdog_config_t* wdt_conf = get_wdt_conf(conf);

	signal(SIGTERM, watchdog_signal_handler);
	signal(SIGINT, watchdog_signal_handler);

	server_ctx_t* s_ctx = server_init(conf);
	if(!s_ctx)
	{
		printf("Failed create server!\n");
		return NMINX_ERROR;
	}

	watchdog_process_config_t* workers_pool = (watchdog_process_config_t*) 
		calloc(wdt_conf->workers, sizeof(watchdog_process_config_t));

	if(!workers_pool)
	{
		printf("Failed to initialize workers pool\n");
		server_destroy(s_ctx);
		return NMINX_ERROR;
	}

	process_config_t w_ctx = { 0 };
	w_ctx.process = run_worker_process;
	w_ctx.data = (void*) s_ctx;

	for(uint32_t i = 0; i < wdt_conf->workers; ++i)
	{
		watchdog_process_config_t* w = &workers_pool[i];
		w->process = &w_ctx;
		w->handler = watchdog_state_handler;
	}

	if(watchdog_start(workers_pool, wdt_conf->workers) != NMINX_OK)
	{
		printf("Failed initialize watchdog!\n");
		free(workers_pool);
		server_destroy(s_ctx);
		return NMINX_ERROR;
	}

	while(is_active)
	{
		watchdog_exec(wdt_conf->poll_timeout);
	}
	///@todo wait childrens before destroy all configs
	watchdog_stop();

	free(workers_pool);
	server_destroy(s_ctx);

	return NMINX_OK;
}

static int debug_process(void* data)
{	
	printf("debug_process started\n");

	if(!data)
	{
		printf("Main config is NULL\n");
		return NMINX_ERROR;
	}
	config_t* conf = (config_t*) data;

	signal(SIGTERM, worker_signal_handler);
	signal(SIGINT, worker_signal_handler);

	server_ctx_t* s_ctx = server_init(conf);
	if(!s_ctx)
	{
		printf("Failed create server!\n");
		return NMINX_ERROR;
	}

	while(is_active)
	{
		int result = server_process_events(s_ctx);
		if(result != NMINX_OK)
		{	// somthing wrong break loop, worker exit
			printf("Worker process events stoped!\n");
			break;
		}
	}

	server_destroy(s_ctx);

	return NMINX_OK;
}


int main(int argc, char* argv[]) 
{
	// initialize application config
	config_t m_cfg = { 0 };
	config_t* pconf = &m_cfg;

	set_io_conf(pconf, &mtcp_conf);
	set_wdt_conf(pconf, &wdt_conf);
	set_serv_conf(pconf, &serv_conf);
	set_conn_conf(pconf, &conn_conf);
	set_http_req_conf(pconf, &http_req_conf);

	// initialize wdt_process
	//process_config_t mp_cfg = { 0 };
	//mp_cfg.process = main_process;
	//mp_cfg.data = (void*) &m_cfg;

	//// initialize daemon 
	//daemon_config_t cfg = { 0 };
	//cfg.process = &mp_cfg;

	//if(process_daemon(&cfg) != NMINX_OK)
	//{
		//printf("Failed start daemon process\n");
		//return -1;
	//}
	
	//return 0;

	return debug_process((void*) pconf);
}
