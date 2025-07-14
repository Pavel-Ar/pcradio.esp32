set(COMPONENT_SRC
    "src/main.c"
    "src/utils/hardware.c"
    "src/utils/config.c"
    "src/utils/wireless.c"
    "src/utils/ntp.c"

    "src/player/playlist.c"
    "src/player/player.c"
    "src/player/wrapper.c"
    "src/player/audio_i2s.c"
    "src/player/codec_aac.c"
    "src/player/codec_mp3.c"
    "src/player/eq.c"
    "src/player/alc.c"
    "src/player/volume.c"
    "src/player/icy.c"

    "src/web/api.c"
    "src/web/www.c"
    "src/web/metrics.c"
)

set(COMPONENT_INCLUDE_DIRS
  "include/utils"
  "include/player"
  "include/web"
  )

set(COMPONENT_PRIV_REQUIRES
    spi_flash
)

set(COMPONENT_REQUIRES
    json
    esp_wifi
    nvs_flash
    esp_http_client
    esp_http_server
    lwip
    esp_timer
    esp_audio_codec
    driver
    esp_adc
)

idf_component_register(
    SRCS ${COMPONENT_SRC}
    INCLUDE_DIRS ${COMPONENT_INCLUDE_DIRS}
    REQUIRES ${COMPONENT_REQUIRES}
    PRIV_REQUIRES ${COMPONENT_PRIV_REQUIRES}
)
