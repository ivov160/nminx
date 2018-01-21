#include "http_request.h"

static int ngx_http_process_request_line(socket_ctx_t* socket);
static int ngx_http_process_request_headers(socket_ctx_t* socket);
static ssize_t ngx_http_read_request_header(ngx_http_request_t *r);
static ngx_int_t ngx_http_validate_host(ngx_str_t *host, ngx_pool_t *pool, ngx_uint_t alloc);

static void ngx_http_set_exten(ngx_http_request_t *r);

//static void ngx_http_finalize_request(ngx_http_request_t *r, ngx_int_t rc);
static void ngx_http_close_request(ngx_http_request_t *r, ngx_int_t rc);

static char *ngx_http_client_errors[] = {

    /* NGX_HTTP_PARSE_INVALID_METHOD */
    "client sent invalid method\n",

    /* NGX_HTTP_PARSE_INVALID_REQUEST */
    "client sent invalid request\n",

    /* NGX_HTTP_PARSE_INVALID_VERSION */
    "client sent invalid version\n",

    /* NGX_HTTP_PARSE_INVALID_09_METHOD */
    "client sent invalid method in HTTP/0.9 request\n"
};

ngx_http_header_t  ngx_http_headers_in[] = {
    //{ ngx_string("Host"), offsetof(ngx_http_headers_in_t, host),
                 //ngx_http_process_host },

    //{ ngx_string("Connection"), offsetof(ngx_http_headers_in_t, connection),
                 //ngx_http_process_connection },

    //{ ngx_string("If-Modified-Since"),
                 //offsetof(ngx_http_headers_in_t, if_modified_since),
                 //ngx_http_process_unique_header_line },

    //{ ngx_string("If-Unmodified-Since"),
                 //offsetof(ngx_http_headers_in_t, if_unmodified_since),
                 //ngx_http_process_unique_header_line },

    //{ ngx_string("If-Match"),
                 //offsetof(ngx_http_headers_in_t, if_match),
                 //ngx_http_process_unique_header_line },

    //{ ngx_string("If-None-Match"),
                 //offsetof(ngx_http_headers_in_t, if_none_match),
                 //ngx_http_process_unique_header_line },

    //{ ngx_string("User-Agent"), offsetof(ngx_http_headers_in_t, user_agent),
                 //ngx_http_process_user_agent },

    //{ ngx_string("Referer"), offsetof(ngx_http_headers_in_t, referer),
                 //ngx_http_process_header_line },

    //{ ngx_string("Content-Length"),
                 //offsetof(ngx_http_headers_in_t, content_length),
                 //ngx_http_process_unique_header_line },

    //{ ngx_string("Content-Range"),
                 //offsetof(ngx_http_headers_in_t, content_range),
                 //ngx_http_process_unique_header_line },

    //{ ngx_string("Content-Type"),
                 //offsetof(ngx_http_headers_in_t, content_type),
                 //ngx_http_process_header_line },

    //{ ngx_string("Range"), offsetof(ngx_http_headers_in_t, range),
                 //ngx_http_process_header_line },

    //{ ngx_string("If-Range"),
                 //offsetof(ngx_http_headers_in_t, if_range),
                 //ngx_http_process_unique_header_line },

    //{ ngx_string("Transfer-Encoding"),
                 //offsetof(ngx_http_headers_in_t, transfer_encoding),
                 //ngx_http_process_header_line },

    //{ ngx_string("Expect"),
                 //offsetof(ngx_http_headers_in_t, expect),
                 //ngx_http_process_unique_header_line },

    //{ ngx_string("Upgrade"),
                 //offsetof(ngx_http_headers_in_t, upgrade),
                 //ngx_http_process_header_line },

//#if (NGX_HTTP_GZIP)
    //{ ngx_string("Accept-Encoding"),
                 //offsetof(ngx_http_headers_in_t, accept_encoding),
                 //ngx_http_process_header_line },

    //{ ngx_string("Via"), offsetof(ngx_http_headers_in_t, via),
                 //ngx_http_process_header_line },
//#endif

    //{ ngx_string("Authorization"),
                 //offsetof(ngx_http_headers_in_t, authorization),
                 //ngx_http_process_unique_header_line },

    //{ ngx_string("Keep-Alive"), offsetof(ngx_http_headers_in_t, keep_alive),
                 //ngx_http_process_header_line },

//#if (NGX_HTTP_X_FORWARDED_FOR)
    //{ ngx_string("X-Forwarded-For"),
                 //offsetof(ngx_http_headers_in_t, x_forwarded_for),
                 //ngx_http_process_multi_header_lines },
//#endif

//#if (NGX_HTTP_REALIP)
    //{ ngx_string("X-Real-IP"),
                 //offsetof(ngx_http_headers_in_t, x_real_ip),
                 //ngx_http_process_header_line },
//#endif

//#if (NGX_HTTP_HEADERS)
    //{ ngx_string("Accept"), offsetof(ngx_http_headers_in_t, accept),
                 //ngx_http_process_header_line },

    //{ ngx_string("Accept-Language"),
                 //offsetof(ngx_http_headers_in_t, accept_language),
                 //ngx_http_process_header_line },
//#endif

//#if (NGX_HTTP_DAV)
    //{ ngx_string("Depth"), offsetof(ngx_http_headers_in_t, depth),
                 //ngx_http_process_header_line },

    //{ ngx_string("Destination"), offsetof(ngx_http_headers_in_t, destination),
                 //ngx_http_process_header_line },

    //{ ngx_string("Overwrite"), offsetof(ngx_http_headers_in_t, overwrite),
                 //ngx_http_process_header_line },

    //{ ngx_string("Date"), offsetof(ngx_http_headers_in_t, date),
                 //ngx_http_process_header_line },
//#endif

    //{ ngx_string("Cookie"), offsetof(ngx_http_headers_in_t, cookies),
                 //ngx_http_process_multi_header_lines },

    { ngx_null_string, 0, NULL }
};

