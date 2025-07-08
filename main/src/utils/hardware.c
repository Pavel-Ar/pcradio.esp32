#include "hardware.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "esp_chip_info.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_flash.h"
#include "esp_err.h"

/*
Расшифровка JEDEC ID
    Manufacturer:
        0xEF → Winbond
        0xC8 → GigaDevice
        0x20 → Micron
        0x1F → Atmel
    Capacity (LSB):
        0x15 → 2 MB
        0x16 → 4 MB
        0x17 → 8 MB
        0x18 → 16 MB
*/

void print_system_info(void) {
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    printf("This is %s chip with %d CPU core(s), WiFi%s%s, ",
           CONFIG_IDF_TARGET,
           chip_info.cores,
           (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    unsigned major_rev = chip_info.revision / 100;
    unsigned minor_rev = chip_info.revision % 100;
    printf("silicon revision v%d.%d, ", major_rev, minor_rev);

#ifdef CONFIG_ESP32_SPIRAM_SUPPORT
    // ПРИМЕЧАНИЕ: Этот блок кода и связанные с ним функции PSRAM будут активны,
    // только если поддержка PSRAM включена в конфигурации проекта (sdkconfig).
    // Это делается через menuconfig:
    // Component config ---> ESP PSRAM ---> [*] Support for external, SPI-connected RAM
    size_t psram_size = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    // ПРИМЕЧАНИЕ ПО ПОВОДУ РАЗМЕРА PSRAM:
    // Оригинальный ESP32 может отобразить в свое адресное пространство не более 4МБ PSRAM
    // для использования в качестве кучи (heap). Даже если физически установлено 8МБ PSRAM (как на многих платах Wroover),
    // psram_size здесь будет равен 4МБ. Предупреждение в логе загрузки
    // "W (XXX) esp_psram: Virtual address not enough for PSRAM, map as much as we can. 4MB is mapped"
    // как раз об этом и сообщает. Текущий код тестирует память, доступную через heap.
    if (psram_size > 0) {
        printf("%uMB %s PSRAM found (in heap)\n", (unsigned int)(psram_size / (1024 * 1024)),
               (chip_info.features & CHIP_FEATURE_EMB_PSRAM) ? "embedded" : "external");
    } else {
        printf("PSRAM support compiled in, but no PSRAM heap available (size is 0).\n");
    }
#else // CONFIG_ESP32_SPIRAM_SUPPORT не определен
    // ПРИМЕЧАНИЕ: Если вы видите сообщение "PSRAM support not enabled in project configuration",
    // это означает, что CONFIG_ESP32_SPIRAM_SUPPORT не определен.
    // Вам необходимо включить поддержку PSRAM в menuconfig (см. инструкции выше),
    // чтобы приложение могло использовать PSRAM и чтобы тесты PSRAM выполнялись.
    if (chip_info.features & CHIP_FEATURE_EMB_PSRAM) {
        printf("Chip theoretically supports embedded PSRAM, but PSRAM support is not enabled in project configuration.\n");
    } else {
        printf("PSRAM support not enabled in project configuration.\n");
    }
#endif

    uint32_t jedec_id;
    esp_err_t ret = esp_flash_read_id(NULL, &jedec_id);
    if (ret == ESP_OK) {
        ESP_LOGI("FLASH", "JEDEC ID: 0x%08lX", jedec_id);
        uint8_t mfg_id = (jedec_id >> 16) & 0xFF;
        uint8_t mem_type = (jedec_id >> 8) & 0xFF; // JEDEC memory type
        uint8_t capacity_code = jedec_id & 0xFF;    // JEDEC capacity code
        ESP_LOGI("FLASH", "Manufacturer ID: 0x%02X, Memory Type: 0x%02X, Capacity Code: 0x%02X", mfg_id, mem_type, capacity_code);

        if (capacity_code >= 0x14 && capacity_code <= 0x21) { // 0x21 for 128MB (1Gbit)
            uint32_t actual_flash_size_mb = 1 << (capacity_code - 0x14);
            ESP_LOGI("FLASH", "Actual Flash Size (from JEDEC ID capacity_code 0x%02X): %lu MB", capacity_code, actual_flash_size_mb);
        } else {
            ESP_LOGW("FLASH", "Flash capacity code 0x%02X is outside the common calculation range (0x14-0x21). Actual size may not be accurately calculated by this method.", capacity_code);
        }
    } else {
        ESP_LOGE("FLASH", "Failed to read JEDEC ID: %s", esp_err_to_name(ret));
    }

    // Также выведем размер, сконфигурированный в системе (из таблицы разделов)
    uint32_t configured_flash_size_bytes = 0;
    esp_err_t flash_size_ret = esp_flash_get_size(NULL, &configured_flash_size_bytes);
    if (flash_size_ret == ESP_OK && configured_flash_size_bytes > 0) {
        ESP_LOGI("FLASH", "Configured Flash Size (reported by system/partition table): %lu MB", configured_flash_size_bytes / (1024 * 1024));
    } else {
        ESP_LOGE("FLASH", "Failed to get configured flash size: %s", esp_err_to_name(flash_size_ret));
    }

    // Добавим информацию о доступной памяти
    ESP_LOGI("MEMORY", "Available heap memory:");
    ESP_LOGI("MEMORY", "  Internal RAM (all): Free: %zu bytes, Largest Free Block: %zu bytes",
             heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    ESP_LOGI("MEMORY", "  Internal RAM (DMA capable): Free: %zu bytes, Largest Free Block: %zu bytes",
             heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA),
             heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA));
    ESP_LOGI("MEMORY", "  Default (usually Internal): Free: %zu bytes, Largest Free Block: %zu bytes",
             heap_caps_get_free_size(MALLOC_CAP_DEFAULT),
             heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));

#ifdef CONFIG_ESP32_SPIRAM_SUPPORT
    // psram_size была определена ранее в этой функции при проверке наличия PSRAM
    // ПРИМЕЧАНИЕ: Весь этот блок тестирования PSRAM зависит от CONFIG_ESP32_SPIRAM_SUPPORT.
    // Убедитесь, что поддержка PSRAM включена в menuconfig.
    // psram_size здесь - это размер PSRAM, добавленной в кучу (обычно 4МБ для ESP32 с >4МБ физической PSRAM).
    size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    if (psram_free > 0) { // Проверяем, есть ли свободная PSRAM, даже если она сконфигурирована
        ESP_LOGI("MEMORY", "  PSRAM (in heap): Free: %zu bytes, Largest Free Block: %zu bytes",
                 psram_free,
                 heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
    } else {
         ESP_LOGI("MEMORY", "  PSRAM (in heap): Configured, but no free heap reported (or size is 0).");
    }

    // Тест PSRAM
    // psram_size (общий размер, добавленный в кучу) и psram_free (свободный размер в куче) определены выше
    // Изменяем условие с > на >= чтобы тест выполнялся, если доступно ровно 4MB PSRAM в куче.
    if (psram_size >= (4 * 1024 * 1024)) { // Условие: общий размер PSRAM в куче >= 4MB
        size_t largest_free_block_psram = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
        if (largest_free_block_psram > 0) { // И есть свободная PSRAM для теста
            ESP_LOGI("PSRAM_TEST", "Total PSRAM in heap >= 4MB (%uMB). Largest Free Block: %zu bytes. Performing memory test on this block.",
                     (unsigned int)(psram_size / (1024 * 1024)), largest_free_block_psram);

            size_t test_size = largest_free_block_psram; // Пытаемся протестировать самый большой непрерывный блок

            ESP_LOGI("PSRAM_TEST", "Attempting to allocate %zu bytes from PSRAM heap for test.", test_size);
            uint8_t *psram_buffer = (uint8_t *)heap_caps_malloc(test_size, MALLOC_CAP_SPIRAM);

            if (psram_buffer != NULL) {
                ESP_LOGI("PSRAM_TEST", "Successfully allocated %zu bytes from PSRAM heap.", test_size);

                // Заполнение и проверка
                bool test_ok = true;
                ESP_LOGI("PSRAM_TEST", "Writing pattern (0xA5) to %zu bytes...", test_size);
                for (size_t i = 0; i < test_size; i++) {
                    psram_buffer[i] = 0xA5;
                }

                ESP_LOGI("PSRAM_TEST", "Verifying pattern in %zu bytes...", test_size);
                for (size_t i = 0; i < test_size; i++) {
                    if (psram_buffer[i] != 0xA5) {
                        ESP_LOGE("PSRAM_TEST", "Verification failed at byte %zu! Expected 0xA5, got 0x%02X", i, psram_buffer[i]);
                        test_ok = false;
                        break;
                    }
                }

                if (test_ok) {
                    ESP_LOGI("PSRAM_TEST", "PSRAM memory test (%zu bytes write/read) PASSED.", test_size);
                } else {
                    ESP_LOGE("PSRAM_TEST", "PSRAM memory test (%zu bytes write/read) FAILED.", test_size);
                }

                heap_caps_free(psram_buffer);
                ESP_LOGI("PSRAM_TEST", "Freed %zu bytes from PSRAM heap.", test_size);
            } else {
                ESP_LOGE("PSRAM_TEST", "Failed to allocate %zu bytes from PSRAM heap for test. This shouldn't happen if test_size was from largest_free_block.",
                         test_size);
            }
        } else { // psram_size >= 4MB, но нет свободных блоков в PSRAM
            ESP_LOGI("PSRAM_TEST", "Total PSRAM in heap >= 4MB (%uMB), but no free PSRAM blocks available to test (Largest Free Block: 0 bytes).",
                     (unsigned int)(psram_size / (1024 * 1024)));
        }
    } else if (psram_size > 0) { // psram_size < 4MB, но > 0
        ESP_LOGI("PSRAM_TEST", "Total PSRAM in heap is %uMB (< 4MB). Largest Free Block: %zu bytes. Not performing extensive test.",
                 (unsigned int)(psram_size / (1024 * 1024)), heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
    }
    // Если psram_size == 0, то начальные проверки PSRAM уже обработали бы это.
#endif
}
