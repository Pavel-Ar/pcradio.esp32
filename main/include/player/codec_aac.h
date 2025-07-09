#ifndef CODEC_AAC_H
#define CODEC_AAC_H

#include "esp_err.h"
#include "esp_audio_dec.h"
#include "esp_audio_types.h"
#include <stdbool.h>
#include <stdint.h>

#define CODEC_AAC_BUFFER_SIZE ( 8192 * 2 ) // Размер буфера для сырых AAC данных (аналогично HTTP_RECEIVE_BUFFER_SIZE)
#define CODEC_AAC_PCM_BUFFER_SIZE (4096)   // Размер буфера для PCM данных AAC (примерное значение, можно настроить)

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t codec_init_aac_decoder(void);
esp_err_t codec_open_aac_decoder(esp_audio_dec_handle_t *handle_out);
esp_err_t codec_aac_get_stream_info(esp_audio_dec_handle_t handle, int *out_sample_rate, int *out_channels, int *out_bits_per_sample);
esp_err_t codec_aac_init_buffers(uint32_t raw_buffer_capacity);
void codec_aac_deinit_buffers(void);
esp_err_t codec_aac_process_data(esp_audio_dec_handle_t dec_handle, const uint8_t* data, uint32_t len);
esp_err_t codec_aac_unregister_decoder(void);
esp_err_t codec_deinit_aac_decoder(void);
esp_err_t codec_aac_close_decoder(esp_audio_dec_handle_t handle);

#ifdef __cplusplus
}
#endif
#endif
