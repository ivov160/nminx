#include <nginx/ngx_config.h>
#include <nginx/ngx_core.h>

#include <nginx/ngx_http.h>
#include <nginx/ngx_palloc.h>

#include "http_request.h"

#include <nminx/io.h>
#include <nminx/socket.h>
#include <nminx/server.h>
#include <nminx/http_connection.h>

http_connection_ctx_t* http_connection_create(config_t* conf)
{
	connection_config_t* conn_conf = get_conn_conf(conf);

	ngx_pool_t* pool = ngx_create_pool(conn_conf->pool_size);
	if(!pool)
	{
		printf("Failed allocate connection memmory pool, size: %d\n", 
				conn_conf->pool_size);
		return NULL;
	}

	http_connection_ctx_t* hc = (http_connection_ctx_t*)
		ngx_pcalloc(pool, sizeof(http_connection_ctx_t));

	if(!hc)
	{
		printf("Faield allocate http_connection_ctx_t!\n");
		ngx_destroy_pool(pool);
		return NULL;
	}

	hc->lb = ngx_pcalloc(pool, sizeof(http_large_buffer_t));
	if(!hc->lb)
	{
		printf("Failed allocate http_large_buffer_t!\n");
		ngx_destroy_pool(pool);
		return NULL;
	}

	hc->conf = conf;
	hc->pool = pool;
	hc->need_close = FALSE;

	return hc;
}

int http_connection_destroy(http_connection_ctx_t* conn)
{
	// connection contained in pool
	// destory_pool clear all mammory
	if(conn->pool)
	{
		ngx_destroy_pool(conn->pool);
	}

	// now not need
	//free(conn);

	return NMINX_OK;
}

int http_listner_init_handler(config_t* conf, socket_ctx_t* socket)
{
	server_ctx_t* s_ctx = (server_ctx_t*) socket->data;
	io_ctx_t* io_ctx = s_ctx->io_ctx;

	if(http_request_init_config(conf) == NMINX_ERROR)
	{
		printf("Failed initialize http_request_config!\n");
		return NMINX_ERROR;
	}

	if(io_poll_ctl(io_ctx, IO_CTL_ADD, IO_EVENT_READ, socket) == NMINX_ERROR)
	{
		printf("Failed attach read event to socket!\n");
		return NMINX_ERROR;
	}
	socket->read_handler = http_connection_accept;

	return NMINX_OK;
}

int http_connection_accept(socket_ctx_t* l_socket)
{
	server_ctx_t* s_ctx = (server_ctx_t*) l_socket->data;
	config_t* conf = s_ctx->conf;

	socket_ctx_t* c_socket = socket_accept(l_socket);
	if(!c_socket)
	{
		printf("Failed accept client!\n");
		return NMINX_ERROR;
	}

	http_connection_ctx_t* hc = http_connection_create(conf);
	if(!hc)
	{
		printf("Failed create http_connection!\n");
		socket_close_action(c_socket);
		return NMINX_ERROR;
	}
	hc->socket = c_socket;

	c_socket->data = hc;
	c_socket->cleanup_handler = http_connection_cleanup_handler;

	c_socket->read_handler = http_connection_read_handler;
	//c_socket->write_handler = http_connection_write_handler;
	//c_socket->close_handler = http_connection_close_handler;

	if(server_add_socket(s_ctx, c_socket) == NMINX_ERROR)
	{
		printf("Failed add socket to server!\n");
		// destruction of http_connection_ctx_t wil be in socket cleanup handler
		// socket destruction wil be in close_action handler
		socket_close_action(c_socket);
		return NMINX_ERROR;
	}

	if(io_poll_ctl(c_socket->io, IO_CTL_ADD, IO_EVENT_READ, c_socket) == NMINX_ERROR)
	{
		printf("Failed attach read event to socket!\n");
		// destruction of http_connection_ctx_t wil be in socket cleanup handler
		// socket destruction wil be in close_action handler
		socket_close_action(c_socket);
		return NMINX_ERROR;
	}
	return NMINX_OK;
}

int http_connection_read_handler(socket_ctx_t* socket)
{
	http_connection_ctx_t* hc = (http_connection_ctx_t*) socket->data;
	config_t* conf = hc->conf;
	http_request_config_t* http_req_conf = get_http_req_conf(conf);

	uint32_t buf_size = http_req_conf->pool_size;
	ngx_buf_t* buf = hc->buf;

	if(buf == NULL)
	{
		buf = ngx_create_temp_buf(hc->pool, buf_size);
		if(!buf)
		{
			return NMINX_ERROR;
		}
	}
	else if (buf->start == NULL) 
	{
        buf->start = ngx_palloc(hc->pool, buf_size);
        if (buf->start == NULL) {
            return NMINX_ERROR;
        }

        buf->pos = buf->start;
        buf->last = buf->start;
        buf->end = buf->last + buf_size;
	}

	int rsize = socket_read(socket, buf->last, buf_size);
	if(rsize == 0 && errno == EAGAIN)
	{
		///@note connection timeout not started, because mtcp is control this
		///@note in nginx read event force added to event pool
		
		// from nginx if connection read return again
        if (ngx_pfree(hc->pool, buf->start) == NMINX_OK) {
            buf->start = NULL;
        }
		return NMINX_AGAIN;
	}

	if(rsize < 0)
	{
		return NMINX_ERROR;
	}

    buf->last += rsize;

	ngx_http_request_t* r = hc->request;
	if(!r)
	{
		// initialize request
		r = http_request_create(hc);
		if(!r)
		{
			printf("Failed create http_request struct!\n");
			return NMINX_ERROR;
		}
		hc->request = r;
	}

	// connection initialize done
	// next events handled as http
	if(http_request_init_events(hc) == NMINX_ERROR)
	{
		printf("Failed initilize http processing events!\n");
		return NMINX_ERROR;
	}
	return socket_read_action(socket);
}

//int http_connection_write_handler(socket_ctx_t* socket)
//{
	//return NMINX_OK;
//}

//int http_connection_close_handler(socket_ctx_t* socket)
//{
	/////@todo need check control flags
	////http_connection_ctx_t* hc = (http_connection_ctx_t*) socket->data;
	////http_connection_destroy(hc);

	//socket_destroy(socket);
	//return NMINX_OK;
//}

int http_connection_cleanup_handler(void* data)
{
	http_connection_ctx_t* hc = (http_connection_ctx_t*) data;

	if(hc->request != NULL)
	{
		http_request_destroy(hc->request);
		hc->request = NULL;
	}

	http_connection_destroy(hc);

	return NMINX_OK;
}

