#ifndef _NMINX_IO_H
#define _NMINX_IO_H

#include <nminx/nminx.h>

typedef struct
{
	int ep;
	mctx_t mctx;
} io_ctx_t;

//typedef struct
//{

//} io_actions_t;

io_ctx_t* io_init(nminx_config_t* cfg);
int io_destroy(io_ctx_t* io);

//int io_process_events(io_ctx_t* io);


#endif //_NMINX_IO_H
