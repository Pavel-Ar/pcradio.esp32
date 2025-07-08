#ifndef WRAPPER_H
#define WRAPPER_H

#include "esp_err.h"
#include "esp_audio_dec.h"
#include <stddef.h>
#include <stdint.h>

typedef enum {
    WRAPPER_TYPE_NONE = 0,
    WRAPPER_TYPE_AAC  = 1,
    WRAPPER_TYPE_MP3  = 3,
} wrapper_audio_type_t;

wrapper_audio_type_t wrapper_detect_audio_type_from_data(const uint8_t* data, size_t len);

wrapper_audio_type_t wrapper_detect_audio_type_from_url(const char *url);

esp_err_t wrapper_open_detected_decoder(wrapper_audio_type_t type, esp_audio_dec_handle_t *handle_out);

void wrapper_dec_close(esp_audio_dec_handle_t handle);

#endif
