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

/**
 * @note	flags value getted from mtcp api 
 *			if mtcp changed to another lib, need convert flags 
 */
enum io_event_flag
{
	IO_EVENT_NONE = 0x000,
	IO_EVENT_READ = 0x001,
	IO_EVENT_WRITE = 0x004,
	IO_EVENT_ERROR = 0x008,
};

io_ctx_t* io_init(nminx_config_t* cfg);
int io_destroy(io_ctx_t* io);

int io_poll_ctl(io_ctx_t* io, int op, int flags, socket_ctx_t* sock);
int io_poll_events(io_ctx_t* io, socket_ctx_t** s_buff, int s_buff_size);

#endif //_NMINX_IO_H
