#ifndef EQ_H_
#define EQ_H_

#include <stdio.h>
#include "esp_ae_eq.h"

#define EQ_PRESET_DEFAULT       0
#define EQ_PRESET_DANCE         1
#define EQ_PRESET_FULL_BASS     2
#define EQ_PRESET_FULL_TREBLE   3
#define EQ_PRESET_POP           4
#define EQ_PRESET_ROCK          5
#define EQ_PRESET_SOFT          6
#define EQ_PRESET_LARGE_HALL    7
#define EQ_PRESET_PARTY         8
#define EQ_PRESET_CLUB          9
#define EQ_PRESET_COUNT         10

void filter_para_cfg(int fc, float q, float gain, esp_ae_eq_filter_type_t filter_type,
                            esp_ae_eq_filter_para_t *eq_para);
esp_ae_eq_cfg_t *eq_config_single(int sample_rate, int channel, int bit,
                                      int fc, float q, float gain,
                                      esp_ae_eq_filter_type_t filter_type);
esp_ae_eq_cfg_t *eq_config_para(int sample_rate, int channel, int bit, int preset_index);
const char* get_eq_preset_name(int preset_index);
#endif
