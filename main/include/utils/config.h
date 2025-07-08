#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

// ntp
#define MAX_NTP_SERVERS_CONFIG 3
#define MAX_NTP_SERVER_LEN 64
#define MAX_TZ_LEN 64

// wifi
#define MAX_SSID_LEN 32
#define MAX_PASSWORD_LEN 64

typedef struct {
    int channel;
    int volume;
    int eq_preset;
    bool mute;
} player_config_t;

typedef struct {
    char ssid[MAX_SSID_LEN + 1];
    char password[MAX_PASSWORD_LEN + 1];
} app_wifi_config_t;

typedef struct {
    char ntp_servers[MAX_NTP_SERVERS_CONFIG][MAX_NTP_SERVER_LEN];
    int num_ntp_servers;
    char ntp_tz_display[MAX_TZ_LEN]; // Исходная строка TZ для отображения (например, "+0300")
    char ntp_tz_formatted[MAX_TZ_LEN]; // Отформатированная строка TZ для setenv (например, "GMT-3")
} ntp_config_t;

typedef struct {
    app_wifi_config_t wifi; // Используется переименованный тип
    ntp_config_t ntp;
    player_config_t player;
    bool is_loaded; // Флаг, что конфигурация успешно загружена
} app_config_t;

// Глобальный указатель на конфигурацию, доступный во всем проекте
extern const app_config_t* g_app_config;

esp_err_t config_init(void);
const app_config_t* get_app_config(void);

// Функции для обновления и сохранения настроек плеера
void config_set_player_channel(int channel);
void config_set_player_volume(int volume);
void config_set_player_eq_preset(int eq_preset);
void config_set_player_mute(bool mute);

#endif // APP_CONFIG_H
