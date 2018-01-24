
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <nginx/ngx_config.h>
#include <nginx/ngx_core.h>
#include <nginx/ngx_http.h>
#include <nginx/ngx_http_request.h>

#include <nminx/http_request.h>
#include <nminx/socket.h>


#define NGX_SENDFILE_MAXSIZE  2147483647L

ngx_chain_t *
ngx_output_chain_to_iovec(ngx_iovec_t *vec, ngx_chain_t *in, size_t limit)
{
    size_t         total, size;
    u_char        *prev;
    ngx_uint_t     n;
    struct iovec  *iov;

    iov = NULL;
    prev = NULL;
    total = 0;
    n = 0;

    for ( /* void */ ; in && total < limit; in = in->next) {

        if (ngx_buf_special(in->buf)) {
            continue;
        }

        //if (in->buf->in_file) {
            //break;
        //}

        if (!ngx_buf_in_memory(in->buf)) {
            printf(
                          "bad buf in output chain "
                          "t:%d r:%d f:%d %p %p-%p \n",
                          in->buf->temporary,
                          in->buf->recycled,
                          in->buf->in_file,
                          in->buf->start,
                          in->buf->pos,
                          in->buf->last);

            //ngx_debug_point();
            return NGX_CHAIN_ERROR;
        }

        size = in->buf->last - in->buf->pos;

        if (size > limit - total) {
            size = limit - total;
        }

        if (prev == in->buf->pos) {
            iov->iov_len += size;

        } else {
            if (n == vec->nalloc) {
                break;
            }

            iov = &vec->iovs[n++];

            iov->iov_base = (void *) in->buf->pos;
            iov->iov_len = size;
        }

        prev = in->buf->pos + size;
        total += size;
    }

    vec->count = n;
    vec->size = total;

    return in;
}


ssize_t
ngx_writev(http_connection_ctx_t *c, ngx_iovec_t *vec)
{
    ssize_t    n;
    ngx_err_t  err;

eintr:

    n = socket_writev(c->socket, vec->iovs, vec->count);

    //ngx_log_debug2(NGX_LOG_DEBUG_EVENT, c->log, 0,
                   //"writev: %z of %uz", n, vec->size);

    if (n == -1) {
        err = errno;

        switch (err) {
        case NGX_EAGAIN:
            //ngx_log_debug0(NGX_LOG_DEBUG_EVENT, c->log, err,
                           //"writev() not ready");
            return NGX_AGAIN;

        case NGX_EINTR:
            //ngx_log_debug0(NGX_LOG_DEBUG_EVENT, c->log, err,
                           //"writev() was interrupted");
            goto eintr;

        default:
			c->error = 1;
            //c->write->error = 1;
            //ngx_connection_error(c, err, "writev() failed");
            return NGX_ERROR;
        }
    }

    return n;
}

ngx_chain_t *
ngx_send_chain(http_connection_ctx_t *c, ngx_chain_t *in, off_t limit)
{
    int            tcp_nodelay;
    off_t          send, prev_send;
    size_t         file_size, sent;
    ssize_t        n;
    ngx_err_t      err;
    ngx_buf_t     *file;
    ngx_event_t   *wev;
    ngx_chain_t   *cl;
    ngx_iovec_t    header;
    struct iovec   headers[NGX_IOVS_PREALLOCATE];

    //wev = c->write;

    //if (!wev->ready) {
        //return in;
    //}


    /* the maximum limit size is 2G-1 - the page size */
    if (limit == 0 || limit > (off_t) (NGX_SENDFILE_MAXSIZE - ngx_pagesize)) {
        limit = NGX_SENDFILE_MAXSIZE - ngx_pagesize;
    }


    send = 0;

    header.iovs = headers;
    header.nalloc = NGX_IOVS_PREALLOCATE;

    for ( ;; ) {
        prev_send = send;

        /* create the iovec and coalesce the neighbouring bufs */
        cl = ngx_output_chain_to_iovec(&header, in, limit - send);

        if (cl == NGX_CHAIN_ERROR) {
            return NGX_CHAIN_ERROR;
        }

        send += header.size;

		n = ngx_writev(c, &header);
		if (n == NGX_ERROR) {
			return NGX_CHAIN_ERROR;
		}
		sent = (n == NGX_AGAIN) ? 0 : n;

        c->sent += sent;

        in = ngx_chain_update_sent(in, sent);

        if (n == NGX_AGAIN) {
            return in;
        }

        if ((size_t) (send - prev_send) != sent) {

            /*
             * sendfile() on Linux 4.3+ might be interrupted at any time,
             * and provides no indication if it was interrupted or not,
             * so we have to retry till an explicit EAGAIN
             *
             * sendfile() in threads can also report less bytes written
             * than we are prepared to send now, since it was started in
             * some point in the past, so we again have to retry
             */

            send = prev_send + sent;
            continue;
        }

        if (send >= limit || in == NULL) {
            return in;
        }
    }
}


