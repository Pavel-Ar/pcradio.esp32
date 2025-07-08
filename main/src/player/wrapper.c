#include "wrapper.h"
#include "esp_log.h"
#include <string.h>

#include "codec_mp3.h"
#include "codec_aac.h"

static const char *TAG = "WRAPPER";

esp_err_t wrapper_open_detected_decoder(wrapper_audio_type_t type, esp_audio_dec_handle_t *handle_out) {
    if (handle_out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *handle_out = NULL;

    ESP_LOGI(TAG, "Attempting to open decoder for detected type: %d", (int)type);

    switch (type) {
        case WRAPPER_TYPE_MP3:
            ESP_LOGI(TAG, "Opening MP3 decoder via specific wrapper function.");
            return codec_open_mp3_decoder(handle_out);
        case WRAPPER_TYPE_AAC:
            ESP_LOGI(TAG, "Opening AAC decoder via specific wrapper function for type %d.", (int)type);
            return codec_open_aac_decoder(handle_out);
        default:
            ESP_LOGE(TAG, "Unsupported audio type for opening decoder: %d", (int)type);
            return ESP_ERR_NOT_SUPPORTED;
    }
}

wrapper_audio_type_t wrapper_detect_audio_type_from_data(const uint8_t* data, size_t len) {
    if (!data || len < 4) {
        return WRAPPER_TYPE_NONE;
    }

    if (data[0] == 0xFF) {
        uint8_t second_byte = data[1];

        if (((second_byte & 0xF0) == 0xF0 || (second_byte & 0xF0) == 0xE0) &&
            ((second_byte & 0x06) == 0x02)) {
            ESP_LOGI(TAG, "Detected MP3 by header bytes: %02X %02X %02X %02X",
                    data[0], data[1], data[2], data[3]);
            return WRAPPER_TYPE_MP3;
        }

        if (second_byte == 0xFB || second_byte == 0xFA ||
            second_byte == 0xF3 || second_byte == 0xF2 ||
            second_byte == 0xE3 || second_byte == 0xE2) {
            ESP_LOGI(TAG, "Detected MP3 by specific header pattern: %02X %02X %02X %02X",
                    data[0], data[1], data[2], data[3]);
            return WRAPPER_TYPE_MP3;
        }

        if (second_byte == 0xF1 || second_byte == 0xF9) {
            ESP_LOGI(TAG, "Detected AAC by header bytes: %02X %02X %02X %02X",
                    data[0], data[1], data[2], data[3]);
            return WRAPPER_TYPE_AAC;
        }
    }

    ESP_LOGW(TAG, "Could not identify audio format from header: %02X %02X %02X %02X. Returning NONE.",
             data[0], data[1], data[2], data[3]);
    return WRAPPER_TYPE_NONE;
}

wrapper_audio_type_t wrapper_detect_audio_type_from_url(const char *url) {
    if (!url) {
        return WRAPPER_TYPE_NONE;
    }

    const char *extension = strrchr(url, '.');
    if (extension) {
        extension++;
        if (strcasecmp(extension, "mp3") == 0) {
            return WRAPPER_TYPE_MP3;
        } else if (strcasecmp(extension, "aac") == 0) {
            return WRAPPER_TYPE_AAC;
        }
    }

    if (strstr(url, "aac") != NULL) {
        return WRAPPER_TYPE_AAC;
    } else if (strstr(url, "mp3") != NULL) {
        return WRAPPER_TYPE_MP3;
    }

    ESP_LOGW(TAG, "Could not determine audio type from URL: %s. Returning NONE.", url);
    return WRAPPER_TYPE_NONE;
}

void wrapper_dec_close(esp_audio_dec_handle_t handle) {
    if (handle) {
        esp_audio_dec_close(handle);
    }
}
