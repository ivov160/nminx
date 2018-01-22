#ifndef _NMINX_H
#define _NMINX_H

#include <nginx/ngx_config.h>
#include <nginx/ngx_core.h>

#include <nminx/config.h>

#define  NMINX_OK         NGX_OK
#define  NMINX_ERROR      NGX_ERROR
#define  NMINX_AGAIN      NGX_AGAIN
#define  NMINX_BUSY       NGX_BUSY
#define  NMINX_DONE       NGX_DONE
#define  NMINX_DECLINED   NGX_DECLINED
#define  NMINX_ABORT      NGX_ABORT

#define TRUE 1
#define FALSE 0

#define MAX_CONNECTIONS 10000

#define UNDERSCORES_IN_HEADERS (1 << 1)
#define IGNORE_INVALID_HEADERS (1 << 2)
#define URI_MERGE_SLASHES (1 << 3)

#endif	//_NMINX_H

