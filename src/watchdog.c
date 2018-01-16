#include <nminx/watchdog.h>

#include <sys/wait.h>

#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

#include <error.h>
#include <errno.h>
#include <string.h>


/** @todo	think about is_alive flag, may be good ide for checking watched processed,
 *			now watched process is where pid > 0
 */
typedef struct 
{
	pid_t pid;
	watchdog_process_config_t* wd_config;

} watchdog_process_info_t;

struct watchdog_config
{
	watchdog_process_info_t* pool;
	uint32_t size;
};

static struct watchdog_config w_config = { NULL, 0 };

static watchdog_process_info_t* watchdog_find_process(pid_t pid);


int watchdog_start(watchdog_process_config_t* pool, uint32_t size)
{
	w_config.pool = (watchdog_process_info_t*) calloc(size, sizeof(watchdog_process_info_t));
	if(!w_config.pool)
	{
		printf("Failed initialize array for watchdog poll!\n");
		return NMINX_ERROR;
	}
	w_config.size = size;

	for(uint32_t i = 0; i < size; ++i)
	{
		watchdog_process_config_t* w_p = &pool[i];
		watchdog_process_info_t* w_pi = &w_config.pool[i];

		if(!w_p->handler || !w_p->process)
		{	//skip bad process 
			continue;
		}

		pid_t c_pid = process_spawn(w_p->process);
		if(c_pid == -1)
		{
			printf("Failed start child porcess\n");
			return NMINX_ERROR;
		}
		w_pi->pid = c_pid;
		w_pi->wd_config = w_p;
	}
	return NMINX_OK;
}

/** @todo	think about wait state
 */
int watchdog_stop()
{
	watchdog_signal_all(SIGTERM);

	if(w_config.pool)
	{
		free(w_config.pool);
		w_config.size = 0;
	}
}

int watchdog_signal_all(int sig)
{
	for(uint32_t i = 0; i < w_config.size; ++i)
	{
		watchdog_process_info_t* pi = &w_config.pool[i];
		if(pi->pid == 0)
		{	//skip not watched process
			continue;
		}

		if(process_signal(pi->pid, sig) == NMINX_ERROR)
		{
			printf("Failed notify pid: %d\n", pi->pid);
			return NMINX_ERROR;
		}
	}
	return NMINX_OK;
}

int watchdog_signal_one(int sig, pid_t pid)
{
	watchdog_process_info_t* pi = watchdog_find_process(pid);
	if(pi)
	{
		if(process_signal(pi->pid, sig) == NMINX_ERROR)
		{
			printf("Failed notify pid: %d\n", pi->pid);
			return NMINX_ERROR;
		}
	}
	return NMINX_OK;
}

int watchdog_exec(int* is_alive, uint32_t ms_timeout)
{
	process_state_t p_state;
	while(*is_alive)
	{
		int result = watchdog_poll(&p_state);
		if(result == NMINX_ERROR)
		{
			return NMINX_ERROR;
		}

		if(result == NMINX_AGAIN && w_config.size > 0)
		{
			struct timespec ts;
			ts.tv_sec = ms_timeout / 1000;
			ts.tv_nsec = (ms_timeout % 1000) * 1000000;
			nanosleep(&ts, NULL);
		}
		else
		{
			watchdog_process_info_t* pi = watchdog_find_process(p_state.pid);
			if(!pi)
			{
				printf("watchdog_exec not found child process pid: %d\n", p_state.pid);
				return NMINX_ERROR;
			}

			watchdog_process_config_t* wdc = pi->wd_config;
			if(wdc->handler)
			{
				int cmd = wdc->handler(&p_state, wdc->data);
				if(cmd == NMINX_AGAIN)
				{	// restart watched process
					pid_t c_pid = process_spawn(wdc->process);
					if(c_pid == -1)
					{
						printf("Failed start child porcess\n");
						return NMINX_ERROR;
					}
					//printf("wdt pid change old: %d, new: %d\n", pi->pid, c_pid);
					pi->pid = c_pid;
				}
				else
				{	// stop watching process
					pi->pid = 0;
				}
			}
		}
	}
	return NMINX_OK;
}

int watchdog_poll(process_state_t* pstate)
{
	int status = 0;
	pid_t c_pid = waitpid(-1, &status, WNOHANG);

	if(c_pid == 0)
	{
		return NMINX_AGAIN;
	}
	else if(c_pid == -1)
	{
		switch (errno)
		{
			case ECHILD:
				return NMINX_AGAIN;

			case EINTR:
				return NMINX_ABORT;

			default:
				return NMINX_ERROR;
		};
	}

	memset(pstate, 0, sizeof(process_state_t));

	pstate->pid = c_pid;
	if(WIFEXITED(status))
	{
		pstate->is_exit = TRUE;
		pstate->status = WEXITSTATUS(status);
		printf("child pid: %d exit with status: %d\n", c_pid, WEXITSTATUS(status));
	}
	else if(WIFSIGNALED(status))
	{
		pstate->is_signal = TRUE;
		pstate->status = WTERMSIG(status);
		printf("child pid: %d accept signal: %d\n", c_pid, WTERMSIG(status));
	}
	else if(WIFSTOPPED(status))
	{
		pstate->is_stoped = TRUE;
		pstate->status = WSTOPSIG(status);
		printf("child pid: %d stoped by signal: %d\n", c_pid, WSTOPSIG(status));
	}
	return NMINX_OK;
}

static watchdog_process_info_t* watchdog_find_process(pid_t pid)
{
	for(uint32_t i = 0; i < w_config.size; ++i)
	{
		watchdog_process_info_t* pi = &w_config.pool[i];
		if(pi->pid == pid)
		{
			return pi;
		}
	}
	return NULL;
}