ngx_int_t
ngx_http_write_filter(ngx_http_request_t *r, ngx_chain_t *in)
{
    off_t                      size, sent, nsent, limit;
    ngx_uint_t                 last, flush, sync;
    ngx_msec_t                 delay;
    ngx_chain_t               *cl, *ln, **ll, *chain;

	http_connection_ctx_t	  *c;
	http_request_config_t	  *rq;

    //ngx_http_core_loc_conf_t  *clcf;

    c = r->connection;
	rq = get_http_req_conf(c->conf);

    if (c->error) {
        return NGX_ERROR;
    }

    size = 0;
    flush = 0;
    sync = 0;
    last = 0;
    ll = &r->out;

    /* find the size, the flush point and the last link of the saved chain */

    for (cl = r->out; cl; cl = cl->next) {
        ll = &cl->next;

        //ngx_log_debug7(NGX_LOG_DEBUG_EVENT, c->log, 0,
                       //"write old buf t:%d f:%d %p, pos %p, size: %z "
                       //"file: %O, size: %O",
                       //cl->buf->temporary, cl->buf->in_file,
                       //cl->buf->start, cl->buf->pos,
                       //cl->buf->last - cl->buf->pos,
                       //cl->buf->file_pos,
                       //cl->buf->file_last - cl->buf->file_pos);

        size += ngx_buf_size(cl->buf);

        if (cl->buf->flush || cl->buf->recycled) {
            flush = 1;
        }

        if (cl->buf->sync) {
            sync = 1;
        }

        if (cl->buf->last_buf) {
            last = 1;
        }
    }

    /* add the new chain to the existent one */

    for (ln = in; ln; ln = ln->next) {
        cl = ngx_alloc_chain_link(r->pool);
        if (cl == NULL) {
            return NGX_ERROR;
        }

        cl->buf = ln->buf;
        *ll = cl;
        ll = &cl->next;

        //ngx_log_debug7(NGX_LOG_DEBUG_EVENT, c->log, 0,
                       //"write new buf t:%d f:%d %p, pos %p, size: %z "
                       //"file: %O, size: %O",
                       //cl->buf->temporary, cl->buf->in_file,
                       //cl->buf->start, cl->buf->pos,
                       //cl->buf->last - cl->buf->pos,
                       //cl->buf->file_pos,
                       //cl->buf->file_last - cl->buf->file_pos);

        size += ngx_buf_size(cl->buf);

        if (cl->buf->flush || cl->buf->recycled) {
            flush = 1;
        }

        if (cl->buf->sync) {
            sync = 1;
        }

        if (cl->buf->last_buf) {
            last = 1;
        }
    }

    *ll = NULL;

    if (size == 0
        && !(c->buffered & NGX_LOWLEVEL_BUFFERED)
        && !(last && c->need_last_buf))
    {
        if (last || flush || sync) {
            for (cl = r->out; cl; /* void */) {
                ln = cl;
                cl = cl->next;
                ngx_free_chain(r->pool, ln);
            }

            r->out = NULL;
            c->buffered &= ~NGX_HTTP_WRITE_BUFFERED;

            return NGX_OK;
        }

        printf("the http output chain is empty\n");
        //ngx_debug_point();
        return NGX_ERROR;
    }

    limit = 0;
    sent = c->sent;

    //ngx_log_debug1(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   //"http write filter limit %O", limit);

	chain = ngx_send_chain(c, r->out, limit);

    //ngx_log_debug1(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   //"http write filter %p", chain);

    if (chain == NGX_CHAIN_ERROR) {
        c->error = 1;
        return NGX_ERROR;
    }

    for (cl = r->out; cl && cl != chain; /* void */) {
        ln = cl;
        cl = cl->next;
        ngx_free_chain(r->pool, ln);
    }

    r->out = chain;

    if (chain) {
        c->buffered |= NGX_HTTP_WRITE_BUFFERED;
        return NGX_AGAIN;
    }

    c->buffered &= ~NGX_HTTP_WRITE_BUFFERED;

    //if ((c->buffered & NGX_LOWLEVEL_BUFFERED) && r->postponed == NULL) {
        //return NGX_AGAIN;
    //}

    return NGX_OK;
}

