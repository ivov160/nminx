#ifndef _NMINX_PROCESS_H
#define _NMINX_PROCESS_H

#include <nminx/nminx.h>
#include <sys/types.h>

typedef int (*process_ptr)(void*);

typedef struct 
{
	process_ptr process;
	void* data;

} process_config_t;

typedef struct 
{
	char* work_dir;
	char* pid_file;

	uid_t uid;
	gid_t gid;

	process_config_t* process;

} daemon_config_t;

int process_daemon(daemon_config_t* dc);
pid_t process_spawn(process_config_t* pc);
int process_signal(pid_t pid, int sig);


#endif //_NMINX_PROCESS_H
