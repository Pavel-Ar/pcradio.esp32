#include <esp_http_client.h>
#include <esp_http_server.h>
#include <esp_log.h>
#include <esp_heap_caps.h>
#include <string.h>
#include "api.h"
#include "playlist.h"
#include "player.h"
#include "audio_i2s.h"
#include "volume.h"
#include "eq.h"
#include "playlist.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "config.h"
#include "www.h"

#define TAG "API"

static httpd_handle_t server = NULL;

static esp_err_t send_json_error(httpd_req_t *req, const char *http_status, const char *message) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, http_status);
    char resp_str[256];
    snprintf(resp_str, sizeof(resp_str), "{\"status\":\"error\",\"message\":\"%s\"}", message);
    return httpd_resp_send(req, resp_str, strlen(resp_str));
}

static esp_err_t api_channel_handler(httpd_req_t *req) {
    char content[100];
    int ret = httpd_req_recv(req, content, sizeof(content) - 1);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            send_json_error(req, "408 Request Timeout", "Request timed out");
        }
        return ESP_FAIL;
    }
    content[ret] = '\0';

    for (int i = 0; i < ret; i++) {
        if (content[i] < '0' || content[i] > '9') {
            send_json_error(req, "400 Bad Request", "Invalid input, only digits are allowed");
            return ESP_FAIL;
        }
    }

    ESP_LOGI(TAG, "Received digits: %s", content);
    httpd_resp_set_type(req, "application/json");

    int channel_to_play = atoi(content);

    if (playlist_get_channel_count() >= channel_to_play && channel_to_play > 0) {
        player_play_channel(channel_to_play);
        config_set_player_channel(channel_to_play);
        ESP_LOGI(TAG, "Switched to channel: %d", channel_to_play);
        const char *icy_name = "";
        for (int i = 0; i < 20; ++i) {
            icy_name = player_get_icy_name();
            if (icy_name && icy_name[0] != '\0') break;
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        int len = snprintf(NULL, 0, "{\"status\":\"success\",\"channel\":%d,\"icy_name\":\"%s\"}", channel_to_play, icy_name) + 1;
        char *psram_resp = (char *)heap_caps_malloc(len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!psram_resp) {
            ESP_LOGE(TAG, "Failed to allocate response buffer");
            send_json_error(req, "500 Internal Server Error", "Failed to allocate response buffer");
            return ESP_FAIL;
        }
        snprintf(psram_resp, len, "{\"status\":\"success\",\"channel\":%d,\"icy_name\":\"%s\"}", channel_to_play, icy_name);
        char len_str[16];
        snprintf(len_str, sizeof(len_str), "%d", len - 1);
        httpd_resp_set_hdr(req, "Content-Length", len_str);
        esp_err_t res = httpd_resp_send(req, psram_resp, len - 1);
        heap_caps_free(psram_resp);
        return res;
    } else {
        ESP_LOGE(TAG, "Invalid channel number: %d", channel_to_play);
        return send_json_error(req, "400 Bad Request", "Invalid channel number");
    }
}

static esp_err_t api_volume_handler(httpd_req_t *req) {
    char content[10];
    int ret = httpd_req_recv(req, content, sizeof(content) - 1);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            send_json_error(req, "408 Request Timeout", "Request timed out");
        }
        return ESP_FAIL;
    }
    content[ret] = '\0';

    for (int i = 0; i < ret; i++) {
        if (content[i] < '0' || content[i] > '9') {
            send_json_error(req, "400 Bad Request", "Invalid input, only digits are allowed");
            return ESP_FAIL;
        }
    }

    int volume = atoi(content);
    if (volume < 0 || volume > 100) {
        send_json_error(req, "400 Bad Request", "Volume must be between 0 and 100");
        return ESP_FAIL;
    }

    if (audio_i2s_set_volume(volume) == ESP_OK) {
        config_set_player_volume(volume);
        ESP_LOGI(TAG, "Volume set to %d%%", volume);
        char resp_str[64];
        snprintf(resp_str, sizeof(resp_str), "{\"status\":\"success\",\"volume\":%d}", volume);
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
    } else {
        send_json_error(req, "500 Internal Server Error", "Failed to set volume");
        return ESP_FAIL;
    }
}

