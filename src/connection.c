#include <nminx/connection.h>
#include <nminx/socket.h>
#include <nminx/io.h>

#include <errno.h>

#define MAX(a, b) 			((a)>(b)?(a):(b))
#define MIN(a, b) 			((a)<(b)?(a):(b))

int connection_read_handler(socket_ctx_t* socket)
{
	int size = 0;
	connection_ctx_t* conn_ctx = (connection_ctx_t*) socket->data;

	if(conn_ctx->flushed)
	{
		uint32_t read_size = MIN(RCVBUF_SIZE, CBUF_SIZE);

		size = socket_read(socket, conn_ctx->buffer, read_size);
		if(size == 0)
		{
			if(errno != EAGAIN)
			{
				return NMINX_ERROR;
			}
			return NMINX_AGAIN;
		}

		conn_ctx->flushed = 0;
		conn_ctx->size += size;

		io_poll_ctl(socket->io, IO_CTL_MOD, IO_EVENT_WRITE, socket);

		///@todo add error handling
		//if(io_poll_ctl(io_ctx, IO_CTL_MOD, IO_EVENT_WRITE, socket) == NMINX_ERROR)
		//{
			//printf("Failed attach read event to socket!\n");
			//free(conn);
			//socket_close_action(c_socket);
			//socket_destroy(c_socket);
			//return NMINX_ERROR;
		//}
	}
	return size;
}

int connection_write_handler(socket_ctx_t* socket)
{
	uint32_t sent = 0;
	connection_ctx_t* conn_ctx = (connection_ctx_t*) socket->data;
	if(!conn_ctx->flushed)
	{
		uint32_t w_size = MIN(SNDBUF_SIZE, (conn_ctx->size <= conn_ctx->offset ? 0 : conn_ctx->size - conn_ctx->offset));
		if(w_size > 0) 
		{
			sent = socket_write(socket, conn_ctx->buffer + conn_ctx->offset, w_size);
			if (sent < 0) 
			{
				if(errno != EAGAIN)
				{
					return NMINX_ERROR;
				}
				return NMINX_AGAIN;
			}

			printf("Socket %d: mtcp_write try: %d, ret: %d\n", socket->fd, w_size, sent);
			conn_ctx->offset += sent;
		}
		else
		{	//nothing to write, buffer sended to client, disable EPOLLOUT event
			memset(conn_ctx, 0, sizeof(connection_ctx_t));
			conn_ctx->flushed = 1;

			io_poll_ctl(socket->io, IO_CTL_MOD, IO_EVENT_READ, socket);
		}
	}
	return sent;
}

int connection_close_handler(socket_ctx_t* socket)
{
	connection_ctx_t* conn_ctx = (connection_ctx_t*) socket->data;
	if(conn_ctx)
	{
		free(conn_ctx);
	}

	socket_close(socket);
}
