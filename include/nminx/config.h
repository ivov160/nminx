#ifndef __H_NMINX_CONFIG_H
#define __H_NMINX_CONFIG_H

#include <nminx/nminx.h>

/**
 * @defgroup config Config 
 * @brief Конфиги приложения
 *
 * @addtogroup config
 * @{
 */

typedef struct config_s config_t;
typedef struct mtcp_config_s mtcp_config_t;
typedef struct watchdog_config_s watchdog_config_t;
typedef struct server_config_s server_config_t;
typedef struct connection_config_s connection_config_t;
typedef struct http_request_config_s http_request_config_t;

/**
 * @brief Кол-во слотов для хранения конфигов
 */
#define CONFIG_SLOTS_COUNT 8

#define CONFIG_SLOT_1 0
#define CONFIG_SLOT_2 1
#define CONFIG_SLOT_3 2
#define CONFIG_SLOT_4 3
#define CONFIG_SLOT_5 4
#define CONFIG_SLOT_6 5
#define CONFIG_SLOT_7 6
#define CONFIG_SLOT_8 7

/**
 * @brief Макрос для получения конфига системы io
 */
#define get_io_conf(mc) (mtcp_config_t*) mc->slots[CONFIG_SLOT_1]

/**
 * @brief Макрос для получения конфига мастер процесса
 */
#define get_wdt_conf(mc) (watchdog_config_t*) mc->slots[CONFIG_SLOT_2]

/**
 * @brief Макрос для получения конфига сервера
 */
#define get_serv_conf(mc) (server_config_t*) mc->slots[CONFIG_SLOT_3]

/**
 * @brief Макрос для получения конфига подключения
 */
#define get_conn_conf(mc) (connection_config_t*) mc->slots[CONFIG_SLOT_4]

/**
 * @brief Макрос для получения конфига http запроса
 */
#define get_http_req_conf(mc) (http_request_config_t*) mc->slots[CONFIG_SLOT_5]

/**
 * @brief Макрос для установки конфига io 
 */
#define set_io_conf(mc, conf) mc->slots[CONFIG_SLOT_1] = (uintptr_t) conf

/**
 * @brief Макрос для установки конфига мастер процесса
 */
#define set_wdt_conf(mc, conf) mc->slots[CONFIG_SLOT_2] = (uintptr_t) conf

/**
 * @brief Макрос для установки конфига сервера
 */
#define set_serv_conf(mc, conf) mc->slots[CONFIG_SLOT_3] = (uintptr_t) conf

/**
 * @brief Макрос для установки конфига подключения
 */
#define set_conn_conf(mc, conf) mc->slots[CONFIG_SLOT_4] = (uintptr_t) conf

/**
 * @brief Макрос для установки конфига запроса
 */
#define set_http_req_conf(mc, conf) mc->slots[CONFIG_SLOT_5] = (uintptr_t) conf

/**
 * @brief Конфиг приложения
 * Является контейнером содержащим указатели на отдельные конфиги
 * Каждый конфиг ложится в свой слот. И доступ к этому слоту покрывется макросом
 */
struct config_s
{
	uintptr_t slots[CONFIG_SLOTS_COUNT];		///< Слоты для конфигов @see CONFIG_SLOTS_COUNT 

	// pool for application
	ngx_pool_t* pool;							///< Пул для выделения памяти, доступен до окончания жизни приложения

	// temporary pool for iteration
	// after iteration pool reseted pointers is not valid
	ngx_pool_t* temp_pool;						///< Временный пул, используется во время инициализации приложения, 
												///< и скидывается при каждой итерации
};

/**
 * @brief Конфиг для инициализации mTcp
 */
struct mtcp_config_s
{
	int cpu;				///< Номер ядра к которому будет прикриплен master mTcp
	int max_events;			///< Максимальное кол-во отслеживаемых дескрипторов
	char* config_path;		///< Путь до конфига mTcp
};

/**
 * @brief Конфиг для сетевых подключений
 */
struct connection_config_s
{
	uint32_t pool_size;			///< Указывает размер пула памяти для алокации данных используемых в сетевом соеденении
	uint32_t buffer_size;		///< Размер буфера для приема запроса
};

/**
 * @brief Конфиг для http запроса
 */
struct http_request_config_s
{
	uint32_t pool_size;					///< Размер пула памяти используещегося для алокации данных используемых в запросе
	uint32_t large_buffer_chunk_size;	///< Количество частей для приема большого запроса (connection_config_s.buffer_size < total_requst_size)
	uint32_t large_buffer_chunk_count;	///< Размер блока для приеми большого запроса, всего будет выделенно large_buffer_chunk_size * large_buffer_chunk_count
	
	uint32_t headers_flags;				///< Фалаги для указания обработки заголовков

	ngx_hash_t headers_in_hash;			///< Хеш содержащий обработчики для процессинга принятых заголовков
};

/**
 * @brief Конфиг для управление мастер процессом
 */
struct watchdog_config_s
{
	uint32_t poll_timeout;		///< Таймаут в ms, после опроса дочерних процессов

	char* work_dir;				///< Путь до рабочегно каталога
	char* pid_file;				///< Путь до файла содержащего pid мастер процесса

	int silent;					///< Флаг для подавления stdout, stderr
};

typedef int (*listner_init_hanler_t)(config_t*, socket_ctx_t*);

/**
 * @brief Конфиг для сервера
 */
struct server_config_s
{
	in_addr_t ip;									///< Ip для входящих соеденений
	in_port_t port;									///< Порт для входящих соеденений
	uint32_t backlog;								///< Размер очереди для входящих соеденений

	listner_init_hanler_t listener_init_handler;	///< Хандлер для инициализации обработчиков принимающего сокета
};

/**
 * @}
 */

#endif //__H_NMINX_CONFIG_H
