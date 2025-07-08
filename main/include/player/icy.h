#ifndef ICY_H
#define ICY_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "esp_http_client.h"
#include "freertos/semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ICY_NAME_MAX_LEN 128
#define ICY_GENRE_MAX_LEN 128
#define ICY_URL_MAX_LEN 256
#define ICY_METAINT_RESPONSE_HEADER "icy-metaint"
#define ICY_MAX_META_SIZE 4080

typedef struct {
    int metaint_interval;       // Interval for metadata
    int bytes_until_meta;       // Bytes left until next metadata block
    size_t buffer_capacity;     // Capacity of metadata buffer
    uint8_t *meta_buffer;       // Buffer for metadata
    char *icy_name;             // Stream title
    char *icy_genre;            // Stream genre
    char *icy_url;              // Stream URL
} icy_state_t;

void icy_state_init(icy_state_t *state);
void icy_state_cleanup(icy_state_t *state);
void icy_handle_header(icy_state_t *state, const char *key, const char *value);
void icy_allocate_buffer(icy_state_t *state);
esp_err_t icy_process_metadata(icy_state_t *state, esp_http_client_handle_t client, SemaphoreHandle_t stop_sem, bool *stream_end);
const char *icy_get_name(const icy_state_t *state);

#ifdef __cplusplus
}
#endif

#endif
