#ifndef _NMINX_WATCHDOG_H
#define _NMINX_WATCHDOG_H

#include <nminx/nminx.h>
#include <nminx/process.h>

typedef struct 
{
	int is_exit;
	int is_signal;
	int is_stoped;

	int status;

	pid_t pid;

} process_state_t;

typedef int (*watchdog_handler)(process_state_t*, void* data);

typedef struct 
{
	process_config_t* process;

	watchdog_handler handler;
	void* data;

} watchdog_process_config_t;

int watchdog_start(watchdog_process_config_t* pool, uint32_t size);
int watchdog_stop();

int watchdog_signal_all(int sig);
int watchdog_signal_one(int sig, pid_t pid);

int watchdog_exec(uint32_t ms_timeout);
int watchdog_poll(process_state_t* pstate);


#endif //_NMINX_WATCHDOG_H
