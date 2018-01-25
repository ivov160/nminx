#ifndef _NMINX_SERVER_H
#define _NMINX_SERVER_H

#include <nminx/nminx.h>
#include <nminx/config.h>

/**
 * @defgroup server Server 
 * @brief Управление сервером
 * 
 * @addtogroup server
 * @{
 */

/**
 * @brief Порт по умолчанию для входящих соеденений
 */
#define SERVER_DEFAULT_PORT 80

/**
 * @brief Контекст сервера
 */
struct server_ctx_s
{
	config_t* conf;								///< Конфиг
	io_ctx_t* io_ctx;							///< Контекст ввода-вывода
	socket_ctx_t* sockets[MAX_CONNECTIONS];		///< слоты соеденений

	int is_run;									///< флаг состояния 1 - сервер работает, 0 - остановлен
	uint32_t free_slots;						///< кол-во свободных слотов
};

server_ctx_t* server_init(config_t* conf);
void server_destroy(server_ctx_t* srv);

int server_process_events(server_ctx_t* s_cfg);

int server_add_socket(server_ctx_t* s_ctx, socket_ctx_t* sock);
void server_rm_socket(server_ctx_t* s_ctx, socket_ctx_t* sock);

/**
 *@}
 */

#endif //_NMINX_SERVER_H
