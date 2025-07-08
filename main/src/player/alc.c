#include "alc.h"
#include "esp_log.h"
#include <stdint.h>

#define DEFAULT_ALC_GAIN_DB -11

static const char *TAG = "ALC";

static void *alc_handle = NULL;
static esp_ae_alc_cfg_t current_alc_config;

esp_err_t alc_init(const alc_config_t *config) {
    if (alc_handle != NULL) {
        ESP_LOGW(TAG, "ALC already initialized. Call alc_deinit() first.");
        return ESP_ERR_INVALID_STATE;
    }
    if (config == NULL) {
        ESP_LOGE(TAG, "ALC configuration cannot be NULL.");
        return ESP_ERR_INVALID_ARG;
    }

    current_alc_config.sample_rate = config->sample_rate;
    current_alc_config.channel = config->channels;
    current_alc_config.bits_per_sample = config->bits_per_sample;

    esp_err_t ret = esp_ae_alc_open(&current_alc_config, &alc_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error initializing ALC: %s", esp_err_to_name(ret));
        alc_handle = NULL;
        return ret;
    }

    ESP_LOGI(TAG, "ALC initialized: %d Hz, %d channel(s), %d bits",
             config->sample_rate, config->channels, config->bits_per_sample);

    for (uint8_t i = 0; i < current_alc_config.channel; i++) {
        esp_err_t gain_ret = esp_ae_alc_set_gain(alc_handle, i, DEFAULT_ALC_GAIN_DB);
        if (gain_ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set default ALC gain (%d dB) for channel %d: %s",
                     DEFAULT_ALC_GAIN_DB, i, esp_err_to_name(gain_ret));
        } else {
            ESP_LOGI(TAG, "Default ALC gain set to %d dB for channel %d", DEFAULT_ALC_GAIN_DB, i);
        }
    }

    return ESP_OK;
}

esp_err_t alc_process(void *input_buffer, void *output_buffer, int num_samples) {
    if (alc_handle == NULL) {
        ESP_LOGE(TAG, "ALC not initialized.");
        return ESP_ERR_INVALID_STATE;
    }
    if (input_buffer == NULL || output_buffer == NULL) {
        ESP_LOGE(TAG, "Input or output buffer cannot be NULL.");
        return ESP_ERR_INVALID_ARG;
    }
    if (num_samples <= 0) {
        ESP_LOGE(TAG, "Number of samples must be positive.");
        return ESP_ERR_INVALID_ARG;
    }

    return esp_ae_alc_process(alc_handle, num_samples, input_buffer, output_buffer);
}

esp_err_t alc_set_gain_db(int channel_index, int gain_db) {
    if (alc_handle == NULL) {
        ESP_LOGE(TAG, "ALC not initialized.");
        return ESP_ERR_INVALID_STATE;
    }
    if (channel_index < 0 || channel_index >= current_alc_config.channel) {
        ESP_LOGE(TAG, "Invalid channel index: %d. Available channels: %d", channel_index, current_alc_config.channel);
        return ESP_ERR_INVALID_ARG;
    }
    if (gain_db < -64 || gain_db > 63) {
        ESP_LOGE(TAG, "Gain %d dB is out of the allowed range [-64, 63].", gain_db);
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Setting gain for channel %d: %d dB", channel_index, gain_db);
    return esp_ae_alc_set_gain(alc_handle, (uint8_t)channel_index, (int8_t)gain_db);
}

esp_err_t alc_get_gain_db(int channel_index, int8_t *gain_db) {
    if (alc_handle == NULL) {
        ESP_LOGE(TAG, "ALC not initialized.");
        return ESP_ERR_INVALID_STATE;
    }
    if (channel_index < 0 || channel_index >= current_alc_config.channel) {
        ESP_LOGE(TAG, "Invalid channel index: %d. Available channels: %d", channel_index, (int)current_alc_config.channel);
        return ESP_ERR_INVALID_ARG;
    }
    if (gain_db == NULL) {
        ESP_LOGE(TAG, "Pointer to store gain cannot be NULL.");
        return ESP_ERR_INVALID_ARG;
    }
    return esp_ae_alc_get_gain(alc_handle, (uint8_t)channel_index, gain_db);
}

esp_err_t alc_deinit(void) {
    if (alc_handle != NULL) {
        esp_ae_alc_close(alc_handle);
        alc_handle = NULL;
        ESP_LOGI(TAG, "ALC deinitialized.");
        return ESP_OK;
    }
    ESP_LOGI(TAG, "ALC was not initialized.");
    return ESP_OK;
}
