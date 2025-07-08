#include <stdio.h>

#include "wireless.h"
#include "ntp.h"
#include "playlist.h"
#include "player.h"
#include "audio_i2s.h"
#include "eq.h"
#include "alc.h"
#include "api.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "hardware.h"
#include "config.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"

static const char *TAG = "MAIN";

void app_main(void) {
    print_system_info();
    config_init();
    wifi_init_sta();
    ntp_init();
    playlist_init();
    api_server_start();

    if (player_init() == ESP_OK) {
        ESP_LOGI(TAG, "Applying player settings from config: volume=%d, mute=%d, eq_preset=%d, channel=%d",
                 g_app_config->player.volume,
                 g_app_config->player.mute,
                 g_app_config->player.eq_preset,
                 g_app_config->player.channel);

        audio_i2s_set_volume(g_app_config->player.volume);
        player_play_channel(g_app_config->player.channel);
    }

    while(1) {
      vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
