#include <nminx/watchdog.h>
#include <nminx/process.h>

#include <sys/wait.h>

#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

#include <error.h>
#include <errno.h>
#include <string.h>


static watchdog_pool_ctx_t* wdt_pool = NULL;

static watchdog_process_ctx_t* watchdog_find_process(pid_t pid);

int watchdog_start(watchdog_pool_ctx_t* wdtp)
{
	wdt_pool = wdtp;
	if(!wdt_pool)
	{
		printf("Failed initialize array for watchdog poll!\n");
		return NMINX_ERROR;
	}

	for(uint32_t i = 0; i < wdt_pool->size; ++i)
	{
		watchdog_process_ctx_t* wp = &wdt_pool->pool[i];

		if(!wp->handler || !wp->process)
		{	//skip bad process 
			continue;
		}

		pid_t c_pid = process_spawn(wp->process);
		if(c_pid == -1)
		{
			printf("Failed start child porcess\n");
			return NMINX_ERROR;
		}
		wp->pid = c_pid;
	}
	return NMINX_OK;
}

/** 
 * @todo	think about wait state
 */
int watchdog_stop()
{
	int is_done = FALSE;
	while(!is_done)
	{
		watchdog_signal_all(SIGTERM);

		// NMINX_AGAIN - not lef child
		process_state_t p_state;
		int rc = watchdog_poll(&p_state);

		if(rc == NMINX_AGAIN)
		{
			is_done = TRUE;
		}
	}
	wdt_pool = NULL;
}

int watchdog_signal_all(int sig)
{
	for(uint32_t i = 0; i < wdt_pool->size; ++i)
	{
		watchdog_process_ctx_t* p = &wdt_pool->pool[i];
		if(p->pid == 0)
		{	//skip not watched process
			continue;
		}

		if(process_signal(p->pid, sig) == NMINX_ERROR)
		{
			printf("Failed notify pid: %d, error: %s\n", p->pid, strerror(errno));
			return NMINX_ERROR;
		}
	}
	return NMINX_OK;
}

int watchdog_signal_one(int sig, pid_t pid)
{
	watchdog_process_ctx_t* p = watchdog_find_process(pid);
	if(p)
	{
		if(process_signal(p->pid, sig) == NMINX_ERROR)
		{
			printf("Failed notify pid: %d\n", p->pid);
			return NMINX_ERROR;
		}
	}
	return NMINX_OK;
}

int watchdog_exec(uint32_t ms_timeout)
{
	process_state_t p_state = { 0 };
	int result = watchdog_poll(&p_state);

	if(result == NMINX_ERROR)
	{
		return NMINX_ERROR;
	}

	if(result == NMINX_AGAIN && wdt_pool->size > 0)
	{
		struct timespec ts;
		ts.tv_sec = ms_timeout / 1000;
		ts.tv_nsec = (ms_timeout % 1000) * 1000000;
		nanosleep(&ts, NULL);
	}
	else
	{
		watchdog_process_ctx_t *p = watchdog_find_process(p_state.pid);
		printf("watchdog find ctx: %p\n", (void*) p);
		if(!p)
		{
			printf("watchdog_exec not found child process pid: %d\n", p_state.pid);
			return NMINX_ERROR;
		}

		int cmd = p->handler(&p_state, p->data);
		if(cmd == NMINX_AGAIN)
		{	// restart watched process
			pid_t c_pid = process_spawn(p->process);
			if(c_pid == -1)
			{
				printf("Failed start child porcess\n");
				return NMINX_ERROR;
			}
			printf("wdt pid change p: %p, old: %d, new: %d\n", (void*)p, p->pid, c_pid);
			p->pid = c_pid;
		}
		else
		{	// stop watching process
			p->pid = 0;
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

static watchdog_process_ctx_t* watchdog_find_process(pid_t pid)
{
	for(uint32_t i = 0; i < wdt_pool->size; ++i)
	{
		watchdog_process_ctx_t* p = &wdt_pool->pool[i];
		if(p->pid == pid)
		{
			printf("watchdog find return ctx: %p\n", (void*) p);
			return p;
		}
	}
	return NULL;
}