ngx_http_request_t* http_request_create(http_connection_ctx_t* ctx)
{
    ngx_pool_t                 *pool;
    ngx_http_request_t         *r;
    http_connection_ctx_t      *hc;

    //c->requests++;
    hc = ctx;

    pool = ngx_create_pool(hc->m_cfg->request_pool_size);
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
    //r->read_event_handler = ngx_http_block_reading;

	/**
	 * @note	In nginx http_connection can hold large buffer 
	 *			If http request size mutch more then connection buffer 
	 *			(I ignore that)
	 *
	 * ngx_http_request.c:555
	 * @code{.c}
	 *		r->header_in = hc->busy ? hc->busy->buf : c->buffer;
	 * @endcode
	 */
	r->header_in = hc->buf;

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

    //r->main = r;
    r->count = 1;

    //tp = ngx_timeofday();
    //r->start_sec = tp->sec;
    //r->start_msec = tp->msec;

    r->method = NGX_HTTP_UNKNOWN;
    r->http_version = NGX_HTTP_VERSION_10;

    r->headers_in.content_length_n = -1;
    r->headers_in.keep_alive_n = -1;
    r->headers_out.content_length_n = -1;
    r->headers_out.last_modified_time = -1;

    r->uri_changes = NGX_HTTP_MAX_URI_CHANGES + 1;
	r->subrequests = NGX_HTTP_MAX_SUBREQUESTS + 1;

    r->http_state = NGX_HTTP_READING_REQUEST_STATE;

    return r;
}

int http_request_init_events(http_connection_ctx_t* ctx)
{
	socket_ctx_t* socket = ctx->socket;

	socket->read_handler = ngx_http_process_request_line;
	return NMINX_OK;
}

static int ngx_http_process_request_line(socket_ctx_t* socket)
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
                return n;
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
                    return NGX_ERROR;
                }

                r->headers_in.server = host;
            }

            if (r->http_version < NGX_HTTP_VERSION_10) {

                ngx_http_process_request(r);
                return;
            }


            if (ngx_list_init(&r->headers_in.headers, r->pool, 20,
                              sizeof(ngx_table_elt_t))
                != NGX_OK)
            {
                ngx_http_close_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
                return NMINX_ERROR;
            }

			socket->read_handler = ngx_http_process_request_headers;
			return socket_read_action(socket);
        }

        if (rc != NGX_AGAIN) {

            /* there was error while a request line parsing */

            printf(ngx_http_client_errors[rc - NGX_HTTP_CLIENT_ERROR]);

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
	return rc;
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

	if(NMINX_ERROR)
	{
		c->error = 1;
        ngx_http_finalize_request(r, NGX_HTTP_BAD_REQUEST);
        return NGX_ERROR;
	}

    r->header_in->last += n;

    return n;
}


