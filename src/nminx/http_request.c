#include "http_request.h"

#include <nminx/io.h>
#include <nminx/socket.h>
#include <nminx/server.h>
#include <nminx/http_connection.h>


static int ngx_http_process_request_line(socket_ctx_t* socket);
static int ngx_http_process_request_headers(socket_ctx_t* socket);

static int ngx_http_write_stub_handler(socket_ctx_t* socket);
static int ngx_http_write_content_handler(socket_ctx_t* socket);

static ssize_t ngx_http_read_request_header(ngx_http_request_t *r);
static ngx_int_t ngx_http_validate_host(ngx_str_t *host, ngx_pool_t *pool, ngx_uint_t alloc);
static ngx_int_t ngx_http_alloc_large_header_buffer(ngx_http_request_t *r, ngx_uint_t request_line);

static void ngx_http_set_exten(ngx_http_request_t *r);
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
	r->connection = (void*) hc;

	/**
	 * @note	In nginx this code disable event polling for current connection
	 */
    //r->read_event_handler = ngx_http_block_reading;

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
	r->subrequests = NGX_HTTP_MAX_SUBREQUESTS + 1;

    r->http_state = NGX_HTTP_READING_REQUEST_STATE;

    return r;
}

void http_request_destroy(ngx_http_request_t* r)
{
	ngx_destroy_pool(r->pool);
}

int 
http_request_init_config(config_t* conf)
{
	http_request_config_t* hrc = get_http_req_conf(conf);
	if(ngx_http_init_headers_in_hash(conf, hrc) == NGX_ERROR)
	{
		return NMINX_ERROR;
	}
	return NMINX_OK;
}


int 
http_request_init_events(http_connection_ctx_t* ctx)
{
	socket_ctx_t* socket = ctx->socket;

	socket->read_handler = ngx_http_process_request_line;
	socket->write_handler = ngx_http_write_stub_handler;

	return NMINX_OK;
}

static int 
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
                return NGX_ERROR;
            }

            if (r->host_start && r->host_end) {

                host.len = r->host_end - r->host_start;
                host.data = r->host_start;

                rc = ngx_http_validate_host(&host, r->pool, 0);

                if (rc == NGX_DECLINED) {
                    printf("client sent invalid host in request line\n");
                    return ngx_http_finalize_request(r, NGX_HTTP_BAD_REQUEST);
                }
				
                if (rc == NGX_ERROR) {
                    ngx_http_close_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
                    return NGX_ERROR;
                }

                r->headers_in.server = host;
            }

            if (r->http_version < NGX_HTTP_VERSION_10) {
				return ngx_http_finalize_request(r, NGX_HTTP_VERSION_NOT_SUPPORTED);
            }

            if (ngx_list_init(&r->headers_in.headers, r->pool, 20,
                              sizeof(ngx_table_elt_t))
                != NGX_OK)
            {
                ngx_http_close_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
                return NGX_ERROR;
            }

			socket->read_handler = ngx_http_process_request_headers;
			return socket_read_action(socket);
        }

        if (rc != NGX_AGAIN) {
            /* there was error while a request line parsing */
            printf("Invalid request line, error: %s\n", ngx_http_client_errors[rc - NGX_HTTP_CLIENT_ERROR]);

            if (rc == NGX_HTTP_PARSE_INVALID_VERSION) {
                return ngx_http_finalize_request(r, NGX_HTTP_VERSION_NOT_SUPPORTED);
            } else {
                return ngx_http_finalize_request(r, NGX_HTTP_BAD_REQUEST);
            }
        }

        /* NGX_AGAIN: a request line parsing is still incomplete */
        if (r->header_in->pos == r->header_in->end) {

            rv = ngx_http_alloc_large_header_buffer(r, 1);

            if (rv == NGX_ERROR) {
                ngx_http_close_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
                return NGX_ERROR;
            }

            if (rv == NGX_DECLINED) {
                r->request_line.len = r->header_in->end - r->request_start;
                r->request_line.data = r->request_start;

                printf("client sent too long URI \n");
                return ngx_http_finalize_request(r, NGX_HTTP_REQUEST_URI_TOO_LARGE);
            }
        }
    }
	return rc;
}

