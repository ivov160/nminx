#include <nminx/server.h>


server_config_t* server_init(nminx_cfg* cfg)
{
	server_config_t* s_cfg = 
		(server_config_t*) calloc(1, sizeof(server_config_t));
	
	if(!s_cfg)
	{
		printf("Failed allocation server_config_t\n");
		return s_cfg;
	}
	s_cfg->m_cfg = m_cfg;

	//mtcp_core_affinitize(core);
	
	// create mtcp context: this will spawn an mtcp thread 
	s_cfg->mctx = mtcp_create_context(cfg->mtcp_cpu);
	if (!s_cfg->mctx) 
	{
		printf("Failed to create mtcp context!\n");
		free(s_cfg);
		return NULL;
	}

	// create epoll descriptor 
	s_cfg->ep = mtcp_epoll_create(s_cfg->mctx, m_cfg->mtcp_max_events);
	if (s_cfg->ep < 0) 
	{
		printf("Failed to create epoll descriptor!\n");
		mtcp_destroy_context(s_cfg->mctx);
		free(s_cfg);
		return NULL;
	}

	//ctx->connections = (struct connection_ctx*) calloc(MAX_FLOW_NUM, sizeof(struct connection_ctx));
	//if(!ctx->connections)
	//{
		//mtcp_destroy_context(ctx->mctx);
		//free(ctx);
		//printf("Failed allocation memmory for connections pool!\n");
		//return NULL;
	//}
	return s_cfg;
}

int server_destroy(server_config_t* s_cfg)
{
	if(s_cfg)
	{
		//if(ctx->connections)
		//{	/// @todo need checking for not closed connections
			///// or loop with force connection_close
			//free(ctx->connections);
		//}

		//if(ctx->socket >= 0)
		//{
			//mtcp_epoll_ctl(s_cfg->mctx, s_cfg->ep, MTCP_EPOLL_CTL_DEL, s_cfg->socket, NULL);
			////mtcp_close(ctx->mctx, ctx->socket);
		//}

		if(s_cfg->mctx)
		{
			mtcp_destroy_context(s_cfg->mctx);
		}
		free(s_cfg);
	}
	}
	return NMINX_OK;
}

int server_process_events(server_config_t* cfg)
{
	return NMINX_OK;
}
