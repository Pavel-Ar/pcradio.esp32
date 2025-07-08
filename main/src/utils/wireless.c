#include "wireless.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "config.h"

static const char *TAG = "wifi_station";

#define DEFAULT_WIFI_SSID ""
#define DEFAULT_WIFI_PASSWORD ""


static EventGroupHandle_t s_wifi_event_group;


static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Disconnected from AP, attempting to reconnect...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP address:" IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}


void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    const app_config_t *cfg = get_app_config();
    if (!cfg || !cfg->is_loaded) {
        ESP_LOGE(TAG, "Application configuration not loaded. Aborting Wi-Fi initialization.");
        return;
    }

    char current_ssid[MAX_SSID_LEN + 1];
    char current_password[MAX_PASSWORD_LEN + 1];

    strncpy(current_ssid, cfg->wifi.ssid, sizeof(current_ssid) -1);
    current_ssid[sizeof(current_ssid)-1] = '\0';
    strncpy(current_password, cfg->wifi.password, sizeof(current_password) -1);
    current_password[sizeof(current_password)-1] = '\0';

    if (strlen(current_ssid) == 0 || strcmp(current_ssid, DEFAULT_WIFI_SSID) == 0) {
        ESP_LOGE(TAG, "Wi-Fi SSID is empty or default. Please configure Wi-Fi credentials in /storage/config.json.");
    }


    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    if (strlen(current_ssid) == 0) {
        ESP_LOGE(TAG, "No valid Wi-Fi SSID. Aborting Wi-Fi start.");
        return;
    }


    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    strncpy((char*)wifi_config.sta.ssid, current_ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char*)wifi_config.sta.password, current_password, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished. Waiting for connection to SSID: %s...", current_ssid);

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to AP SSID: %s", current_ssid);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT while waiting for Wi-Fi connection");
    }
}

EventGroupHandle_t get_wifi_event_group(void) {
    return s_wifi_event_group;
}
