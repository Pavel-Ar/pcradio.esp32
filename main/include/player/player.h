#ifndef PLAYER_H
#define PLAYER_H

#include "esp_err.h"

esp_err_t player_init(void);
esp_err_t player_play_channel(int channel_number);
esp_err_t player_stop(void);
esp_err_t player_deinit(void);
const char* player_get_icy_name(void);
#endif
