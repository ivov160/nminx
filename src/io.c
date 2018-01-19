#include <nminx/io.h>
// not good hack, but now all mtcp data hidden in modules
// io implementation (mtcp) can be replaced without big refactoring
#include "io_ctx.h"

#include <stdio.h>
#include <stdlib.h>

#include <error.h>
#include <errno.h>
#include <string.h>

#include <mtcp_api.h>
#include <mtcp_epoll.h>

static io_ctx_t mtcp_io_ctx = { 0 };

io_ctx_t* io_init(nminx_config_t* m_cfg)
{
	if(mtcp_io_ctx.mctx)
	{
		return &mtcp_io_ctx;
	}

	int result = mtcp_init(m_cfg->mtcp_config_path);
	if(result < 0) 
	{
		printf("Failed to initialize mtcp\n");
		return NULL;
	}

	// mTcp tuning
	//struct mtcp_conf mcfg = { 0 };
	////mtcp_getconf(&mcfg);
		////mcfg.num_cores = 1;
	////mtcp_setconf(&mcfg);

	//mtcp_core_affinitize(core);
	
	// create mtcp context: this will spawn an mtcp thread 
	mtcp_io_ctx.mctx = mtcp_create_context(m_cfg->mtcp_cpu);
	if (!mtcp_io_ctx.mctx) 
	{
		printf("Failed to create mtcp context!\n");
		mtcp_destroy();
		return NULL;
	}

	// create epoll descriptor 
	mtcp_io_ctx.ep = mtcp_epoll_create(mtcp_io_ctx.mctx, m_cfg->mtcp_max_events);
	if (mtcp_io_ctx.ep < 0) 
	{
		printf("Failed to create epoll descriptor!\n");
		mtcp_destroy_context(mtcp_io_ctx.mctx);
		mtcp_destroy();
		return NULL;
	}

	return &mtcp_io_ctx;
}

int io_destroy(io_ctx_t* io)
{
	if(io == &mtcp_io_ctx && mtcp_io_ctx.mctx)
	{
		mtcp_destroy_context(mtcp_io_ctx.mctx);
		mtcp_destroy();

		mtcp_io_ctx.mctx = NULL;
		mtcp_io_ctx.ep = 0;
	}
}

int io_poll_ctl(io_ctx_t* io, int op, int flags, socket_ctx_t* sock)
{
	if(!io || !sock)
	{
		struct mtcp_epoll_event ev = { 0 };
		ev.data.ptr = (void*) sock;
		ev.events = flags;

		int result = mtcp_epoll_ctl(io->mctx, io->ep, op, sock->fd, &ev);
		return result < 0 ? NMINX_ERROR : NMINX_OK;

	}
	return NMINX_ERROR;
}

int io_poll_events(io_ctx_t* io, socket_ctx_t** s_buff, int s_buff_size)
{
	struct mtcp_epoll_event* events = (struct mtcp_epoll_event*) 
		malloc(sizeof(struct mtcp_epoll_event) * s_buff_size);

	int nevents = mtcp_epoll_wait(io->mctx, io->ep, events, s_buff_size, -1);
	if (nevents < 0) 
	{
		free(events);
		return NMINX_ERROR;
	}

	int s_buff_offset = 0;
	for (int i = 0; i < nevents; i++) 
	{
		if (events[i].data.ptr)
		{
			socket_ctx_t* socket = (socket_ctx_t*) events[i].data.ptr;
			socket->flags = events[i].events;

			s_buff[s_buff_offset] = socket;
			++s_buff_offset;
		}
	}
	
	free(events);
	// convert offset to count
	return s_buff_offset + 1;
}
