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

static int process_daemon_start(void* data);

pid_t process_spawn(process_config_t* pc)
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

///@todo checking for result
int process_signal(pid_t pid, int sig)
{
	return kill(pid, sig);
}

int process_daemon(daemon_config_t* dc)
{	
	if(!dc)
	{
		printf("Failed run as daemon, config is null\n");
		return NMINX_ERROR;
	}

	if(!dc->process)
	{
		printf("Failed start daemon process, process is empty \n");
		return NMINX_ERROR;
	}

	umask(0);

	process_config_t pc;
	pc.process = process_daemon_start;
	pc.data = dc;

	pid_t d_pid = process_spawn(&pc);
	if(d_pid == -1)
	{
		printf("Failed start daemon porcess\n");
		return NMINX_ERROR;
	}
	return NMINX_OK;
}

static int process_daemon_start(void* data)
{
	daemon_config_t* dc = (daemon_config_t*) data;

	if(setsid() == -1)
	{
		printf("Failed mark process as lead\n");
		return NMINX_ERROR;
	}

	//int fd = open("/dev/null", O_RDWR);
	//if(dup2(fd, STDIN_FILENO) == -1)
	//{
		//printf("Failed redirect stdin\n");
		//exit(NMINX_ERROR);
	//}

	//if(dup2(fd, STDOUT_FILENO) == -1)
	//{
		//printf("Failed redirect stdout\n");
		//exit(NMINX_ERROR);
	//}

	//if(dup2(fd, STDERR_FILENO) == -1)
	//{
		//printf("Failed redirect sderr\n");
		//exit(NMINX_ERROR);
	//}

	//if(close(fd) == -1)
	//{
		//printf("Failed close redirection target\n");
		//exit(NMINX_ERROR);
	//}

	if(dc->work_dir)
	{
		int result = chdir(dc->work_dir);
		if(result != 0)
		{
			printf("Failed change work directory, error: %s\n", strerror(errno));
			return NMINX_ERROR;
		}
	}

	if(dc->uid)
	{
		if(setuid(dc->uid) != 0)
		{
			printf("Failed change user id, error: %s\n", strerror(errno));
			return NMINX_ERROR;
		}
	}

	if(dc->gid)
	{
		if(setgid(dc->gid) != 0)
		{
			printf("Failed change group id, error: %s\n", strerror(errno));
			return NMINX_ERROR;
		}
	}

	if(dc->pid_file)
	{
		//int fd = open(dc->pid_file, O_CREAT|O_RDWR|O_TRUNC);
		int fd = open(dc->pid_file, O_RDWR);
		if(fd == -1)
		{
			printf("Failed open pid file, error: %s\n", strerror(errno));
			return NMINX_ERROR;
		}

		pid_t pid = getpid();
		if(dprintf(fd, "%ul", pid) < 0)
		{
			printf("Failed write pid file, error: %s\n", strerror(errno));
			return NMINX_ERROR;
		}
	}

	process_config_t* pc = dc->process;
	exit(pc->process(pc->data));
}

