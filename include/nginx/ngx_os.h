/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_OS_H_INCLUDED_
#define _NGX_OS_H_INCLUDED_


#include <nginx/ngx_config.h>
#include <nginx/ngx_core.h>

//#include <nminx/http_connection.h>


#define NGX_IO_SENDFILE    1

#if (IOV_MAX > 64)
#define NGX_IOVS_PREALLOCATE  64
#else
#define NGX_IOVS_PREALLOCATE  IOV_MAX
#endif


typedef struct {
    struct iovec  *iovs;
    ngx_uint_t     count;
    size_t         size;
    ngx_uint_t     nalloc;
} ngx_iovec_t;

//ngx_chain_t *ngx_output_chain_to_iovec(ngx_iovec_t *vec, ngx_chain_t *in, size_t limit);
//ssize_t ngx_writev(http_connection_ctx_t *c, ngx_iovec_t *vec);


#if (NGX_FREEBSD)
#include <ngx_freebsd.h>


#elif (NGX_LINUX)
#include <ngx_linux.h>


#elif (NGX_SOLARIS)
#include <ngx_solaris.h>


#elif (NGX_DARWIN)
#include <ngx_darwin.h>
#endif


#endif /* _NGX_OS_H_INCLUDED_ */