static esp_err_t api_mute_handler(httpd_req_t *req) {
    char content[10];
    int ret = httpd_req_recv(req, content, sizeof(content) - 1);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            send_json_error(req, "408 Request Timeout", "Request timed out");
        }
        return ESP_FAIL;
    }
    content[ret] = '\0';

    bool mute = false;
    if (strcmp(content, "true") == 0 || strcmp(content, "1") == 0) {
        mute = true;
    } else if (strcmp(content, "false") == 0 || strcmp(content, "0") == 0) {
        mute = false;
    } else {
        send_json_error(req, "400 Bad Request", "Invalid input, use 'true', 'false', '1', or '0'");
        return ESP_FAIL;
    }

    if (audio_i2s_set_mute(mute) == ESP_OK) {
        config_set_player_mute(mute);
        ESP_LOGI(TAG, "Mute set to %s", mute ? "true" : "false");
        char resp_str[64];
        snprintf(resp_str, sizeof(resp_str), "{\"status\":\"success\",\"mute\":%s}", mute ? "true" : "false");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
    } else {
        send_json_error(req, "500 Internal Server Error", "Failed to set mute");
        return ESP_FAIL;
    }
}

static esp_err_t api_eq_handler(httpd_req_t *req) {
    char content[10];
    int ret = httpd_req_recv(req, content, sizeof(content) - 1);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            send_json_error(req, "408 Request Timeout", "Request timed out");
        }
        return ESP_FAIL;
    }
    content[ret] = '\0';

    for (int i = 0; i < ret; i++) {
        if (content[i] < '0' || content[i] > '9') {
            send_json_error(req, "400 Bad Request", "Invalid input, only digits are allowed");
            return ESP_FAIL;
        }
    }

    int preset_index = atoi(content);
    if (preset_index < 0 || preset_index >= EQ_PRESET_COUNT) {
        send_json_error(req, "400 Bad Request", "Invalid EQ preset index");
        return ESP_FAIL;
    }

    uint32_t sample_rate = audio_i2s_get_sample_rate();
    uint8_t channels = audio_i2s_get_channels();
    uint8_t bits_per_sample = audio_i2s_get_bits_per_sample();

    if (sample_rate == 0) {
        ESP_LOGE(TAG, "I2S not initialized, cannot set EQ");
        send_json_error(req, "500 Internal Server Error", "Audio not playing, cannot set EQ");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Attempting to configure EQ with: preset=%d, SR=%d, Ch=%d, Bits=%d", preset_index, (int)sample_rate, channels, bits_per_sample);

    esp_ae_eq_cfg_t *eq_cfg = eq_config_para(sample_rate, channels, bits_per_sample, preset_index);
    if (eq_cfg == NULL) {
        ESP_LOGE(TAG, "Failed to create EQ config for preset %d", preset_index);
        send_json_error(req, "500 Internal Server Error", "Failed to create EQ config");
        return ESP_FAIL;
    }

    if (audio_i2s_set_eq_config(eq_cfg) == ESP_OK) {
        config_set_player_eq_preset(preset_index);
        const char *preset_name = get_eq_preset_name(preset_index);
        ESP_LOGI(TAG, "EQ preset set to %s (%d)", preset_name, preset_index);
        char resp_str[128];
        snprintf(resp_str, sizeof(resp_str), "{\"status\":\"success\",\"preset_name\":\"%s\"}", preset_name);
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
    } else {
        send_json_error(req, "500 Internal Server Error", "Failed to set EQ preset");
        return ESP_FAIL;
    }
}

static esp_err_t api_playlist_handler(httpd_req_t *req)
{
    char content[50];
    int ret = httpd_req_recv(req, content, sizeof(content) - 1);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            send_json_error(req, "408 Request Timeout", "Request timed out");
        }
        return ESP_FAIL;
    }
    content[ret] = '\0';

    if (strcmp(content, "update") == 0) {
        ESP_LOGI(TAG, "Received request to update playlist.");

        esp_err_t update_err = playlist_update_sync();

        if (update_err == ESP_OK) {
            int channel_count = playlist_get_channel_count();
            ESP_LOGI(TAG, "Playlist updated successfully. New channel count: %d", channel_count);
            char resp_str[128];
            snprintf(resp_str, sizeof(resp_str), "{\"status\":\"success\",\"message\":\"Playlist updated\",\"channel_count\":%d}", channel_count);
            httpd_resp_set_type(req, "application/json");
            return httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
        } else {
            ESP_LOGE(TAG, "Failed to update playlist.");
            send_json_error(req, "500 Internal Server Error", "Failed to update playlist");
            return ESP_FAIL;
        }
    } else {
        send_json_error(req, "400 Bad Request", "Invalid parameter. Use 'update'");
        return ESP_FAIL;
    }
}

