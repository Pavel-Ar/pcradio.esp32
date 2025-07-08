#ifndef API_H
#define API_H

#include <esp_http_server.h>

// Запуск HTTP API сервера (создает задачу на 0 ядре)
esp_err_t api_server_start(void);

// Остановка HTTP API сервера
void api_server_stop(void);

#endif // API_H
