#include <stdio.h>
#include <string.h>
#include <math.h>

#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_ae_eq.h"
#include "esp_ae_bit_cvt.h"
#include "esp_ae_data_weaver.h"
#include "eq.h"

#define TAG "EQ"

static const char *eq_preset_names_internal[EQ_PRESET_COUNT] = {
    "DEFAULT",
    "DANCE",
    "FULL BASS",
    "FULL TREBLE",
    "POP",
    "ROCK",
    "SOFT",
    "LARGE HALL",
    "PARTY",
    "CLUB"
};

static int music_mode[EQ_PRESET_COUNT][10] = { // Используем EQ_PRESET_COUNT
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},         // default (EQ_PRESET_DEFAULT)
    {9, 8, 5, 2, 1, 0, -3, -4, -3, 0},      // dance (EQ_PRESET_DANCE)
    {8, 8, 8, 7, 4, 0, -3, -5, -7, -9},     // full bass (EQ_PRESET_FULL_BASS)
    {-9, -8, -7, -6, -3, 1, 5, 8, 10, 11},  // full treble (EQ_PRESET_FULL_TREBLE)
    {-2, -1, 0, 2, 3, 2, 0, -2, -2, -1},    // pop (EQ_PRESET_POP)
    {6, 5, 2, -2, -5, -2, 0, 3, 5, 6},      // rock (EQ_PRESET_ROCK)
    {2, 1, 0, 0, -1, 0, 1, 2, 3, 4},        // soft (EQ_PRESET_SOFT)
    {8, 7, 6, 3, 2, 0, -1, -2, -1, 0},      // large_hall (EQ_PRESET_LARGE_HALL)
    {4, 4, 3, 2, 0, 0, 0, 0, 0, 4},         // party (EQ_PRESET_PARTY)
    {0, 0, 0, 1, 2, 3, 3, 2, 1, 0}          // club (EQ_PRESET_CLUB)
};

void filter_para_cfg(int fc, float q_val, float gain_val, esp_ae_eq_filter_type_t filter_type,
                            esp_ae_eq_filter_para_t *eq_para)
{
    eq_para->filter_type = filter_type;
    eq_para->fc = fc;
    eq_para->q = q_val;
    eq_para->gain = gain_val;
}

esp_ae_eq_cfg_t *eq_config_single(int sample_rate, int channel, int bit,
                                      int fc, float q_val, float gain_val,
                                      esp_ae_eq_filter_type_t filter_type)
{
    esp_ae_eq_cfg_t *eq_config = calloc(1, sizeof(esp_ae_eq_cfg_t));
    if (eq_config == NULL) {
        ESP_LOGI(TAG, "eq config calloc error.");
        return NULL;
    }
    eq_config->bits_per_sample = bit;
    eq_config->sample_rate = sample_rate;
    eq_config->channel = channel;
    eq_config->filter_num = 1;
    eq_config->para = calloc(1, sizeof(esp_ae_eq_filter_para_t) * eq_config->filter_num);
    if (eq_config->para == NULL) {
        ESP_LOGI(TAG, "para calloc error.");
        free(eq_config);
        return NULL;
    }
    esp_ae_eq_filter_para_t *para = eq_config->para;
    filter_para_cfg(fc, q_val, gain_val, filter_type, para);
    return eq_config;
}

esp_ae_eq_cfg_t *eq_config_para(int sample_rate, int channel, int bit, int preset_index)
{
    esp_ae_eq_cfg_t *eq_config = calloc(1, sizeof(esp_ae_eq_cfg_t));
    if (eq_config == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for EQ config");
        return NULL;
    }
    eq_config->bits_per_sample = bit;
    eq_config->sample_rate = sample_rate;
    eq_config->channel = channel;
    eq_config->filter_num = 10;
    eq_config->para = calloc(1, sizeof(esp_ae_eq_filter_para_t) * eq_config->filter_num);
    if (eq_config->para == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for EQ parameters");
        free(eq_config);
        return NULL;
    }
    esp_ae_eq_filter_para_t *para = eq_config->para;
    int fc_arr[10] = {31, 62, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};
    float q_arr[10];
    float gain_arr[10];

    if (preset_index < 0 || preset_index >= EQ_PRESET_COUNT) {
        ESP_LOGE(TAG, "Invalid preset index: %d. Using default.", preset_index);
        preset_index = EQ_PRESET_DEFAULT;
    }

    for (int i = 0; i < eq_config->filter_num; i++) {
        gain_arr[i] = music_mode[preset_index][i];
        q_arr[i] = 1.0f;
    }

    esp_ae_eq_filter_type_t filter_type = ESP_AE_EQ_FILTER_PEAK;

    for (int i = 0; i < eq_config->filter_num; i++) {
        filter_para_cfg(fc_arr[i], q_arr[i], gain_arr[i], filter_type, &para[i]);
    }

    return eq_config;
}

const char* get_eq_preset_name(int preset_index) {
    if (preset_index >= 0 && preset_index < EQ_PRESET_COUNT) {
        return eq_preset_names_internal[preset_index];
    }
    return EQ_PRESET_DEFAULT;
}
