#ifndef ACL_H
#define ACL_H

#include "esp_err.h"
#include "esp_ae_alc.h"
#include <stdint.h>

typedef struct {
    int sample_rate;
    int channels;
    int bits_per_sample;
} alc_config_t;
esp_err_t alc_init(const alc_config_t *config);
esp_err_t alc_process(void *input_buffer, void *output_buffer, int num_samples);
esp_err_t alc_set_gain_db(int channel_index, int gain_db);
esp_err_t alc_get_gain_db(int channel_index, int8_t *gain_db);
esp_err_t alc_deinit(void);
#endif
