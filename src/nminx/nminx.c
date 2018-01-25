#include <nminx/nminx.h>
#include <nminx/config.h>

#include <nminx/server.h>
#include <nminx/process.h>
#include <nminx/watchdog.h>
#include <nminx/http_connection.h>

#include <nginx/ngx_core.h>
#include <nginx/ngx_palloc.h>


#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/** def configs */
static mtcp_config_t mtcp_conf = {
	0, /* cpu */
	MAX_CONNECTIONS, /* max_events */
	"config/nminx.conf", /* config_path */
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
	"run",
	"nminx.pid",
	FALSE
};

static server_config_t serv_conf = {
	INADDR_ANY, /* ip */
	0, /* port */
	4096, /* backlog */
	http_listner_init_handler, /* listener_init_handler */
};

static server_ctx_t* sptr = NULL;
static int wdt_is_active = FALSE;

static int show_help = FALSE;
static int listen_port = SERVER_DEFAULT_PORT;
static int quiet_mode = FALSE;
static char* listen_ip = "0.0.0.0";
static char* config_path = "config/nminx.conf";
static char* work_dir = "run";

static void worker_signal_handler(int sig)
{
	printf("worker_signal_handler called, sig: %d\n", sig);
	if(sptr != NULL)
	{
		sptr->is_run = FALSE;
	}
}

static void watchdog_signal_handler(int sig)
{
	printf("watchdog_signal_handler called, sig: %d\n", sig);
	wdt_is_active = FALSE;

	//SIGTERM not stop mtcp poll
	int child_sig = sig == SIGTERM ? SIGINT : sig;
	if(watchdog_signal_all(child_sig) != NMINX_OK)
	{
		printf("Failed pass signal: %i to childrens\n", sig);
	}
}

static int watchdog_state_handler(process_state_t* state, void* data)
{
	if(wdt_is_active)
	{
		return wdt_is_active ? NMINX_AGAIN : NMINX_ABORT;
	}
	return NMINX_ABORT;
}

static int server_process(void* data)
{	
	printf("server_process started\n");

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

	sptr = s_ctx;
	s_ctx->is_run = TRUE;

	while(s_ctx->is_run)
	{
		int result = server_process_events(s_ctx);
		if(result != NMINX_OK)
		{	// somthing wrong break loop, worker exit
			printf("Worker process events stoped!\n");
			break;
		}
		// reset tmp_pool after interation
		ngx_reset_pool(conf->temp_pool);
	}

	server_destroy(s_ctx);
	ngx_destroy_pool(conf->temp_pool);
	ngx_destroy_pool(conf->pool);

	return NMINX_OK;
}

static int wdt_process(void* data)
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

	process_ctx_t wp = {
		server_process, data
	};

	watchdog_process_ctx_t wdtp = {
		0, &wp, watchdog_state_handler, NULL
	};

	watchdog_pool_ctx_t wdt_pool = {
		&wdtp, 1
	};

	if(watchdog_start(&wdt_pool) != NMINX_OK)
	{
		printf("Failed initialize watchdog!\n");
		return NMINX_ERROR;
	}

	wdt_is_active = TRUE;
	while(wdt_is_active)
	{
		watchdog_exec(wdt_conf->poll_timeout);
	}

	watchdog_stop();

	return NMINX_OK;
}

static int run_wdt_process(void* data)
{
	if(!data)
	{
		printf("Main config is NULL\n");
		return NMINX_ERROR;
	}
	config_t* conf = (config_t*) data;
	watchdog_config_t* wdt_conf = get_wdt_conf(conf);

	if(setsid() == -1)
	{
		printf("Failed mark process as lead\n");
		return NMINX_ERROR;
	}

	if(wdt_conf->silent)
	{
		int fd = open("/dev/null", O_RDWR);
		if(dup2(fd, STDIN_FILENO) == -1)
		{
			printf("Failed redirect stdin\n");
			return NMINX_ERROR;
		}

		if(dup2(fd, STDOUT_FILENO) == -1)
		{
			printf("Failed redirect stdout\n");
			return NMINX_ERROR;
		}

		if(dup2(fd, STDERR_FILENO) == -1)
		{
			printf("Failed redirect sderr\n");
			return NMINX_ERROR;
		}

		if(close(fd) == -1)
		{
			printf("Failed close redirection target\n");
			return NMINX_ERROR;
		}
	}

	if(wdt_conf->pid_file)
	{
		int fd = open(wdt_conf->pid_file,  O_RDWR | O_CREAT | O_TRUNC);
		if(fd == -1)
		{
			printf("Failed open pid file path: %s, error: %s\n", wdt_conf->pid_file, strerror(errno));
			return NMINX_ERROR;
		}

		pid_t pid = getpid();
		if(dprintf(fd, "%u", pid) < 0)
		{
			printf("Failed write pid file, error: %s\n", strerror(errno));
			return NMINX_ERROR;
		}
	}

	if(wdt_conf->work_dir)
	{
		int result = chdir(wdt_conf->work_dir);
		if(result != 0)
		{
			printf("Failed change work directory, error: %s\n", strerror(errno));
			return NMINX_ERROR;
		}
	}

	return wdt_process(data);
}