static esp_err_t api_player_handler(httpd_req_t *req) {
    char content[20];
    int ret = httpd_req_recv(req, content, sizeof(content) - 1);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    content[ret] = '\0';

    if (strcmp(content, "stop") == 0) {
        ESP_LOGI(TAG, "Received request to stop player.");
        esp_err_t stop_err = player_play_channel(-1);

        if (stop_err == ESP_OK) {
            ESP_LOGI(TAG, "Player stopped successfully.");
            char resp_str[128];
            snprintf(resp_str, sizeof(resp_str), "{\"status\":\"success\",\"message\":\"Player stopped\"}");
            httpd_resp_set_type(req, "application/json");
            return httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
        } else {
            ESP_LOGE(TAG, "Failed to stop player.");
            send_json_error(req, "500 Internal Server Error", "Failed to stop player");
            return ESP_FAIL;
        }
    } else {
        send_json_error(req, "400 Bad Request", "Invalid parameter. Use 'stop'");
        return ESP_FAIL;
    }
}

static esp_err_t api_server_start_internal(void) {
    if (server) return ESP_OK;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;
    config.max_uri_handlers = 15;

    esp_err_t ret = httpd_start(&server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start server: %s", esp_err_to_name(ret));
        return ret;
    }

    httpd_uri_t channel_uri = {
        .uri       = "/api/channel",
        .method    = HTTP_POST,
        .handler   = api_channel_handler,
        .user_ctx  = NULL
    };
    esp_err_t reg_ret = httpd_register_uri_handler(server, &channel_uri);
    if (reg_ret != ESP_OK) ESP_LOGE(TAG, "Failed to register /api/channel: %s", esp_err_to_name(reg_ret));


    httpd_uri_t volume_uri = {
        .uri       = "/api/volume",
        .method    = HTTP_POST,
        .handler   = api_volume_handler,
        .user_ctx  = NULL
    };
    reg_ret = httpd_register_uri_handler(server, &volume_uri);
    if (reg_ret != ESP_OK) ESP_LOGE(TAG, "Failed to register /api/volume: %s", esp_err_to_name(reg_ret));


    httpd_uri_t mute_uri = {
        .uri       = "/api/mute",
        .method    = HTTP_POST,
        .handler   = api_mute_handler,
        .user_ctx  = NULL
    };
    reg_ret = httpd_register_uri_handler(server, &mute_uri);
    if (reg_ret != ESP_OK) ESP_LOGE(TAG, "Failed to register /api/mute: %s", esp_err_to_name(reg_ret));


    httpd_uri_t eq_uri = {
        .uri       = "/api/eq",
        .method    = HTTP_POST,
        .handler   = api_eq_handler,
        .user_ctx  = NULL
    };
    reg_ret = httpd_register_uri_handler(server, &eq_uri);
    if (reg_ret != ESP_OK) ESP_LOGE(TAG, "Failed to register /api/eq: %s", esp_err_to_name(reg_ret));


    httpd_uri_t playlist_uri = {
        .uri       = "/api/playlist",
        .method    = HTTP_POST,
        .handler   = api_playlist_handler,
        .user_ctx  = NULL
    };
    reg_ret = httpd_register_uri_handler(server, &playlist_uri);
    if (reg_ret != ESP_OK) ESP_LOGE(TAG, "Failed to register /api/playlist: %s", esp_err_to_name(reg_ret));

    httpd_uri_t player_uri = {
        .uri       = "/api/player",
        .method    = HTTP_POST,
        .handler   = api_player_handler,
        .user_ctx  = NULL
    };
    reg_ret = httpd_register_uri_handler(server, &player_uri);
    if (reg_ret != ESP_OK) ESP_LOGE(TAG, "Failed to register /api/player: %s", esp_err_to_name(reg_ret));


    esp_err_t www_ret = www_server_register_handlers(server);
    if (www_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register www handlers: %s", esp_err_to_name(www_ret));
    } else {
        ESP_LOGI(TAG, "Successfully registered www handlers.");
    }

    ESP_LOGI(TAG, "API server started");
    return ESP_OK;
}

static void api_server_task(void *pvParameters) {
    api_server_start_internal();
    vTaskDelete(NULL);
}

esp_err_t api_server_start(void) {
    xTaskCreatePinnedToCore(api_server_task, "api_server", 4096, NULL, 5, NULL, 0);
    return ESP_OK;
}

void api_server_stop(void) {
    if (server) {
        httpd_stop(server);
        server = NULL;
        ESP_LOGI(TAG, "API server stopped");
    }
}
