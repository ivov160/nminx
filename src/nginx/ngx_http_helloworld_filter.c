#include <nginx/ngx_config.h>
#include <nginx/ngx_core.h>
#include <nginx/ngx_http.h>
#include <nginx/ngx_http_request.h>

#include <nminx/http_request.h>

static u_char msg[sizeof("Hello World!!")] = "Hello World!!";
static size_t msg_len = sizeof("Hello World!!") - 1;

ngx_int_t
ngx_http_helloworld_filter(ngx_http_request_t *r)
{
	ngx_buf_t				   *b;
    ngx_chain_t                out;
	ngx_int_t				   rc;
	http_connection_ctx_t	   *c;

	c = r->connection;

	r->keepalive = 0;
	r->header_only = 1;
	r->headers_out.status = NGX_HTTP_OK;
	r->headers_out.content_length_n = msg_len;

	//return ngx_http_send_header(r);
	rc = ngx_http_send_header(r);
	if(rc == NGX_ERROR) {
		return rc;
	}

    b = ngx_create_temp_buf(r->pool, msg_len);
    if (b == NULL) {
        return NGX_ERROR;
    }

    ngx_cpymem(b->last, msg, msg_len);
	b->last += msg_len;

    b->last_buf = 1;
    b->last_in_chain = 1;

    out.buf = b;
    out.next = NULL;

	return ngx_http_write_filter(r, &out);
}

