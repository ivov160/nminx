#include <nminx/http_connection.h>

#include <nginx/ngx_config.h>
#include <nginx/ngx_core.h>
#include <nginx/ngx_http.h>


//#include <ngx_config.h>
//#include <ngx_core.h>

//#include <ngx_buf.h>
//#include <ngx_http.h>
//#include <ngx_http_request.h>

struct http_connection_ctx_s
{
	ngx_http_request_t* request;
};


http_connection_ctx_t* create_http_connection(nminx_config_t* m_cfg)
{
	http_connection_ctx_t* hc = (http_connection_ctx_t*)
		calloc(1, sizeof(http_connection_ctx_t));

	if(!hc)
	{
		printf("Faield allocate http_connection_ctx_t!\n");
		return NULL;
	}

	return hc;
}

int destroy_http_connection(http_connection_ctx_t* conn)
{
	free(conn);
}

int http_connection_read_handler(socket_ctx_t* socket)
{
}

int http_connection_write_handler(socket_ctx_t* socket)
{
}

int http_connection_close_handler(socket_ctx_t* socket)
{
	http_connection_ctx_t* hc = (http_connection_ctx_t*) socket->data;
	destroy_http_connection(hc);
	socket_close(socket);
}