static int
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
					return NGX_ERROR;
				}

				if (rv == NGX_DECLINED) {
					p = r->header_name_start;

					r->lingering_close = 1;

					if (p == NULL) {
						printf("client sent too large request\n");
						return ngx_http_finalize_request(r, NGX_HTTP_REQUEST_HEADER_TOO_LARGE);
					}

					printf("client sent too long header line\n");
					return ngx_http_finalize_request(r, NGX_HTTP_REQUEST_HEADER_TOO_LARGE);
				}
			}

            n = ngx_http_read_request_header(r);
            if (n == NGX_AGAIN || n == NGX_ERROR) {
                return n;
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
                return NGX_ERROR;
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
                return NGX_ERROR;
            }

            if (h->key.len == r->lowcase_index) {
                ngx_memcpy(h->lowcase_key, r->lowcase_header, h->key.len);
            } else {
                ngx_strlow(h->lowcase_key, h->key.data, h->key.len);
            }

			hh = ngx_hash_find(&hrc->headers_in_hash, h->hash,
							   h->lowcase_key, h->key.len);
							   
			if (hh && hh->handler(r, h, hh->offset) != NGX_OK) {
				return NGX_ERROR;
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
                return NGX_ERROR;
            }

			///@note set request process handlers to socket
            //ngx_http_process_request(r);
            return NGX_AGAIN;
        }

        if (rc == NGX_AGAIN) {
            /* a header line parsing is still not complete */
            continue;
        }

        /* rc == NGX_HTTP_PARSE_INVALID_HEADER */
        printf("client sent invalid header line\n");
        return ngx_http_finalize_request(r, NGX_HTTP_BAD_REQUEST);
    }
	return NGX_AGAIN;
}

static ssize_t
ngx_http_read_request_header(ngx_http_request_t *r)
{
    ssize_t                     n;
	http_connection_ctx_t*		c;

    c = (http_connection_ctx_t*) r->connection;
    n = r->header_in->last - r->header_in->pos;

    if (n > 0) {
        return n;
    }

	n = socket_read(c->socket, r->header_in->last, 
			r->header_in->end - r->header_in->last);

	//if(n == 0)
	//{
		//c->error = 1;
        //ngx_http_finalize_request(r, NGX_HTTP_BAD_REQUEST);
        //return NGX_ERROR;
	//}

	if(n == NGX_ERROR)
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

	c = (http_connection_ctx_t*) r->connection;
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

    c = (http_connection_ctx_t*) r->connection;
	s = c->socket;

    //c->read->handler = ngx_http_request_handler;
    //c->write->handler = ngx_http_request_handler;
    //r->read_event_handler = ngx_http_block_reading;

    //ngx_http_handler(r);
}

void ngx_http_finalize_request(ngx_http_request_t *r, ngx_int_t rc)
{
	http_connection_ctx_t	  *c;
	socket_ctx_t			  *sock;

    c = (http_connection_ctx_t*) r->connection;
	sock = c->sock;
	
	if(rc <= 0)
	{
		if(rc == NGX_AGAIN) {
			return rc;
		}

		if(rc == NGX_ERROR) {
			//close connection
		}
		return NGINX_OK;
	}

	r->headers_out.status = rc;

	sock->write_handler = ngx_http_write_content_handler;
	if(io_poll_ctl(sock->io, IO_EVENT_WRITE, IO_CTL_MOD, sock) == NGX_ERROR) {
		//close connection
	}
}

