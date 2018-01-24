#include <nginx/ngx_http_request.h>

#include <nminx/io.h>
#include <nminx/socket.h>
#include <nminx/server.h>
#include <nminx/http_request.h>


static void ngx_http_request_handler(socket_ctx_t* socket);

static void ngx_http_request_error_handler(socket_ctx_t* socket);
static void ngx_http_process_request_line(socket_ctx_t* socket);
static void ngx_http_process_request_headers(socket_ctx_t* socket);

static ssize_t ngx_http_read_request_header(ngx_http_request_t *r);
static ngx_int_t ngx_http_alloc_large_header_buffer(ngx_http_request_t *r, ngx_uint_t request_line);

static void ngx_http_writer(ngx_http_request_t *r);

static void ngx_http_write_stub_handler(socket_ctx_t* socket);
static void ngx_http_write_content_handler(socket_ctx_t* socket);

static ngx_int_t ngx_http_validate_host(ngx_str_t *host, ngx_pool_t *pool, ngx_uint_t alloc);
static void ngx_http_set_exten(ngx_http_request_t *r);

static ngx_int_t ngx_http_set_write_handler(ngx_http_request_t *r);
static void ngx_http_finalize_connection(ngx_http_request_t *r);

static void ngx_http_close_connection(http_connection_ctx_t *c);
static void ngx_http_terminate_request(ngx_http_request_t *r, ngx_int_t rc);
static void ngx_http_close_request(ngx_http_request_t *r, ngx_int_t rc);

static ngx_int_t ngx_http_init_headers_in_hash(config_t *cf, http_request_config_t *hrc);

