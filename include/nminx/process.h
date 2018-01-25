#ifndef _NMINX_PROCESS_H
#define _NMINX_PROCESS_H

#include <nminx/nminx.h>
#include <sys/types.h>

typedef int (*process_ptr)(void*);

struct process_ctx_s
{
	process_ptr process;
	void* data;
};

pid_t process_spawn(process_ctx_t* pc);
int process_signal(pid_t pid, int sig);


#endif //_NMINX_PROCESS_H
