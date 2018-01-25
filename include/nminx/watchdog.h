#ifndef _NMINX_WATCHDOG_H
#define _NMINX_WATCHDOG_H

#include <nminx/nminx.h>

/**
 * @defgroup watchdog Watchdog 
 * @brief Управление дочерними процессами
 *
 * @addtogroup watchdog
 * @{
 */

/**
 * @brief Структура отражающая состояние процесса
 */
struct process_state_s
{
	int is_exit;		///< процесс завершился 
	int is_signal;		///< процесс завершился по сигналу
	int is_stoped;		///< процесс был остановлен по сигналу

	int status;			///< значение (в случае сигнала - номер сигнала, иначе статус завершения)

	pid_t pid;			///< pid процесса
};

typedef int (*watchdog_handler)(process_state_t*, void* data);

/**
 * @brief Конфигурация запускаемогопроцесса
 */
struct watchdog_process_ctx_s
{
	pid_t pid;						///< pid процесса
	process_ctx_t* process;			///< запускаемый процесс

	watchdog_handler handler;		///< хандлер для обработки статуса
	void* data;						///< данный передаваемый в хандлер

};

/**
 * @brief Пулл процессов
 */
struct watchdog_pool_ctx_s
{
	watchdog_process_ctx_t* pool;		///< пул отслеживаемых процессов
	uint32_t size;						///< размер пула
};

int watchdog_start(watchdog_pool_ctx_t* wdt_pool);
int watchdog_stop();

int watchdog_signal_all(int sig);
int watchdog_signal_one(int sig, pid_t pid);

int watchdog_exec(uint32_t ms_timeout);
int watchdog_poll(process_state_t* pstate);

/**
 * @}
 */


#endif //_NMINX_WATCHDOG_H
