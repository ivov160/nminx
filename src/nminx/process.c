#include <nminx/process.h>

#include <error.h>
#include <errno.h>
#include <string.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/file.h>
#include <sys/signal.h>
#include <sys/resource.h>

pid_t process_spawn(process_ctx_t* pc)
{
	pid_t pid = fork();
	if(pid == -1)
	{
		printf("Failed create child process, error: %s\n", strerror(errno));
	}
	else if(pid == 0)
	{
		exit(pc->process(pc->data));
	}
	return pid;
}

int process_signal(pid_t pid, int sig)
{
	return kill(pid, sig);
}

