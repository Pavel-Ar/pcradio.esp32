#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "esp_vfs.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_littlefs.h"
#include "www.h"

static const char *TAG = "WWW";

#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + 128)
#define SCRATCH_BUFSIZE (10240)
#define BASE_PATH "/storage/www"

static esp_err_t www_get_handler(httpd_req_t *req);
static esp_err_t www_404_handler(httpd_req_t *req, httpd_err_code_t err);

static const httpd_uri_t static_handlers[] = {
    { .uri = "/",          .method = HTTP_GET, .handler = www_get_handler, .user_ctx = NULL },
    { .uri = "/index.html", .method = HTTP_GET, .handler = www_get_handler, .user_ctx = NULL },
    { .uri = "/favicon.ico", .method = HTTP_GET, .handler = www_get_handler, .user_ctx = NULL },
    { .uri = "/css/*",      .method = HTTP_GET, .handler = www_get_handler, .user_ctx = NULL },
    { .uri = "/js/*",       .method = HTTP_GET, .handler = www_get_handler, .user_ctx = NULL },
    { .uri = "/images/*",   .method = HTTP_GET, .handler = www_get_handler, .user_ctx = NULL },
    { .uri = "/*",          .method = HTTP_GET, .handler = www_get_handler, .user_ctx = NULL }
};
#define NUM_STATIC_HANDLERS (sizeof(static_handlers) / sizeof(static_handlers[0]))

typedef struct {
    char *buf;
    size_t len;
} httpd_send_user_ctx_t;

esp_err_t www_server_register_handlers(httpd_handle_t server) {
    if (server == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    for (size_t i = 0; i < NUM_STATIC_HANDLERS; ++i) {
        esp_err_t ret = httpd_register_uri_handler(server, &static_handlers[i]);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register static handler '%s': %s", static_handlers[i].uri, esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "Registered static handler '%s'", static_handlers[i].uri);
        }
    }

    esp_err_t err_ret = httpd_register_err_handler(server,
                                                    HTTPD_404_NOT_FOUND,
                                                    www_404_handler);
    if (err_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register 404 error handler: %s", esp_err_to_name(err_ret));
    } else {
        ESP_LOGI(TAG, "Registered 404 error handler for static files");
    }

    return ESP_OK;
}

static const char* get_content_type(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) {
        return "application/octet-stream";
    }
    if (strcmp(ext, ".html") == 0) return "text/html";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".js") == 0) return "application/javascript";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".jpg") == 0) return "image/jpeg";
    if (strcmp(ext, ".ico") == 0) return "image/x-icon";
    return "application/octet-stream";
}

static esp_err_t www_get_handler(httpd_req_t *req) {
    char filepath[FILE_PATH_MAX];
    struct stat file_stat;

    ESP_LOGW(TAG, "www_get_handler invoked for URI: '%s'", req->uri);

    strlcpy(filepath, BASE_PATH, sizeof(filepath));
    if (strcmp(req->uri, "/") == 0) {
        strlcat(filepath, "/index.html", sizeof(filepath));
    } else {
        strlcat(filepath, req->uri, sizeof(filepath));
    }

    if (stat(filepath, &file_stat) == -1) {
        ESP_LOGE(TAG, "File not found: %s", filepath);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    int fd = open(filepath, O_RDONLY);
    if (fd == -1) {
        ESP_LOGE(TAG, "Failed to open file: %s", filepath);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, get_content_type(filepath));

    char *chunk = malloc(SCRATCH_BUFSIZE);
    if (!chunk) {
        ESP_LOGE(TAG, "Failed to allocate memory for chunk");
        close(fd);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    ssize_t read_bytes;
    do {
        read_bytes = read(fd, chunk, SCRATCH_BUFSIZE);
        if (read_bytes > 0) {
            if (httpd_resp_send_chunk(req, chunk, read_bytes) != ESP_OK) {
                close(fd);
                free(chunk);
                ESP_LOGE(TAG, "File sending failed!");
                return ESP_FAIL;
            }
        }
    } while (read_bytes > 0);

    close(fd);
    free(chunk);

    httpd_resp_send_chunk(req, NULL, 0);
    ESP_LOGI(TAG, "File sent successfully: %s", filepath);
    return ESP_OK;
}

static esp_err_t www_404_handler(httpd_req_t *req, httpd_err_code_t err) {
    if (req->method == HTTP_GET) {
        return www_get_handler(req);
    }
    return ESP_FAIL;
}