static ngx_int_t ngx_get_options(int argc, char *const *argv)
{
    u_char     *p;
    ngx_int_t   i;

    for (i = 1; i < argc; i++) {

        p = (u_char *) argv[i];

        if (*p++ != '-') {
            printf("invalid option: \"%s\" \n", argv[i]);
            return NMINX_ERROR;
        }

        while (*p) {

            switch (*p++) {

            case '?':
            case 'h':
                show_help = TRUE;
                break;
            case 'i':
                listen_ip = argv[++i];
				goto next;
                break;

            case 'p':
                listen_port = atoi(argv[++i]);
				goto next;
                break;

            case 'q':
                quiet_mode = TRUE;
                break;

            case 'c':
				config_path = argv[++i];
				goto next;
				break;

            case 'w':
				work_dir = argv[++i];
				goto next;
				break;

            default:
                printf("invalid option: \"%c\"", *(p - 1));
                return NGX_ERROR;
            }
        }

    next:
        continue;
    }

    return NMINX_OK;
}

static void print_help(char* name)
{
	printf(
		"Usage: %s [-?hq] -i ip -p port -c conf -w work_dir \n"
		"Options: \n"
		"	-?, -h				: this help\n"
		"	-i ip				: listen ip, default: %s\n"
		"	-p port				: listen port, default: %d\n"
		"	-c conf				: path to config (mtcp conf), default: %s\n"
		"	-w work_dir			: daemon working dir, default: %s\n",
		name,
		listen_ip,
		listen_port,
		config_path,
		work_dir
	);
}

int main(int argc, char* argv[]) 
{
	ngx_cpuinfo();

	if(ngx_get_options(argc, argv) == NMINX_ERROR)
	{
		printf("Failed parse args\n");
		return -1;
	}

	if(show_help)
	{
		print_help(argv[0]);
		return 0;
	}

	// initialize application config
	config_t m_cfg = { 0 };
	config_t* pconf = &m_cfg;

	ngx_pool_t* pool = ngx_create_pool(NGX_DEFAULT_POOL_SIZE);
	if(pool == NULL)
	{
		printf("Failed allocate memmory pool for config!\n");
		return -1;
	}
	pconf->pool = pool;

	ngx_pool_t* temp_pool = ngx_create_pool(NGX_DEFAULT_POOL_SIZE);
	if(pool == NULL)
	{
		printf("Failed allocate memmory pool for config!\n");
		return -1;
	}
	pconf->temp_pool = temp_pool;

	set_io_conf(pconf, &mtcp_conf);
	set_wdt_conf(pconf, &wdt_conf);
	set_serv_conf(pconf, &serv_conf);
	set_conn_conf(pconf, &conn_conf);
	set_http_req_conf(pconf, &http_req_conf);

	mtcp_conf.config_path = config_path;

	wdt_conf.silent = quiet_mode;
	wdt_conf.work_dir = work_dir;

	wdt_conf.pid_file = ngx_palloc(pool, sizeof("/nminx.pid") + strlen(work_dir));
	if(wdt_conf.pid_file == NULL)
	{
		printf("Failed set pid_file path\n");
		return -1;
	}
	sprintf(wdt_conf.pid_file, "%s/nminx.pid", work_dir);

	serv_conf.ip = inet_addr(listen_ip);
	serv_conf.port = htons(listen_port);

	// initialize wdt_process
	process_ctx_t p_ctx = { 0 };
	p_ctx.data = pconf;
	p_ctx.process = run_wdt_process;

	if(process_spawn(&p_ctx) == NMINX_ERROR)
	{
		printf("Failed start daemon process\n");
		return -1;
	}
	return 0;
	//return server_process((void*) pconf);
}