static ngx_int_t ngx_http_process_header_line(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
static ngx_int_t ngx_http_process_unique_header_line(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
static ngx_int_t ngx_http_process_multi_header_lines(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
static ngx_int_t ngx_http_process_host(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
static ngx_int_t ngx_http_process_connection(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
static ngx_int_t ngx_http_process_user_agent(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);

static char *ngx_http_client_errors[] = {
    /* NGX_HTTP_PARSE_INVALID_METHOD */
    "client sent invalid method",

    /* NGX_HTTP_PARSE_INVALID_REQUEST */
    "client sent invalid request",

    /* NGX_HTTP_PARSE_INVALID_VERSION */
    "client sent invalid version",

    /* NGX_HTTP_PARSE_INVALID_09_METHOD */
    "client sent invalid method in HTTP/0.9 request"
};

ngx_http_header_t  ngx_http_headers_in[] = {
	{ ngx_string("Host"), offsetof(ngx_http_headers_in_t, host),
				 ngx_http_process_host },

	{ ngx_string("Connection"), offsetof(ngx_http_headers_in_t, connection),
				 ngx_http_process_connection },

	{ ngx_string("If-Modified-Since"),
				 offsetof(ngx_http_headers_in_t, if_modified_since),
				 ngx_http_process_unique_header_line },

	{ ngx_string("If-Unmodified-Since"),
				 offsetof(ngx_http_headers_in_t, if_unmodified_since),
				 ngx_http_process_unique_header_line },

	{ ngx_string("If-Match"),
				 offsetof(ngx_http_headers_in_t, if_match),
				 ngx_http_process_unique_header_line },

	{ ngx_string("If-None-Match"),
				 offsetof(ngx_http_headers_in_t, if_none_match),
				 ngx_http_process_unique_header_line },

	{ ngx_string("User-Agent"), offsetof(ngx_http_headers_in_t, user_agent),
				 ngx_http_process_user_agent },

	{ ngx_string("Referer"), offsetof(ngx_http_headers_in_t, referer),
				 ngx_http_process_header_line },

	{ ngx_string("Content-Length"),
				 offsetof(ngx_http_headers_in_t, content_length),
				 ngx_http_process_unique_header_line },

	{ ngx_string("Content-Range"),
				 offsetof(ngx_http_headers_in_t, content_range),
				 ngx_http_process_unique_header_line },

	{ ngx_string("Content-Type"),
				 offsetof(ngx_http_headers_in_t, content_type),
				 ngx_http_process_header_line },

	{ ngx_string("Range"), offsetof(ngx_http_headers_in_t, range),
				 ngx_http_process_header_line },

	{ ngx_string("If-Range"),
				 offsetof(ngx_http_headers_in_t, if_range),
				 ngx_http_process_unique_header_line },

	{ ngx_string("Transfer-Encoding"),
				 offsetof(ngx_http_headers_in_t, transfer_encoding),
				 ngx_http_process_header_line },

	{ ngx_string("Expect"),
				 offsetof(ngx_http_headers_in_t, expect),
				 ngx_http_process_unique_header_line },

	{ ngx_string("Upgrade"),
				 offsetof(ngx_http_headers_in_t, upgrade),
				 ngx_http_process_header_line },

#if (NGX_HTTP_GZIP)
	{ ngx_string("Accept-Encoding"),
				 offsetof(ngx_http_headers_in_t, accept_encoding),
				 ngx_http_process_header_line },

	{ ngx_string("Via"), offsetof(ngx_http_headers_in_t, via),
				 ngx_http_process_header_line },
#endif

	{ ngx_string("Authorization"),
				 offsetof(ngx_http_headers_in_t, authorization),
				 ngx_http_process_unique_header_line },

	{ ngx_string("Keep-Alive"), offsetof(ngx_http_headers_in_t, keep_alive),
				 ngx_http_process_header_line },

#if (NGX_HTTP_X_FORWARDED_FOR)
	{ ngx_string("X-Forwarded-For"),
				 offsetof(ngx_http_headers_in_t, x_forwarded_for),
				 ngx_http_process_multi_header_lines },
#endif

#if (NGX_HTTP_REALIP)
	{ ngx_string("X-Real-IP"),
				 offsetof(ngx_http_headers_in_t, x_real_ip),
				 ngx_http_process_header_line },
#endif

#if (NGX_HTTP_HEADERS)
	{ ngx_string("Accept"), offsetof(ngx_http_headers_in_t, accept),
				 ngx_http_process_header_line },

	{ ngx_string("Accept-Language"),
				 offsetof(ngx_http_headers_in_t, accept_language),
				 ngx_http_process_header_line },
#endif

#if (NGX_HTTP_DAV)
	{ ngx_string("Depth"), offsetof(ngx_http_headers_in_t, depth),
				 ngx_http_process_header_line },

	{ ngx_string("Destination"), offsetof(ngx_http_headers_in_t, destination),
				 ngx_http_process_header_line },

	{ ngx_string("Overwrite"), offsetof(ngx_http_headers_in_t, overwrite),
				 ngx_http_process_header_line },

	{ ngx_string("Date"), offsetof(ngx_http_headers_in_t, date),
				 ngx_http_process_header_line },
#endif

	{ ngx_string("Cookie"), offsetof(ngx_http_headers_in_t, cookies),
				 ngx_http_process_multi_header_lines },

    { ngx_null_string, 0, NULL }
};

//ngx_http_output_header_filter_pt  ngx_http_top_header_filter;
//ngx_http_output_body_filter_pt    ngx_http_top_body_filter;
//ngx_http_request_body_filter_pt   ngx_http_top_request_body_filter;

ngx_http_request_t* 
http_request_create(http_connection_ctx_t* ctx)
{
    ngx_pool_t                 *pool;
    ngx_http_request_t         *r;
    http_connection_ctx_t      *hc;

	config_t				   *conf;
	http_request_config_t	   *hrc;

    hc = ctx;
	conf = ctx->conf;
	hrc = get_http_req_conf(conf);

    pool = ngx_create_pool(hrc->pool_size);
    if (pool == NULL) {
        return NULL;
    }

    r = ngx_pcalloc(pool, sizeof(ngx_http_request_t));
    if (r == NULL) {
        ngx_destroy_pool(pool);
        return NULL;
    }
    r->pool = pool;
	r->connection = hc;

	/**
	 * @note	In nginx this code disable event polling for current connection
	 */
	r->read_event_handler = ngx_http_block_reading;

	/**
	 * @note	In nginx http_connection can hold large buffer 
	 *			If http request size mutch more then connection buffer 
	 *
	 * ngx_http_request.c:555
	 * @code{.c}
	 *		r->header_in = hc->busy ? hc->busy->buf : c->buffer;
	 * @endcode
	 */
    r->header_in = hc->lb->busy ? hc->lb->busy->buf : hc->buf;

    if (ngx_list_init(&r->headers_out.headers, r->pool, 20,
                      sizeof(ngx_table_elt_t))
        != NGX_OK)
    {
        ngx_destroy_pool(r->pool);
        return NULL;
    }

    if (ngx_list_init(&r->headers_out.trailers, r->pool, 4,
                      sizeof(ngx_table_elt_t))
        != NGX_OK)
    {
        ngx_destroy_pool(r->pool);
        return NULL;
    }

    r->method = NGX_HTTP_UNKNOWN;
    r->http_version = NGX_HTTP_VERSION_10;

    r->headers_in.content_length_n = -1;
    r->headers_in.keep_alive_n = -1;
    r->headers_out.content_length_n = -1;
    r->headers_out.last_modified_time = -1;

    r->uri_changes = NGX_HTTP_MAX_URI_CHANGES + 1;
    r->http_state = NGX_HTTP_READING_REQUEST_STATE;

    return r;
}

void http_request_destroy(ngx_http_request_t* r)
{
	ngx_http_free_request(r, 0);
}

void http_request_close(ngx_http_request_t* r)
{
	http_connection_ctx_t* hc;

	hc = r->connection;

	///@todo add handling for keepalive
	//if(r->keepalive) {}
	
	http_connection_close(hc);
}

int 
http_request_init_config(config_t* conf)
{
	http_request_config_t* hrc = get_http_req_conf(conf);
	if(ngx_http_init_headers_in_hash(conf, hrc) == NGX_ERROR)
	{
		return NMINX_ERROR;
	}

    //ngx_http_top_body_filter = ngx_http_write_filter;

	return NMINX_OK;
}

int 
http_request_init_events(http_connection_ctx_t* ctx)
{
	socket_ctx_t* socket = ctx->socket;

	socket->read_handler = ngx_http_process_request_line;
	socket->write_handler = ngx_http_write_stub_handler;
	socket->error_hanler = ngx_http_request_error_handler;

	return NMINX_OK;
}


void
ngx_http_block_reading(ngx_http_request_t *r)
{
	//ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
				   //"http reading blocked");

	//[> aio does not call this handler <]

	//if ((ngx_event_flags & NGX_USE_LEVEL_EVENT)
		//&& r->connection->read->active)
	//{
		//if (ngx_del_event(r->connection->read, NGX_READ_EVENT, 0) != NGX_OK) {
			//ngx_http_close_request(r, 0);
		//}
	//}
}

void
ngx_http_request_empty_handler(ngx_http_request_t *r)
{
    //ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   //"http request empty handler");

    return;
}


static void ngx_http_write_stub_handler(socket_ctx_t* socket)
{
	return;
}

static void ngx_http_request_error_handler(socket_ctx_t* socket)
{
	http_connection_ctx_t* hc = 
		(http_connection_ctx_t*) socket->data;

	hc->error = TRUE;

	int err = 0;
	socklen_t len = sizeof(err);

	if(socket_get_option(socket, SOL_SOCKET, SO_ERROR, (void*) &err, &len) == NMINX_OK)
	{
		if(err != ETIMEDOUT)
		{
			printf("Error on socket %d, error: %s\n", 
					socket->fd, strerror(err));
		}
		else
		{
			printf("Timeout on socket %d\n", socket->fd);
		}
	}
	else
	{
		printf("socket_get_option error\n");
	}
	ngx_http_close_request(hc->request, 0);
}

static void 
ngx_http_process_request_line(socket_ctx_t* socket)
{
    ssize_t						n;
    ngx_int_t					rc, rv;
    ngx_str_t					host;

    http_connection_ctx_t*		c;
    ngx_http_request_t*			r;

	c = (http_connection_ctx_t*) socket->data;
	r = c->request;

    //ngx_log_debug0(NGX_LOG_DEBUG_HTTP, rev->log, 0,
                   //"http process request line");

    rc = NGX_AGAIN;

    for ( ;; ) {

        if (rc == NGX_AGAIN) {
            n = ngx_http_read_request_header(r);

            if (n == NGX_AGAIN || n == NGX_ERROR) {
                return;
            }
        }

        rc = ngx_http_parse_request_line(r, r->header_in);

        if (rc == NGX_OK) {
            /* the request line has been parsed successfully */

            r->request_line.len = r->request_end - r->request_start;
            r->request_line.data = r->request_start;
            r->request_length = r->header_in->pos - r->request_start;

            r->method_name.len = r->method_end - r->request_start + 1;
            r->method_name.data = r->request_line.data;

            if (r->http_protocol.data) {
                r->http_protocol.len = r->request_end - r->http_protocol.data;
            }

            if (ngx_http_process_request_uri(r) != NGX_OK) {
                return;
            }

            if (r->host_start && r->host_end) {

                host.len = r->host_end - r->host_start;
                host.data = r->host_start;

                rc = ngx_http_validate_host(&host, r->pool, 0);

                if (rc == NGX_DECLINED) {
                    printf("client sent invalid host in request line\n");
                    ngx_http_finalize_request(r, NGX_HTTP_BAD_REQUEST);
					return;
                }
				
                if (rc == NGX_ERROR) {
                    ngx_http_close_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
					return;
                }

                r->headers_in.server = host;
            }

            if (r->http_version < NGX_HTTP_VERSION_10) {
				ngx_http_finalize_request(r, NGX_HTTP_VERSION_NOT_SUPPORTED);
				return;
            }

            if (ngx_list_init(&r->headers_in.headers, r->pool, 20,
                              sizeof(ngx_table_elt_t))
                != NGX_OK)
            {
                ngx_http_close_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
				return;
            }

			socket->read_handler = ngx_http_process_request_headers;
			socket_read_action(socket);
			return;
        }

        if (rc != NGX_AGAIN) {
            /* there was error while a request line parsing */
            printf("Invalid request line, error: %s\n", ngx_http_client_errors[rc - NGX_HTTP_CLIENT_ERROR]);

            if (rc == NGX_HTTP_PARSE_INVALID_VERSION) {
                ngx_http_finalize_request(r, NGX_HTTP_VERSION_NOT_SUPPORTED);
            } else {
                ngx_http_finalize_request(r, NGX_HTTP_BAD_REQUEST);
            }
			return;
        }

        /* NGX_AGAIN: a request line parsing is still incomplete */
        if (r->header_in->pos == r->header_in->end) {

            rv = ngx_http_alloc_large_header_buffer(r, 1);

            if (rv == NGX_ERROR) {
                ngx_http_close_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
				return;
            }

            if (rv == NGX_DECLINED) {
                r->request_line.len = r->header_in->end - r->request_start;
                r->request_line.data = r->request_start;

                printf("client sent too long URI \n");
                ngx_http_finalize_request(r, NGX_HTTP_REQUEST_URI_TOO_LARGE);
				return;
            }
        }
    }
}

static void
ngx_http_process_request_headers(socket_ctx_t* socket)
{
    u_char                     *p;
    size_t                      len;
    ssize_t                     n;
    ngx_int_t                   rc, rv;
    ngx_table_elt_t            *h;

	http_connection_ctx_t	   *c;
	config_t				   *conf;
	http_request_config_t	   *hrc;

    ngx_http_header_t          *hh;
    ngx_http_request_t         *r;

	c = (http_connection_ctx_t*) socket->data;
	r = c->request;

	conf = c->conf;
	hrc = get_http_req_conf(conf);


    //ngx_log_debug0(NGX_LOG_DEBUG_HTTP, rev->log, 0,
                   //"http process request header line");

    rc = NGX_AGAIN;

    for ( ;; ) {

        if (rc == NGX_AGAIN) {
			if (r->header_in->pos == r->header_in->end) {

				rv = ngx_http_alloc_large_header_buffer(r, 0);

				if (rv == NGX_ERROR) {
					ngx_http_close_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
					return;
				}

				if (rv == NGX_DECLINED) {
					p = r->header_name_start;

					r->lingering_close = 1;

					if (p == NULL) {
						printf("client sent too large request\n");
						ngx_http_finalize_request(r, NGX_HTTP_REQUEST_HEADER_TOO_LARGE);
						return;
					}

					printf("client sent too long header line\n");
					ngx_http_finalize_request(r, NGX_HTTP_REQUEST_HEADER_TOO_LARGE);
					return;
				}
			}

            n = ngx_http_read_request_header(r);
            if (n == NGX_AGAIN || n == NGX_ERROR) {
                return;
            }
        }

        /* the host header could change the server configuration context */
        rc = ngx_http_parse_header_line(r, r->header_in, 
				hrc->headers_flags & UNDERSCORES_IN_HEADERS);

        if (rc == NGX_OK) {

            r->request_length += r->header_in->pos - r->header_name_start;

            if (r->invalid_header 
					&& hrc->headers_flags & IGNORE_INVALID_HEADERS ) {
                /* there was error while a header line parsing */
                printf("client sent invalid header line\n");
                continue;
            }

            /* a header line has been parsed successfully */

            h = ngx_list_push(&r->headers_in.headers);
            if (h == NULL) {
                ngx_http_close_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
				return;
            }

            h->hash = r->header_hash;

            h->key.len = r->header_name_end - r->header_name_start;
            h->key.data = r->header_name_start;
            h->key.data[h->key.len] = '\0';

            h->value.len = r->header_end - r->header_start;
            h->value.data = r->header_start;
            h->value.data[h->value.len] = '\0';

            h->lowcase_key = ngx_pnalloc(r->pool, h->key.len);
            if (h->lowcase_key == NULL) {
                ngx_http_close_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
				return;
            }

            if (h->key.len == r->lowcase_index) {
                ngx_memcpy(h->lowcase_key, r->lowcase_header, h->key.len);
            } else {
                ngx_strlow(h->lowcase_key, h->key.data, h->key.len);
            }

			hh = ngx_hash_find(&hrc->headers_in_hash, h->hash,
							   h->lowcase_key, h->key.len);
							   
			if (hh && hh->handler(r, h, hh->offset) != NGX_OK) {
				return;
			}

            //ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           //"http header: \"%V: %V\"",
                           //&h->key, &h->value);

            continue;
        }

        if (rc == NGX_HTTP_PARSE_HEADER_DONE) {
            /* a whole header has been parsed successfully */

            //ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           //"http header done");

            r->request_length += r->header_in->pos - r->header_name_start;

            r->http_state = NGX_HTTP_PROCESS_REQUEST_STATE;

			///@note validate request headers
            rc = ngx_http_process_request_header(r);
            if (rc != NGX_OK) {
                return;
            }

			ngx_http_process_request(r);
            return;
        }

        if (rc == NGX_AGAIN) {
            /* a header line parsing is still not complete */
            continue;
        }

        /* rc == NGX_HTTP_PARSE_INVALID_HEADER */
        printf("client sent invalid header line\n");
        ngx_http_finalize_request(r, NGX_HTTP_BAD_REQUEST);
		return;
    }
}

static ssize_t
ngx_http_read_request_header(ngx_http_request_t *r)
{
    ssize_t                     n;
	http_connection_ctx_t*		c;

    c = r->connection;
    n = r->header_in->last - r->header_in->pos;

    if (n > 0) {
        return n;
    }

	n = socket_read(c->socket, r->header_in->last, 
			r->header_in->end - r->header_in->last);

	if(n == -1 && errno != EAGAIN)
	{
		c->error = 1;
        ngx_http_finalize_request(r, NGX_HTTP_BAD_REQUEST);
        return NGX_ERROR;
	}

	if(n == 0)
	{
		c->error = 1;
		ngx_http_finalize_request(r, NGX_HTTP_BAD_REQUEST);
		return NGX_ERROR;
	}

    r->header_in->last += n;

    return n;
}


ngx_int_t 
ngx_http_process_request_uri(ngx_http_request_t *r)
{
	config_t				*conf;
	http_connection_ctx_t	*c;
	http_request_config_t	*hrc;

	c = r->connection;
	conf = c->conf;
	hrc = get_http_req_conf(conf);

    if (r->args_start) {
        r->uri.len = r->args_start - 1 - r->uri_start;
    } else {
        r->uri.len = r->uri_end - r->uri_start;
    }

    if (r->complex_uri || r->quoted_uri) {

        r->uri.data = ngx_pnalloc(r->pool, r->uri.len + 1);
        if (r->uri.data == NULL) {
			ngx_http_close_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
			return NGX_ERROR;
        }

        if (ngx_http_parse_complex_uri(r, 
					hrc->headers_flags & URI_MERGE_SLASHES ) != NGX_OK) {
            r->uri.len = 0;

            printf("client sent invalid request \n");
            ngx_http_finalize_request(r, NGX_HTTP_BAD_REQUEST);
            return NGX_ERROR;
        }

    } else {
        r->uri.data = r->uri_start;
    }

    r->unparsed_uri.len = r->uri_end - r->uri_start;
    r->unparsed_uri.data = r->uri_start;

    r->valid_unparsed_uri = r->space_in_uri ? 0 : 1;

    if (r->uri_ext) {
        if (r->args_start) {
            r->exten.len = r->args_start - 1 - r->uri_ext;
        } else {
            r->exten.len = r->uri_end - r->uri_ext;
        }

        r->exten.data = r->uri_ext;
    }

    if (r->args_start && r->uri_end > r->args_start) {
        r->args.len = r->uri_end - r->args_start;
        r->args.data = r->args_start;
    }

#if (NGX_WIN32)
    {
    u_char  *p, *last;

    p = r->uri.data;
    last = r->uri.data + r->uri.len;

    while (p < last) {

        if (*p++ == ':') {

            /*
             * this check covers "::$data", "::$index_allocation" and
             * ":$i30:$index_allocation"
             */

            if (p < last && *p == '$') {
                printf("client sent unsafe win32 URI \n");
                ngx_http_finalize_request(r, NGX_HTTP_BAD_REQUEST);
                return NGX_ERROR;
            }
        }
    }

    p = r->uri.data + r->uri.len - 1;

    while (p > r->uri.data) {

        if (*p == ' ') {
            p--;
            continue;
        }

        if (*p == '.') {
            p--;
            continue;
        }

        break;
    }

    if (p != r->uri.data + r->uri.len - 1) {
        r->uri.len = p + 1 - r->uri.data;
        ngx_http_set_exten(r);
    }

    }
#endif

    return NGX_OK;
}


ngx_int_t
ngx_http_process_request_header(ngx_http_request_t *r)
{
	if (r->headers_in.server.len == 0)
	{
		return NGX_ERROR;
	}

    if (r->headers_in.host == NULL && r->http_version > NGX_HTTP_VERSION_10) {
        printf("client sent HTTP/1.1 request without \"Host\" header\n");
        ngx_http_finalize_request(r, NGX_HTTP_BAD_REQUEST);
        return NGX_ERROR;
    }

    if (r->headers_in.content_length) {
        r->headers_in.content_length_n =
                            ngx_atoof(r->headers_in.content_length->value.data,
                                      r->headers_in.content_length->value.len);

        if (r->headers_in.content_length_n == NGX_ERROR) {
            printf("client sent invalid \"Content-Length\" header\n");
            ngx_http_finalize_request(r, NGX_HTTP_BAD_REQUEST);
            return NGX_ERROR;
        }
    }

    if (r->method == NGX_HTTP_TRACE) {
        printf("client sent TRACE method\n");
        ngx_http_finalize_request(r, NGX_HTTP_NOT_ALLOWED);
        return NGX_ERROR;
    }

    if (r->headers_in.transfer_encoding) {
        if (r->headers_in.transfer_encoding->value.len == 7
            && ngx_strncasecmp(r->headers_in.transfer_encoding->value.data,
                               (u_char *) "chunked", 7) == 0)
        {
            r->headers_in.content_length = NULL;
            r->headers_in.content_length_n = -1;
            r->headers_in.chunked = 1;

        } else if (r->headers_in.transfer_encoding->value.len != 8
            || ngx_strncasecmp(r->headers_in.transfer_encoding->value.data,
                               (u_char *) "identity", 8) != 0)
        {
            printf("client sent unknown \"Transfer-Encoding\"\n");
            ngx_http_finalize_request(r, NGX_HTTP_NOT_IMPLEMENTED);
            return NGX_ERROR;
        }
    }

    if (r->headers_in.connection_type == NGX_HTTP_CONNECTION_KEEP_ALIVE) {
        if (r->headers_in.keep_alive) {
			///@note disable keep-alive
            //r->headers_in.keep_alive_n =
                            //ngx_atotm(r->headers_in.keep_alive->value.data,
                                      //r->headers_in.keep_alive->value.len);
            ngx_http_finalize_request(r, NGX_HTTP_NOT_IMPLEMENTED);
            return NGX_ERROR;
        }
    }

    return NGX_OK;
}

void
ngx_http_process_request(ngx_http_request_t *r)
{
    http_connection_ctx_t	*c;
	socket_ctx_t			*s;

    c = r->connection;
	s = c->socket;

	s->read_handler = ngx_http_request_handler;
	s->write_handler = ngx_http_request_handler;

	r->read_event_handler = ngx_http_block_reading;

	ngx_http_handler(r);
}

void
ngx_http_handler(ngx_http_request_t *r)
{
	ngx_int_t rc;

    if (!r->internal) {
        switch (r->headers_in.connection_type) {
        case 0:
            r->keepalive = (r->http_version > NGX_HTTP_VERSION_10);
            break;

        case NGX_HTTP_CONNECTION_CLOSE:
            r->keepalive = 0;
            break;

        case NGX_HTTP_CONNECTION_KEEP_ALIVE:
            r->keepalive = 1;
            break;
        }

        r->lingering_close = (r->headers_in.content_length_n > 0
                              || r->headers_in.chunked);
        r->phase_handler = 0;
    }

    r->valid_location = 1;
#if (NGX_HTTP_GZIP)
    r->gzip_tested = 0;
    r->gzip_ok = 0;
    r->gzip_vary = 0;
#endif

	ngx_http_finalize_request(r, ngx_http_helloworld_filter(r));
}

static void
ngx_http_request_handler(socket_ctx_t* sock)
{
    http_connection_ctx_t    *c;
    ngx_http_request_t		 *r;

	c = (http_connection_ctx_t*) sock->data;
    r = c->request;

    //ngx_http_set_log_request(c->log, r);

    //ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   //"http run request: \"%V?%V\"", &r->uri, &r->args);

    if (c->close || c->error) {
        ngx_http_terminate_request(r, 0);
        return;
    }

	if(sock->flags & IO_EVENT_WRITE) {
        r->write_event_handler(r);
    } else {
        r->read_event_handler(r);
    }
}

///@todo need logick for handling rc status
void ngx_http_finalize_request(ngx_http_request_t *r, ngx_int_t rc)
{
	http_connection_ctx_t	  *c;
	socket_ctx_t			  *sock;

    c = r->connection;
	sock = c->socket;

    if (rc == NGX_DONE) {
		// request done, if return ok server close socket
		// and destroy all linked data
		ngx_http_finalize_connection(r);
		return;
    }

	if (rc == NGX_OK && r->filter_finalize) {
		c->error = 1;
	}

	// NGX_DECLAINED - is a dicline phase
    //if (rc == NGX_DECLINED) {
        //r->content_handler = NULL;
        //r->write_event_handler = ngx_http_core_run_phases;
        //ngx_http_core_run_phases(r);
        //return;
    //}

    if (rc == NGX_ERROR
        || rc == NGX_HTTP_REQUEST_TIME_OUT
        || rc == NGX_HTTP_CLIENT_CLOSED_REQUEST
        || c->error)
    {	// return error ngx_http_finalize_request returned from handlers
		// if return error server close connection
        ngx_http_terminate_request(r, rc);
        return;
    }

	// handling error codes
    if (rc >= NGX_HTTP_SPECIAL_RESPONSE
        || rc == NGX_HTTP_CREATED
        || rc == NGX_HTTP_NO_CONTENT)
    {
        if (rc == NGX_HTTP_CLOSE) {
            ngx_http_terminate_request(r, rc);
            return;
        }

		sock->read_handler = ngx_http_request_handler;
		sock->write_handler = ngx_http_request_handler;

		r->keepalive = 0;
		r->header_only = 1;
		r->headers_out.status = rc;
		r->headers_out.content_length_n = 0;

		ngx_http_finalize_request(r, ngx_http_send_header(r));
        return;
    }

    //if (r->buffered || c->buffered || r->postponed) {
	//if (r->buffered) {
	if(r->out) {
        if (ngx_http_set_write_handler(r) != NGX_OK) {
            ngx_http_terminate_request(r, 0);
        }
		return;
    }

	// mark request as done
    r->done = 1;

	r->read_event_handler = ngx_http_block_reading;
	r->write_event_handler = ngx_http_request_empty_handler;

	r->request_complete = 1;

    ngx_http_finalize_connection(r);
}

static void
ngx_http_finalize_connection(ngx_http_request_t *r)
{
    if (r->reading_body) {
        r->keepalive = 0;
        r->lingering_close = 1;
    }

    //if (!ngx_terminate
         //&& !ngx_exiting
         //&& r->keepalive
         //&& clcf->keepalive_timeout > 0)
    //{
        //ngx_http_set_keepalive(r);
        //return;
    //}

	//if (clcf->lingering_close == NGX_HTTP_LINGERING_ALWAYS
		//|| (clcf->lingering_close == NGX_HTTP_LINGERING_ON
			//&& (r->lingering_close
				//|| r->header_in->pos < r->header_in->last
				//|| r->connection->read->ready)))
	//{
		//ngx_http_set_lingering_close(r);
		//return;
	//}

    ngx_http_close_request(r, 0);
}

static void
ngx_http_terminate_request(ngx_http_request_t *r, ngx_int_t rc)
{
    ngx_http_cleanup_t		*cln;
    ngx_http_request_t		*mr;
	http_connection_ctx_t	*hc;

    mr = r;
	hc = mr->connection;

    //ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   //"http terminate request count:%d", mr->count);

	if (rc > 0 && (mr->headers_out.status == 0 || hc->sent == 0)) {
		mr->headers_out.status = rc;
	}

    cln = mr->cleanup;
    mr->cleanup = NULL;

    while (cln) {
        if (cln->handler) {
            cln->handler(cln->data);
        }

        cln = cln->next;
    }

    //ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   //"http terminate cleanup count:%d blk:%d",
                   //mr->count, mr->blocked);

    ngx_http_close_request(mr, rc);
}

static void
ngx_http_close_request(ngx_http_request_t *r, ngx_int_t rc)
{
	http_connection_ctx_t	*c;

	c = r->connection;

    //ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   //"http request count:%d blk:%d", r->count, r->blocked);

	//if (r->count == 0) {
		//printf("http request count is zero");
	//}

    //r->count--;

    //if (r->count || r->blocked) {
        //return;
    //}

    ngx_http_free_request(r, rc);
	ngx_http_close_connection(c);
}


void
ngx_http_free_request(ngx_http_request_t *r, ngx_int_t rc)
{
	ngx_pool_t                *pool;
	ngx_http_cleanup_t        *cln;
	http_connection_ctx_t	  *hc;

	hc = r->connection;

    if (r->pool == NULL) {
        printf("http request already closed");
        return;
    }

    cln = r->cleanup;
    r->cleanup = NULL;

    while (cln) {
        if (cln->handler) {
            cln->handler(cln->data);
        }

        cln = cln->next;
    }

    //if (rc > 0 && (r->headers_out.status == 0 || hc->sent == 0)) {
        //r->headers_out.status = rc;
    //}

    /* the various request strings were allocated from r->pool */

    r->request_line.len = 0;
	hc->destroyed = 1;

    /*
     * Setting r->pool to NULL will increase probability to catch double close
     * of request since the request object is allocated from its own pool.
     */

    pool = r->pool;
    r->pool = NULL;

    ngx_destroy_pool(pool);
}

static void
ngx_http_close_connection(http_connection_ctx_t *c)
{
    c->destroyed = 1;
	http_connection_close(c);
}

void
ngx_http_test_reading(ngx_http_request_t *r)
{
    int						n;
    char					buf[1];
	ngx_err_t				err;
	http_connection_ctx_t	*c;
	socket_ctx_t			*sock;

	c = r->connection;
	sock = c->socket;

    //ngx_event_t       *rev;
    //ngx_connection_t  *c;

    //c = r->connection;
    //rev = c->read;

    //ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0, "http test reading");

	n = socket_recv(sock, buf, 1, MSG_PEEK);
    if (n == 0) {
        //rev->eof = 1;
        c->error = 1;
        err = 0;

		goto closed;
    } else if (n == -1) {
        err = ngx_socket_errno;

        if (err != NGX_EAGAIN) {
            //rev->eof = 1;
            c->error = 1;

			goto closed;
        }
    }
    //[> aio does not call this handler <]
    //if ((ngx_event_flags & NGX_USE_LEVEL_EVENT) && rev->active) {
        //if (ngx_del_event(rev, NGX_READ_EVENT, 0) != NGX_OK) {
            //ngx_http_close_request(r, 0);
        //}
    //}
	return;

closed:

    //if (err) {
        //rev->error = 1;
    //}

    printf("client prematurely closed connection\n");
	ngx_http_finalize_request(c->request, NGX_HTTP_CLIENT_CLOSED_REQUEST);
}


static void
ngx_http_writer(ngx_http_request_t *r)
{
    ngx_int_t                  rc;
	http_connection_ctx_t	   *hc;

	hc = r->connection;

	if(r->out)
	{	//write output
		//rc = chain_write(r->out);
		ngx_http_finalize_request(r, rc);
	}
	else 
	{
		ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
	}
}

static ngx_int_t
ngx_http_set_write_handler(ngx_http_request_t *r)
{
	socket_ctx_t			*sock;
	http_connection_ctx_t	*conn;

	conn = r->connection;
	sock = conn->socket;

    r->http_state = NGX_HTTP_WRITING_REQUEST_STATE;

	//r->read_event_handler = r->discard_body ?
                                //ngx_http_discarded_request_body_handler:
								//ngx_http_test_reading;   
								 
	r->read_event_handler = ngx_http_test_reading;
	r->write_event_handler = ngx_http_writer;

	//if(ngx_http_set_write_handler(r) == NGX_ERROR) {
		////close connection
	//}

	//sock->write_handler = ngx_http_write_content_handler;
	//if(io_poll_ctl(sock->io, IO_EVENT_WRITE, IO_CTL_MOD, sock) == NGX_ERROR) {
		////close connection
	//}

	//if error then error
	return io_poll_ctl(sock->io, IO_EVENT_READ | IO_EVENT_WRITE, IO_CTL_MOD, sock);
}

static void 
ngx_http_set_exten(ngx_http_request_t *r)
{
    ngx_int_t  i;

    ngx_str_null(&r->exten);

    for (i = r->uri.len - 1; i > 1; i--) {
        if (r->uri.data[i] == '.' && r->uri.data[i - 1] != '/') {

            r->exten.len = r->uri.len - i - 1;
            r->exten.data = &r->uri.data[i + 1];

            return;

        } else if (r->uri.data[i] == '/') {
            return;
        }
    }

    return;
}
                              
static ngx_int_t
ngx_http_alloc_large_header_buffer(ngx_http_request_t *r, ngx_uint_t request_line)
{
    u_char                    *old, *new;
    ngx_buf_t                 *b;
    ngx_chain_t               *cl;

	http_connection_ctx_t	  *hc;
	http_large_buffer_t		  *lb;

	config_t				  *conf;
	http_request_config_t	  *hrc;

	hc = r->connection;
	lb = hc->lb;

	conf = hc->conf;
	hrc = get_http_req_conf(conf);

    if (request_line && r->state == 0) {

        /* the client fills up the buffer with "\r\n" */

        r->header_in->pos = r->header_in->start;
        r->header_in->last = r->header_in->start;

        return NGX_OK;
    }

    old = request_line ? r->request_start : r->header_name_start;

    if (r->state != 0
        && (size_t) (r->header_in->pos - old)
									>= hrc->large_buffer_chunk_size)
    {
        return NGX_DECLINED;
    }

    if (lb->free) {
        cl = lb->free;
        lb->free = cl->next;

        b = cl->buf;

        //ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       //"http large header free: %p %uz",
                       //b->pos, b->end - b->last);

    } else if (lb->nbusy < hrc->large_buffer_chunk_count) {
		// allocate another one buffer chunk
        b = ngx_create_temp_buf(hc->pool,
                                hrc->large_buffer_chunk_size);
        if (b == NULL) {
            return NGX_ERROR;
        }

        cl = ngx_alloc_chain_link(hc->pool);
        if (cl == NULL) {
            return NGX_ERROR;
        }

        cl->buf = b;

        //ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       //"http large header alloc: %p %uz",
                       //b->pos, b->end - b->last);

    } else {
        return NGX_DECLINED;
    }

    cl->next = lb->busy;
    lb->busy = cl;
    lb->nbusy++;

    if (r->state == 0) {
        /*
         * r->state == 0 means that a header line was parsed successfully
         * and we do not need to copy incomplete header line and
         * to relocate the parser header pointers
         */

        r->header_in = b;

        return NGX_OK;
    }

    //ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   //"http large header copy: %uz", r->header_in->pos - old);

    new = b->start;

    ngx_memcpy(new, old, r->header_in->pos - old);

    b->pos = new + (r->header_in->pos - old);
    b->last = new + (r->header_in->pos - old);

    if (request_line) {
        r->request_start = new;

        if (r->request_end) {
            r->request_end = new + (r->request_end - old);
        }

        r->method_end = new + (r->method_end - old);

        r->uri_start = new + (r->uri_start - old);
        r->uri_end = new + (r->uri_end - old);

        if (r->schema_start) {
            r->schema_start = new + (r->schema_start - old);
            r->schema_end = new + (r->schema_end - old);
        }

        if (r->host_start) {
            r->host_start = new + (r->host_start - old);
            if (r->host_end) {
                r->host_end = new + (r->host_end - old);
            }
        }

        if (r->port_start) {
            r->port_start = new + (r->port_start - old);
            r->port_end = new + (r->port_end - old);
        }

        if (r->uri_ext) {
            r->uri_ext = new + (r->uri_ext - old);
        }

        if (r->args_start) {
            r->args_start = new + (r->args_start - old);
        }

        if (r->http_protocol.data) {
            r->http_protocol.data = new + (r->http_protocol.data - old);
        }

    } else {
        r->header_name_start = new;
        r->header_name_end = new + (r->header_name_end - old);
        r->header_start = new + (r->header_start - old);
        r->header_end = new + (r->header_end - old);
    }

    r->header_in = b;

    return NGX_OK;
}

static ngx_int_t
ngx_http_validate_host(ngx_str_t *host, ngx_pool_t *pool, ngx_uint_t alloc)
{
    u_char  *h, ch;
    size_t   i, dot_pos, host_len;

    enum {
        sw_usual = 0,
        sw_literal,
        sw_rest
    } state;

    dot_pos = host->len;
    host_len = host->len;

    h = host->data;

    state = sw_usual;

    for (i = 0; i < host->len; i++) {
        ch = h[i];

        switch (ch) {

        case '.':
            if (dot_pos == i - 1) {
                return NGX_DECLINED;
            }
            dot_pos = i;
            break;

        case ':':
            if (state == sw_usual) {
                host_len = i;
                state = sw_rest;
            }
            break;

        case '[':
            if (i == 0) {
                state = sw_literal;
            }
            break;

        case ']':
            if (state == sw_literal) {
                host_len = i + 1;
                state = sw_rest;
            }
            break;

        case '\0':
            return NGX_DECLINED;

        default:

            if (ngx_path_separator(ch)) {
                return NGX_DECLINED;
            }

            if (ch >= 'A' && ch <= 'Z') {
                alloc = 1;
            }

            break;
        }
    }

    if (dot_pos == host_len - 1) {
        host_len--;
    }

    if (host_len == 0) {
        return NGX_DECLINED;
    }

    if (alloc) {
        host->data = ngx_pnalloc(pool, host_len);
        if (host->data == NULL) {
            return NGX_ERROR;
        }

        ngx_strlow(host->data, h, host_len);
    }

    host->len = host_len;

    return NGX_OK;
}

static ngx_int_t
ngx_http_init_headers_in_hash(config_t *cf, http_request_config_t *hrc)
{
    ngx_array_t         headers_in;
    ngx_hash_key_t     *hk;
    ngx_hash_init_t     hash;
    ngx_http_header_t  *header;

	if (ngx_array_init(&headers_in, cf->temp_pool, 32, sizeof(ngx_hash_key_t))
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    for (header = ngx_http_headers_in; header->name.len; header++) {
        hk = ngx_array_push(&headers_in);
        if (hk == NULL) {
            return NGX_ERROR;
        }

        hk->key = header->name;
        hk->key_hash = ngx_hash_key_lc(header->name.data, header->name.len);
        hk->value = header;
    }

    hash.hash = &hrc->headers_in_hash;
    hash.key = ngx_hash_key_lc;
    hash.max_size = 512;
	hash.bucket_size = ngx_align(64, ngx_cacheline_size);
    hash.name = "headers_in_hash";
    hash.pool = cf->pool;
    hash.temp_pool = NULL;

    if (ngx_hash_init(&hash, headers_in.elts, headers_in.nelts) != NGX_OK) {
        return NGX_ERROR;
    }

    return NGX_OK;
}

static ngx_int_t
ngx_http_process_header_line(ngx_http_request_t *r, ngx_table_elt_t *h,
    ngx_uint_t offset)
{
    ngx_table_elt_t  **ph;

    ph = (ngx_table_elt_t **) ((char *) &r->headers_in + offset);

    if (*ph == NULL) {
        *ph = h;
    }

    return NGX_OK;
}

static ngx_int_t
ngx_http_process_unique_header_line(ngx_http_request_t *r, ngx_table_elt_t *h,
    ngx_uint_t offset)
{
    ngx_table_elt_t  **ph;

    ph = (ngx_table_elt_t **) ((char *) &r->headers_in + offset);

    if (*ph == NULL) {
        *ph = h;
        return NGX_OK;
    }

	printf("client sent duplicate header line");
    //ngx_log_error(NGX_LOG_INFO, r->connection->log, 0,
                  //"client sent duplicate header line: \"%V: %V\", "
                  //"previous value: \"%V: %V\"",
                  //&h->key, &h->value, &(*ph)->key, &(*ph)->value);

    ngx_http_finalize_request(r, NGX_HTTP_BAD_REQUEST);

    return NGX_ERROR;
}

static ngx_int_t
ngx_http_process_multi_header_lines(ngx_http_request_t *r, ngx_table_elt_t *h,
    ngx_uint_t offset)
{
    ngx_array_t       *headers;
    ngx_table_elt_t  **ph;

    headers = (ngx_array_t *) ((char *) &r->headers_in + offset);

    if (headers->elts == NULL) {
        if (ngx_array_init(headers, r->pool, 1, sizeof(ngx_table_elt_t *))
            != NGX_OK)
        {
            ngx_http_close_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
			return NGX_ERROR;
        }
    }

    ph = ngx_array_push(headers);
    if (ph == NULL) {
        ngx_http_close_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
		return NGX_ERROR;
    }

    *ph = h;
    return NGX_OK;
}


static ngx_int_t
ngx_http_process_host(ngx_http_request_t *r, ngx_table_elt_t *h,
    ngx_uint_t offset)
{
    ngx_int_t  rc;
    ngx_str_t  host;

    if (r->headers_in.host == NULL) {
        r->headers_in.host = h;
    }

    host = h->value;

    rc = ngx_http_validate_host(&host, r->pool, 0);

    if (rc == NGX_DECLINED) {
        printf("client sent invalid host header");
        ngx_http_finalize_request(r, NGX_HTTP_BAD_REQUEST);
        return NGX_ERROR;
    }

    if (rc == NGX_ERROR) {
        ngx_http_close_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
		return NGX_ERROR;
    }

    if (r->headers_in.server.len) {
        return NGX_OK;
    }
    r->headers_in.server = host;

    return NGX_OK;
}

static ngx_int_t
ngx_http_process_connection(ngx_http_request_t *r, ngx_table_elt_t *h,
    ngx_uint_t offset)
{
    if (ngx_strcasestrn(h->value.data, "close", 5 - 1)) {
        r->headers_in.connection_type = NGX_HTTP_CONNECTION_CLOSE;

    } else if (ngx_strcasestrn(h->value.data, "keep-alive", 10 - 1)) {
        r->headers_in.connection_type = NGX_HTTP_CONNECTION_KEEP_ALIVE;
    }

    return NGX_OK;
}

static ngx_int_t
ngx_http_process_user_agent(ngx_http_request_t *r, ngx_table_elt_t *h,
    ngx_uint_t offset)
{
    u_char  *user_agent, *msie;

    if (r->headers_in.user_agent) {
        return NGX_OK;
    }

    r->headers_in.user_agent = h;

    /* check some widespread browsers while the header is in CPU cache */

    user_agent = h->value.data;

    msie = ngx_strstrn(user_agent, "MSIE ", 5 - 1);

    if (msie && msie + 7 < user_agent + h->value.len) {

        r->headers_in.msie = 1;

        if (msie[6] == '.') {

            switch (msie[5]) {
            case '4':
            case '5':
                r->headers_in.msie6 = 1;
                break;
            case '6':
                if (ngx_strstrn(msie + 8, "SV1", 3 - 1) == NULL) {
                    r->headers_in.msie6 = 1;
                }
                break;
            }
        }

#if 0
        /* MSIE ignores the SSL "close notify" alert */
        if (c->ssl) {
            c->ssl->no_send_shutdown = 1;
        }
#endif
    }

    if (ngx_strstrn(user_agent, "Opera", 5 - 1)) {
        r->headers_in.opera = 1;
        r->headers_in.msie = 0;
        r->headers_in.msie6 = 0;
    }

    if (!r->headers_in.msie && !r->headers_in.opera) {

        if (ngx_strstrn(user_agent, "Gecko/", 6 - 1)) {
            r->headers_in.gecko = 1;

        } else if (ngx_strstrn(user_agent, "Chrome/", 7 - 1)) {
            r->headers_in.chrome = 1;

        } else if (ngx_strstrn(user_agent, "Safari/", 7 - 1)
                   && ngx_strstrn(user_agent, "Mac OS X", 8 - 1))
        {
            r->headers_in.safari = 1;

        } else if (ngx_strstrn(user_agent, "Konqueror", 9 - 1)) {
            r->headers_in.konqueror = 1;
        }
    }

    return NGX_OK;
}

