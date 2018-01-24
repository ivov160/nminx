#ifndef _NMINX_IO_CTX_H
#define _NMINX_IO_CTX_H

#include <mtcp_api.h>
#include <mtcp_epoll.h>

// pimpl data for io
struct io_ctx_s 
{
	int ep;
	mctx_t mctx;
};

#endif //_NMINX_IO_CTX_H
