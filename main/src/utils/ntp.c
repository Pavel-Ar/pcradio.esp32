#include "ntp.h"
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "esp_wifi.h"
#include "wireless.h"
#include "config.h"
#include "esp_heap_caps.h"

#define SNTP_UPDATE_INTERVAL_SUCCESS_MS (3 * 60 * 60 * 1000)
#define SNTP_UPDATE_INTERVAL_RETRY_MS   (3 * 60 * 60 * 1000)
#define SNTP_TASK_STACK_SIZE            (4096)
#define SNTP_TASK_NAME                  "sntp_task"

static const char *TAG = "NTP";
static bool s_time_initialized = false;

static void time_sync_notification_cb(struct timeval *tv) {
    const app_config_t *cfg = get_app_config();
    ESP_LOGI(TAG, "Time synchronized via SNTP.");
    setenv("TZ", cfg->ntp.ntp_tz_formatted, 1);
    tzset();
    ESP_LOGI(TAG, "Timezone for C library set to: '%s' (from display config: '%s')", cfg->ntp.ntp_tz_formatted, cfg->ntp.ntp_tz_display);
    s_time_initialized = true;

    time_t now = tv->tv_sec;
    struct tm timeinfo_local;
    struct tm timeinfo_utc;

    char *strftime_buf_local = heap_caps_malloc(64, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    char *strftime_buf_utc = heap_caps_malloc(64, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (strftime_buf_local && strftime_buf_utc) {
        gmtime_r(&now, &timeinfo_utc);
        strftime(strftime_buf_utc, 64, "%Y-%m-%d %H:%M:%S", &timeinfo_utc);

        localtime_r(&now, &timeinfo_local);
        strftime(strftime_buf_local, 64, "%Y-%m-%d %H:%M:%S", &timeinfo_local);

        ESP_LOGI(TAG, "Post-sync time (UTC): %s", strftime_buf_utc);
        ESP_LOGI(TAG, "Post-sync time (Local, TZ: %s): %s", cfg->ntp.ntp_tz_display, strftime_buf_local);
    } else {
        ESP_LOGE(TAG, "Failed to allocate time string buffers in PSRAM for time_sync_notification_cb. Time details will not be logged.");
        if (strftime_buf_local) free(strftime_buf_local);
        if (strftime_buf_utc) free(strftime_buf_utc);
        strftime_buf_local = NULL;
        strftime_buf_utc = NULL;
    }

    if (strftime_buf_local) free(strftime_buf_local);
    if (strftime_buf_utc) free(strftime_buf_utc);
}

static void initialize_sntp(void) {
    const app_config_t *cfg = get_app_config();
    if (!cfg || !cfg->is_loaded) {
        ESP_LOGE(TAG, "NTP config not loaded. Cannot initialize SNTP.");
        return;
    }

    ESP_LOGI(TAG, "Initializing SNTP...");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);

    if (cfg->ntp.num_ntp_servers > 0) {
        for (int i = 0; i < cfg->ntp.num_ntp_servers && i < MAX_NTP_SERVERS_CONFIG; ++i) {
            if (strlen(cfg->ntp.ntp_servers[i]) > 0) {
                 ESP_LOGI(TAG, "Setting SNTP server %d: %s", i, cfg->ntp.ntp_servers[i]);
                esp_sntp_setservername(i, cfg->ntp.ntp_servers[i]);
            } else if (i == 0) {
                ESP_LOGW(TAG, "Primary NTP server string is empty, using default pool.ntp.org for server 0");
                esp_sntp_setservername(0, "pool.ntp.org");
            }
        }
    } else {
        ESP_LOGW(TAG, "No NTP servers configured, using default pool.ntp.org for server 0");
        esp_sntp_setservername(0, "pool.ntp.org");
    }
    esp_sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    esp_sntp_init();
}

static void stop_sntp(void) {
    if (esp_sntp_enabled()) {
        esp_sntp_stop();
        ESP_LOGI(TAG, "SNTP stopped.");
    }
}

static void sntp_task(void *pvParameters) {
    const app_config_t *cfg = get_app_config();
    EventGroupHandle_t wifi_event_group = get_wifi_event_group();
    if (wifi_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to get Wi-Fi event group. SNTP task cannot start.");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "SNTP task started. Waiting for Wi-Fi connection...");
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    ESP_LOGI(TAG, "Wi-Fi connected. Proceeding with SNTP initialization.");

    initialize_sntp();

    TickType_t next_update_delay = pdMS_TO_TICKS(10000);

    while (1) {
        vTaskDelay(next_update_delay);

        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) != ESP_OK) {
            ESP_LOGW(TAG, "Wi-Fi disconnected. Stopping SNTP and waiting for reconnection.");
            stop_sntp();
            xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
            ESP_LOGI(TAG, "Wi-Fi reconnected. Re-initializing SNTP.");
            initialize_sntp();
            s_time_initialized = false;
            next_update_delay = pdMS_TO_TICKS(10000);
            continue;
        }

        if (esp_sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
            if (!s_time_initialized) {
                ESP_LOGW(TAG, "SNTP status is COMPLETED, but s_time_initialized is false. Re-setting TZ.");
                setenv("TZ", cfg->ntp.ntp_tz_formatted, 1);
                tzset();
                s_time_initialized = true;
            }
            ESP_LOGI(TAG, "Time is synchronized.");

            time_t now;
            struct tm timeinfo_local;
            struct tm timeinfo_utc;

            char *strftime_buf_local = heap_caps_malloc(64, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            char *strftime_buf_utc = heap_caps_malloc(64, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

            if (strftime_buf_local && strftime_buf_utc) {
                time(&now);

                localtime_r(&now, &timeinfo_local);
                strftime(strftime_buf_local, 64, "%Y-%m-%d %H:%M:%S", &timeinfo_local);

                gmtime_r(&now, &timeinfo_utc);
                strftime(strftime_buf_utc, 64, "%Y-%m-%d %H:%M:%S", &timeinfo_utc);

                ESP_LOGI(TAG, "Current raw time_t (UTC seconds since epoch): %ld", (long)now);
                ESP_LOGI(TAG, "Current time (UTC): %s", strftime_buf_utc);
                ESP_LOGI(TAG, "Current time (Local, TZ: %s): %s", cfg->ntp.ntp_tz_display, strftime_buf_local);
            } else {
                ESP_LOGE(TAG, "sntp_task: Failed to allocate time string buffers in PSRAM. Time details will not be logged.");
                if (strftime_buf_local) free(strftime_buf_local);
                if (strftime_buf_utc) free(strftime_buf_utc);
                strftime_buf_local = NULL;
                strftime_buf_utc = NULL;
            }
            if (strftime_buf_local) free(strftime_buf_local);
            if (strftime_buf_utc) free(strftime_buf_utc);

            next_update_delay = pdMS_TO_TICKS(SNTP_UPDATE_INTERVAL_SUCCESS_MS);
        } else {
            ESP_LOGI(TAG, "Time not synchronized. Current SNTP status: %d. Attempting to sync...", esp_sntp_get_sync_status());
            if (!esp_sntp_enabled()) {
                 ESP_LOGI(TAG, "SNTP was not enabled. Re-initializing.");
                 initialize_sntp();
                 s_time_initialized = false;
            }
            ESP_LOGI(TAG, "Next SNTP check/attempt in approx. %ld hours.", (long)(SNTP_UPDATE_INTERVAL_RETRY_MS / (60*60*1000)));
            next_update_delay = pdMS_TO_TICKS(SNTP_UPDATE_INTERVAL_RETRY_MS);
        }
    }
}

void ntp_init(void) {
    const app_config_t *cfg = get_app_config();
    if (!cfg || !cfg->is_loaded) {
         ESP_LOGE(TAG, "NTP configuration not available. Cannot proceed with ntp_init.");
        setenv("TZ", "UTC", 1);
        tzset();
        ESP_LOGW(TAG, "Effective timezone for C library set to 'UTC' due to config load issue.");
    } else {
        ESP_LOGI(TAG, "NTP module init using loaded configuration.");
        setenv("TZ", cfg->ntp.ntp_tz_formatted, 1);
        tzset();
        ESP_LOGI(TAG, "Effective timezone for C library set to '%s' on init (display config: '%s').", cfg->ntp.ntp_tz_formatted, cfg->ntp.ntp_tz_display);
    }

    BaseType_t task_created = xTaskCreatePinnedToCore(
        sntp_task,
        SNTP_TASK_NAME,
        SNTP_TASK_STACK_SIZE,
        NULL,
        5,
        NULL,
        0
    );

    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create sntp_task on core 0.");
    } else {
        ESP_LOGI(TAG, "sntp_task created and pinned to core 0.");
    }
}

void ntp_get_time_string(char *buf, size_t buf_len) {
    time_t now;
    struct tm timeinfo_local;
    time(&now);
    localtime_r(&now, &timeinfo_local);

    if (s_time_initialized) {
        strftime(buf, buf_len, "%Y-%m-%d %H:%M:%S %Z", &timeinfo_local);
    } else {
        snprintf(buf, buf_len, "Time not synchronized yet");
    }
}
