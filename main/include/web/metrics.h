#ifndef METRICS_H
#define METRICS_H

#include "esp_http_server.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t metrics_register_handler(httpd_handle_t server);
esp_err_t metrics_handler(httpd_req_t *req);
char* metrics_generate_prometheus_text(void);

#ifdef __cplusplus
}
#endif

#endif