void
ngx_http_finalize_request(ngx_http_request_t *r, ngx_int_t rc)
{
	http_connection_ctx_t	  *c;
    ngx_http_request_t        *pr;
    //ngx_http_core_loc_conf_t  *clcf;

    c = (http_connection_ctx_t*) r->connection;

    //ngx_log_debug5(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   //"http finalize request: %i, \"%V?%V\" a:%d, c:%d",
                   //rc, &r->uri, &r->args, r == c->data, r->main->count);

    if (rc == NGX_DONE) {
        ngx_http_finalize_connection(r);
        return;
    }

    if (rc == NGX_OK && r->filter_finalize) {
        c->error = 1;
    }

    if (rc == NGX_DECLINED) {
        r->content_handler = NULL;
        r->write_event_handler = ngx_http_core_run_phases;
        ngx_http_core_run_phases(r);
        return;
    }

    if (r != r->main && r->post_subrequest) {
        rc = r->post_subrequest->handler(r, r->post_subrequest->data, rc);
    }

    if (rc == NGX_ERROR
        || rc == NGX_HTTP_REQUEST_TIME_OUT
        || rc == NGX_HTTP_CLIENT_CLOSED_REQUEST
        || c->error)
    {
        if (ngx_http_post_action(r) == NGX_OK) {
            return;
        }

        ngx_http_terminate_request(r, rc);
        return;
    }

    if (rc >= NGX_HTTP_SPECIAL_RESPONSE
        || rc == NGX_HTTP_CREATED
        || rc == NGX_HTTP_NO_CONTENT)
    {
        if (rc == NGX_HTTP_CLOSE) {
            ngx_http_terminate_request(r, rc);
            return;
        }

        if (r == r->main) {
            if (c->read->timer_set) {
                ngx_del_timer(c->read);
            }

            if (c->write->timer_set) {
                ngx_del_timer(c->write);
            }
        }

        c->read->handler = ngx_http_request_handler;
        c->write->handler = ngx_http_request_handler;

        ngx_http_finalize_request(r, ngx_http_special_response_handler(r, rc));
        return;
    }

    if (r != r->main) {
        clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

        if (r->background) {
            if (!r->logged) {
                if (clcf->log_subrequest) {
                    ngx_http_log_request(r);
                }

                r->logged = 1;

            } else {
                ngx_log_error(NGX_LOG_ALERT, c->log, 0,
                              "subrequest: \"%V?%V\" logged again",
                              &r->uri, &r->args);
            }

            r->done = 1;
            ngx_http_finalize_connection(r);
            return;
        }

        if (r->buffered || r->postponed) {

            if (ngx_http_set_write_handler(r) != NGX_OK) {
                ngx_http_terminate_request(r, 0);
            }

            return;
        }

        pr = r->parent;

        if (r == c->data) {

            r->main->count--;

            if (!r->logged) {
                if (clcf->log_subrequest) {
                    ngx_http_log_request(r);
                }

                r->logged = 1;

            } else {
                ngx_log_error(NGX_LOG_ALERT, c->log, 0,
                              "subrequest: \"%V?%V\" logged again",
                              &r->uri, &r->args);
            }

            r->done = 1;

            if (pr->postponed && pr->postponed->request == r) {
                pr->postponed = pr->postponed->next;
            }

            c->data = pr;

        } else {

            ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0,
                           "http finalize non-active request: \"%V?%V\"",
                           &r->uri, &r->args);

            r->write_event_handler = ngx_http_request_finalizer;

            if (r->waited) {
                r->done = 1;
            }
        }

        if (ngx_http_post_request(pr, NULL) != NGX_OK) {
            r->main->count++;
            ngx_http_terminate_request(r, 0);
            return;
        }

        ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0,
                       "http wake parent request: \"%V?%V\"",
                       &pr->uri, &pr->args);

        return;
    }

    if (r->buffered || c->buffered || r->postponed) {

        if (ngx_http_set_write_handler(r) != NGX_OK) {
            ngx_http_terminate_request(r, 0);
        }

        return;
    }

    if (r != c->data) {
        ngx_log_error(NGX_LOG_ALERT, c->log, 0,
                      "http finalize non-active request: \"%V?%V\"",
                      &r->uri, &r->args);
        return;
    }

    r->done = 1;

    r->read_event_handler = ngx_http_block_reading;
    r->write_event_handler = ngx_http_request_empty_handler;

    if (!r->post_action) {
        r->request_complete = 1;
    }

    if (ngx_http_post_action(r) == NGX_OK) {
        return;
    }

    if (c->read->timer_set) {
        ngx_del_timer(c->read);
    }

    if (c->write->timer_set) {
        c->write->delayed = 0;
        ngx_del_timer(c->write);
    }

    if (c->read->eof) {
        ngx_http_close_request(r, 0);
        return;
    }

    ngx_http_finalize_connection(r);
}

static int ngx_http_write_stub_handler(socket_ctx_t* socket)
{
	return NGX_AGAIN;
}

static int ngx_http_write_content_handler(socket_ctx_t* socket)
{
	return NGX_AGAIN;
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

	hc = (http_connection_ctx_t*) r->connection;
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
