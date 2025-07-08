#ifndef WWW_H
#define WWW_H

#include <esp_http_server.h>
#include <esp_err.h>

/**
 * @brief Register handlers for serving the web interface from LittleFS.
 *
 * This function registers a generic GET / * handler to serve files
 * from the "/storage/www" directory.
 *
 * @param server The HTTP server handle.
 * @return
 *  - ESP_OK: on success
 *  - ESP_ERR_INVALID_ARG: if server handle is null
 *  - Other esp_err_t codes from httpd_register_uri_handler on failure.
 */
esp_err_t www_server_register_handlers(httpd_handle_t server);

#endif // WWW_H
