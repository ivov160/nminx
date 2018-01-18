#ifndef _NMINX_IO_H
#define _NMINX_IO_H

#include <nminx/nminx.h>
#include <nminx/socket.h>

enum io_operation
{
	IO_CTL_ADD = 1, 
	IO_CTL_DEL = 2, 
	IO_CTL_MOD = 3, 
};

enum io_event_flag
{
	IO_EVENT_NONE,
	IO_EVENT_READ,
	IO_EVENT_WRITE,
	IO_EVENT_ERROR,
};

io_ctx_t* io_init(nminx_config_t* cfg);
int io_destroy(io_ctx_t* io);

int io_ctl(io, io_operation op, io_event_type ev, socket_ctx_t* sock);
int io_poll_events(io, socket_ctx_t* s_buff, int s_buff_size);

#endif //_NMINX_IO_H
