#ifndef PLAYLIST_H
#define PLAYLIST_H

#include "esp_err.h"
#include <stddef.h>
#include <stdio.h>

typedef struct {
    char *inf;
    char *opt;
    char *url;
} playlist_channel_data_t;

esp_err_t playlist_init(void);
esp_err_t playlist_get_channel_data(int channel_number, playlist_channel_data_t *channel_data);
void playlist_free_channel_data(playlist_channel_data_t *channel_data);
int playlist_get_channel_count(void);
void playlist_log_channel_info(int channel_number);
void playlist_cleanup(void);
esp_err_t playlist_update(void);
esp_err_t playlist_update_sync(void);
#endif
