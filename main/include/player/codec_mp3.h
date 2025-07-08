#ifndef CODEC_MP3_H
#define CODEC_MP3_H

#include "esp_err.h"
#include "esp_audio_dec.h"
#include "audio_i2s.h"

#define CODEC_MP3_BUFFER_SIZE (16384)     // Размер буфера для MP3 данных, можно настроить
#define CODEC_MP3_PCM_BUFFER_SIZE (16384) // Размер PCM буфера для MP3, можно настроить

esp_err_t codec_init_mp3_decoder(void);
esp_err_t codec_open_mp3_decoder(esp_audio_dec_handle_t *handle_out);
esp_err_t codec_mp3_init_buffers(void);
void codec_mp3_deinit_buffers(void);
esp_err_t codec_mp3_process_data(esp_audio_dec_handle_t dec_handle, const uint8_t *data, uint32_t len);
esp_err_t codec_mp3_unregister_decoder(esp_audio_dec_handle_t *handle);
esp_err_t codec_deinit_mp3_decoder(void);
#endif
