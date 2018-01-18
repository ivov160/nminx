#include <stdio.h>
#include <stdlib.h>

#include <nminx/nminx.h>
#include <nminx/server.h>
#include <nminx/watchdog.h>

static int is_active = FALSE;

static int default_mtcp_cpu = 0;
static uint32_t defailt_wdt_timeout = 1000;
static uint32_t default_backlog_size = 4096;
static uint32_t default_worker_pool_size = 3;
static char* default_mtcp_config_path = "config/nminx.conf";

//static int default_mtcp_max_events = 30000; //MAX_FLOW_NUM * 3 see mtcp example server
static int default_mtcp_max_events = MAX_CONNECTIONS; //MAX_FLOW_NUM * 3 see mtcp example server

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
	server_ctx_t* s_cfg = (server_ctx_t*) data;
	nminx_config_t* m_cfg = s_cfg->m_cfg;

	struct mtcp_epoll_event events[MAX_CONNECTIONS] = { 0 };
	while(is_active)
	{
		int result = server_process_events(s_cfg, events, m_cfg->mtcp_max_events);
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
	nminx_config_t* m_cfg = (nminx_config_t*) data;

	int result = mtcp_init(m_cfg->mtcp_config_path);
	if(result) 
	{
		printf("Failed to initialize mtcp\n");
		return NMINX_ERROR;
	}

	// mTcp tuning
	//struct mtcp_conf mcfg = { 0 };
	////mtcp_getconf(&mcfg);
		////mcfg.num_cores = 1;
	////mtcp_setconf(&mcfg);

	// signals configure move to function
	mtcp_register_signal(SIGTERM, watchdog_signal_handler);
	mtcp_register_signal(SIGINT, watchdog_signal_handler);

	server_ctx_t* s_cfg = server_init(m_cfg);
	if(!s_cfg)
	{
		printf("Failed create server!\n");
		mtcp_destroy();
		return NMINX_ERROR;
	}

	listen_socket_config_t* ls_cfg = listen_socket_open(m_cfg->backlog, m_cfg->ip, m_cfg->port);
	if(!ls_cfg)
	{
		printf("Failed create listen socket!\n");
		server_destroy(s_cfg);
		mtcp_destroy();
		return NMINX_ERROR;
	}
	s_cfg->l_socket = ls_cfg;

	watchdog_process_config_t* workers_pool = 
		(watchdog_process_config_t*) calloc(m_cfg->worker_pool_size, sizeof(watchdog_process_config_t));

	if(!workers_pool)
	{
		printf("Failed to initialize workers pool\n");
		listen_socket_close(ls_cfg);
		server_destroy(s_cfg);
		mtcp_destroy();
		return NMINX_ERROR;
	}

	for(uint32_t i = 0; i < m_cfg->worker_pool_size; ++i)
	{
		watchdog_process_config_t* w = &workers_pool[i];
		w->process->process = run_worker_process;
		w->process->data = s_cfg;
		w->handler = watchdog_state_handler;
	}

	if(watchdog_start(workers_pool, m_cfg->worker_pool_size) != NMINX_OK)
	{
		printf("Failed initialize watchdog!\n");
		free(workers_pool);
		listen_socket_close(ls_cfg);
		server_destroy(s_cfg);
		mtcp_destroy();
		return NMINX_ERROR;
	}

	while(is_active)
	{
		watchdog_exec(&is_active, m_cfg->wdt_timeout_ms);
	}
	///@todo wait childrens before destroy all configs
	watchdog_stop();

	free(workers_pool);
	listen_socket_close(ls_cfg);
	server_destroy(s_cfg);
	mtcp_destroy();

	return NMINX_OK;
}

int main(int argc, char* argv[]) 
{
	// initialize application config
	nminx_config_t m_cfg = { 0 };

	m_cfg.mtcp_config_path = default_mtcp_config_path;
	m_cfg.backlog = default_backlog_size;
	m_cfg.mtcp_cpu = default_mtcp_cpu;
	m_cfg.mtcp_max_events = default_mtcp_max_events;

	m_cfg.wdt_timeout_ms = defailt_wdt_timeout;
	m_cfg.worker_pool_size = default_worker_pool_size;

	// initialize wdt_process
	process_config_t mp_cfg = { 0 };
	mp_cfg.process = main_process;
	mp_cfg.data = (void*) &m_cfg;

	// initialize daemon 
	daemon_config_t cfg = { 0 };
	cfg.process = &mp_cfg;

	if(process_daemon(&cfg) != NMINX_OK)
	{
		printf("Failed start daemon process\n");
		return -1;
	}

	return 0;
}