ngx_int_t ngx_http_process_request_uri(ngx_http_request_t *r)
{
    //ngx_http_core_srv_conf_t  *cscf;

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

        //cscf = ngx_http_get_module_srv_conf(r, ngx_http_core_module);
        //if (ngx_http_parse_complex_uri(r, cscf->merge_slashes) != NGX_OK) {
        if (ngx_http_parse_complex_uri(r, TRUE) != NGX_OK) {
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

static void ngx_http_set_exten(ngx_http_request_t *r)
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
                              


//static void
//ngx_http_close_request(ngx_http_request_t *r, ngx_int_t rc)
//{
    //ngx_connection_t  *c;

    //r = r->main;
    //c = r->connection;

    //ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   //"http request count:%d blk:%d", r->count, r->blocked);

    //if (r->count == 0) {
        //ngx_log_error(NGX_LOG_ALERT, c->log, 0, "http request count is zero");
    //}

    //r->count--;

    //if (r->count || r->blocked) {
        //return;
    //}

//#if (NGX_HTTP_V2)
    //if (r->stream) {
        //ngx_http_v2_close_stream(r->stream, rc);
        //return;
    //}
//#endif

    //ngx_http_free_request(r, rc);
    //ngx_http_close_connection(c);
//}


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

static int
ngx_http_process_request_headers(socket_ctx_t* socket)
{
    u_char                     *p;
    size_t                      len;
    ssize_t                     n;
    ngx_int_t                   rc, rv;
    ngx_table_elt_t            *h;
    //ngx_connection_t           *c;

	http_connection_ctx_t	   *c;

    ngx_http_header_t          *hh;
    ngx_http_request_t         *r;

    //ngx_http_core_srv_conf_t   *cscf;
    //ngx_http_core_main_conf_t  *cmcf;

	c = (http_connection_ctx_t*) socket->data;
	r = c->request;

    //ngx_log_debug0(NGX_LOG_DEBUG_HTTP, rev->log, 0,
                   //"http process request header line");

    //cmcf = ngx_http_get_module_main_conf(r, ngx_http_core_module);

    rc = NGX_AGAIN;

    for ( ;; ) {

        if (rc == NGX_AGAIN) {

            //if (r->header_in->pos == r->header_in->end) {

                //rv = ngx_http_alloc_large_header_buffer(r, 0);

                //if (rv == NGX_ERROR) {
                    //ngx_http_close_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
                    //return;
                //}

                //if (rv == NGX_DECLINED) {
                    //p = r->header_name_start;

                    //r->lingering_close = 1;

                    //if (p == NULL) {
                        //ngx_log_error(NGX_LOG_INFO, c->log, 0,
                                      //"client sent too large request");
                        //ngx_http_finalize_request(r,
                                            //NGX_HTTP_REQUEST_HEADER_TOO_LARGE);
                        //return;
                    //}

                    //len = r->header_in->end - p;

                    //if (len > NGX_MAX_ERROR_STR - 300) {
                        //len = NGX_MAX_ERROR_STR - 300;
                    //}

                    //ngx_log_error(NGX_LOG_INFO, c->log, 0,
                                //"client sent too long header line: \"%*s...\"",
                                //len, r->header_name_start);

                    //ngx_http_finalize_request(r,
                                            //NGX_HTTP_REQUEST_HEADER_TOO_LARGE);
                    //return;
                //}
            //}

            n = ngx_http_read_request_header(r);

            if (n == NGX_AGAIN || n == NGX_ERROR) {
                return n;
            }
        }

        /* the host header could change the server configuration context */
        //cscf = ngx_http_get_module_srv_conf(r, ngx_http_core_module);

        //rc = ngx_http_parse_header_line(r, r->header_in,
                                        //cscf->underscores_in_headers);

        rc = ngx_http_parse_header_line(r, r->header_in, TRUE);
        if (rc == NGX_OK) {

            r->request_length += r->header_in->pos - r->header_name_start;

            //if (r->invalid_header && cscf->ignore_invalid_headers) {
            if (r->invalid_header) {
                /* there was error while a header line parsing */
                printf( "client sent invalid header line: \"%*s\" \n",
						  r->header_end - r->header_name_start,
						  r->header_name_start);
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

            //hh = ngx_hash_find(&cmcf->headers_in_hash, h->hash,
                               //h->lowcase_key, h->key.len);
            hh = ngx_hash_find(10, h->hash,
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


ngx_int_t
ngx_http_process_request_header(ngx_http_request_t *r)
{
    //if (r->headers_in.server.len == 0
        //&& ngx_http_set_virtual_server(r, &r->headers_in.server)
           //== NGX_ERROR)
    //{
        //return NGX_ERROR;
    //}

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
            r->headers_in.keep_alive_n =
                            ngx_atotm(r->headers_in.keep_alive->value.data,
                                      r->headers_in.keep_alive->value.len);
        }
    }

    return NGX_OK;
}

void
ngx_http_process_request(ngx_http_request_t *r)
{
    http_connection_ctx_t  *c;

    c = r->connection;

    //c->read->handler = ngx_http_request_handler;
    //c->write->handler = ngx_http_request_handler;
    //r->read_event_handler = ngx_http_block_reading;

    ngx_http_handler(r);
}
