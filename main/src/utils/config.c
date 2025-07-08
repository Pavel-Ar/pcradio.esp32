#include "config.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "esp_littlefs.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

static const char *TAG = "CONFIG";
#define CONFIG_FILE_PATH "/storage/config.json"
#define PLAYER_CONFIG_FILE_PATH "/storage/player.json"
#define LITTLEFS_PARTITION_LABEL "storage"
#define LITTLEFS_BASE_PATH "/storage"

static app_config_t s_app_config;
const app_config_t* g_app_config = &s_app_config;

static void player_config_save(void) {
    cJSON *root = cJSON_CreateObject();
    cJSON *player_json = cJSON_CreateObject();

    cJSON_AddNumberToObject(player_json, "channel", s_app_config.player.channel);
    cJSON_AddNumberToObject(player_json, "volume", s_app_config.player.volume);
    cJSON_AddNumberToObject(player_json, "eq_preset", s_app_config.player.eq_preset);
    cJSON_AddBoolToObject(player_json, "mute", s_app_config.player.mute);
    cJSON_AddItemToObject(root, "player", player_json);

    char* json_string = cJSON_Print(root);
    cJSON_Delete(root);

    if (!json_string) {
        ESP_LOGE(TAG, "Failed to print player JSON string");
        return;
    }

    FILE *f = fopen(PLAYER_CONFIG_FILE_PATH, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open %s for writing", PLAYER_CONFIG_FILE_PATH);
        free(json_string);
        return;
    }

    fprintf(f, "%s", json_string);
    fclose(f);
    free(json_string);
    ESP_LOGI(TAG, "Player configuration saved to %s", PLAYER_CONFIG_FILE_PATH);
}

void config_set_player_channel(int channel) {
    if (s_app_config.player.channel != channel) {
        s_app_config.player.channel = channel;
        player_config_save();
    }
}

void config_set_player_volume(int volume) {
    if (s_app_config.player.volume != volume) {
        s_app_config.player.volume = volume;
        player_config_save();
    }
}

void config_set_player_eq_preset(int eq_preset) {
    if (s_app_config.player.eq_preset != eq_preset) {
        s_app_config.player.eq_preset = eq_preset;
        player_config_save();
    }
}

void config_set_player_mute(bool mute) {
    if (s_app_config.player.mute != mute) {
        s_app_config.player.mute = mute;
        player_config_save();
    }
}


static void util_format_tz_string(const char* input_tz, char* output_tz, size_t output_len) {
    if (input_tz == NULL || output_tz == NULL || output_len == 0) {
        if (output_tz != NULL && output_len > 0) strncpy(output_tz, "UTC", output_len -1);
        if (output_len > 0) output_tz[output_len-1] = '\0';
        return;
    }

    bool format_success = false;
    char sign_char = 0;
    int offset_h = 0;
    int offset_m = 0;

    if (strcmp(input_tz, "UTC") == 0) {
        strncpy(output_tz, "UTC", output_len -1);
        format_success = true;
    } else if (strcmp(input_tz, "0") == 0) {
        strncpy(output_tz, "GMT", output_len -1);
        format_success = true;
    } else if (strlen(input_tz) == 5) {
        sign_char = input_tz[0];
        if ((sign_char == '+' || sign_char == '-') &&
            isdigit((unsigned char)input_tz[1]) && isdigit((unsigned char)input_tz[2]) &&
            isdigit((unsigned char)input_tz[3]) && isdigit((unsigned char)input_tz[4])) {
            char hh_str[3] = {input_tz[1], input_tz[2], '\0'};
            char mm_str[3] = {input_tz[3], input_tz[4], '\0'};
            offset_h = atoi(hh_str);
            offset_m = atoi(mm_str);
            if (offset_h >= 0 && offset_h <= 23 && offset_m >= 0 && offset_m <= 59) {
                if (sign_char == '+') {
                    if (offset_m == 0) snprintf(output_tz, output_len, "GMT-%d", offset_h);
                    else snprintf(output_tz, output_len, "GMT-%d:%02d", offset_h, offset_m);
                } else {
                    if (offset_m == 0) snprintf(output_tz, output_len, "GMT+%d", offset_h);
                    else snprintf(output_tz, output_len, "GMT+%d:%02d", offset_h, offset_m);
                }
                format_success = true;
            }
        }
    }

    if (!format_success) {
        ESP_LOGW(TAG, "TZ string '%s' in config is not valid. Using UTC as default for setenv.", input_tz);
        strncpy(output_tz, "UTC", output_len -1);
    }
    output_tz[output_len-1] = '\0';
    ESP_LOGI(TAG, "Input config TZ: '%s', Formatted TZ for setenv: '%s'", input_tz, output_tz);
}

static esp_err_t init_littlefs_if_not_done(void) {
    size_t total = 0, used = 0;
    esp_err_t ret_info = esp_littlefs_info(LITTLEFS_PARTITION_LABEL, &total, &used);

    if (ret_info == ESP_OK) {
        ESP_LOGI(TAG, "LittleFS already mounted on partition '%s'. Total: %d bytes, Used: %d bytes",
                 LITTLEFS_PARTITION_LABEL, total, used);
        struct stat st;
        if (stat(LITTLEFS_BASE_PATH, &st) == 0 && S_ISDIR(st.st_mode)) {
            ESP_LOGI(TAG, "Base path %s is accessible.", LITTLEFS_BASE_PATH);
            return ESP_OK;
        } else {
            ESP_LOGW(TAG, "LittleFS partition info OK, but base path %s not accessible or not a directory. Will attempt to re-register.", LITTLEFS_BASE_PATH);
            esp_vfs_littlefs_unregister(LITTLEFS_PARTITION_LABEL);
        }
    } else if (ret_info == ESP_ERR_NOT_FOUND) {
        ESP_LOGI(TAG, "LittleFS partition '%s' not found or not mounted. Initializing...", LITTLEFS_PARTITION_LABEL);
    } else {
        ESP_LOGW(TAG, "Error getting LittleFS info for partition '%s': %s. Will attempt to initialize.",
                 LITTLEFS_PARTITION_LABEL, esp_err_to_name(ret_info));
    }

    esp_vfs_littlefs_conf_t conf = {
      .base_path = LITTLEFS_BASE_PATH,
      .partition_label = LITTLEFS_PARTITION_LABEL,
      .format_if_mount_failed = false,
      .dont_mount = false,
    };

    int retry_count = 0;
    const int max_retries = 5;
    const int retry_delay_ms = 3000;

    while (retry_count < max_retries) {
        ESP_LOGI(TAG, "Attempting to initialize LittleFS (Attempt %d/%d)...", retry_count + 1, max_retries);
        esp_err_t ret_register = esp_vfs_littlefs_register(&conf);
        if (ret_register == ESP_OK) {
            ESP_LOGI(TAG, "LittleFS initialized and mounted successfully at %s", LITTLEFS_BASE_PATH);
            return ESP_OK;
        } else {
            ESP_LOGE(TAG, "Failed to initialize/register LittleFS (%s)", esp_err_to_name(ret_register));
            retry_count++;
            if (retry_count < max_retries) {
                ESP_LOGI(TAG, "Retrying in %d ms...", retry_delay_ms);
                vTaskDelay(pdMS_TO_TICKS(retry_delay_ms));
            }
        }
    }

    ESP_LOGE(TAG, "Failed to initialize LittleFS after %d attempts. This is a blocking issue. Halting.", max_retries);
    while(1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    return ESP_FAIL;
}


esp_err_t config_init(void) {
    s_app_config.is_loaded = false;

    esp_err_t ret_nvs = nvs_flash_init();
    if (ret_nvs == ESP_ERR_NVS_NO_FREE_PAGES || ret_nvs == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret_nvs = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret_nvs);
    ESP_LOGI(TAG, "NVS initialized.");

    strncpy(s_app_config.wifi.ssid, "DEFAULT_SSID", MAX_SSID_LEN);
    strncpy(s_app_config.wifi.password, "DEFAULT_PASS", MAX_PASSWORD_LEN);
    s_app_config.ntp.num_ntp_servers = 1;
    strncpy(s_app_config.ntp.ntp_servers[0], "pool.ntp.org", MAX_NTP_SERVER_LEN);
    for (int i = 1; i < MAX_NTP_SERVERS_CONFIG; ++i) {
        s_app_config.ntp.ntp_servers[i][0] = '\0';
    }
    strncpy(s_app_config.ntp.ntp_tz_display, "UTC", MAX_TZ_LEN);
    util_format_tz_string("UTC", s_app_config.ntp.ntp_tz_formatted, MAX_TZ_LEN);

    s_app_config.player.channel = -1;
    s_app_config.player.volume = 50;
    s_app_config.player.eq_preset = 0;
    s_app_config.player.mute = false;

    if (init_littlefs_if_not_done() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize LittleFS. Cannot load configuration. Halting.");
        while(1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    FILE* f_main = fopen(CONFIG_FILE_PATH, "r");
    if (f_main == NULL) {
        ESP_LOGE(TAG, "Failed to open %s. Configuration is critical, cannot continue. Halting.", CONFIG_FILE_PATH);
        while(1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
    }

    fseek(f_main, 0, SEEK_END);
    long main_file_size = ftell(f_main);
    fseek(f_main, 0, SEEK_SET);

    if (main_file_size <= 0) {
        ESP_LOGE(TAG, "File %s is empty. Halting.", CONFIG_FILE_PATH);
        fclose(f_main);
        while(1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
    }

    char* main_json_string = malloc(main_file_size + 1);
    if (main_json_string == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for main JSON. Halting.");
        fclose(f_main);
        while(1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
    }

    fread(main_json_string, 1, main_file_size, f_main);
    fclose(f_main);
    main_json_string[main_file_size] = '\0';

    cJSON *main_root = cJSON_Parse(main_json_string);
    free(main_json_string);

    if (main_root == NULL) {
        ESP_LOGE(TAG, "Error parsing %s. Halting. Error: [%s]", CONFIG_FILE_PATH, cJSON_GetErrorPtr() ? cJSON_GetErrorPtr() : "unknown");
        while(1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
    }

    FILE* f_player = fopen(PLAYER_CONFIG_FILE_PATH, "r");
    if (f_player == NULL) {
        ESP_LOGW(TAG, "%s not found. Creating with default values.", PLAYER_CONFIG_FILE_PATH);
        player_config_save();
    } else {
        fseek(f_player, 0, SEEK_END);
        long player_file_size = ftell(f_player);
        fseek(f_player, 0, SEEK_SET);

        if (player_file_size > 0) {
            char* player_json_string = malloc(player_file_size + 1);
            if (player_json_string) {
                fread(player_json_string, 1, player_file_size, f_player);
                player_json_string[player_file_size] = '\0';
                cJSON *player_root = cJSON_Parse(player_json_string);
                free(player_json_string);

                if (player_root) {
                    cJSON *player_json = cJSON_GetObjectItemCaseSensitive(player_root, "player");
                    if (player_json) {
                        cJSON *channel = cJSON_GetObjectItem(player_json, "channel");
                        if (cJSON_IsNumber(channel)) s_app_config.player.channel = channel->valueint;

                        cJSON *volume = cJSON_GetObjectItem(player_json, "volume");
                        if (cJSON_IsNumber(volume)) s_app_config.player.volume = volume->valueint;

                        cJSON *eq_preset = cJSON_GetObjectItem(player_json, "eq_preset");
                        if (cJSON_IsNumber(eq_preset)) s_app_config.player.eq_preset = eq_preset->valueint;

                        cJSON *mute = cJSON_GetObjectItem(player_json, "mute");
                        if (cJSON_IsBool(mute)) s_app_config.player.mute = cJSON_IsTrue(mute);
                    }
                    cJSON_Delete(player_root);
                } else {
                    ESP_LOGW(TAG, "Failed to parse %s. Using default player config.", PLAYER_CONFIG_FILE_PATH);
                }
            }
        }
        fclose(f_player);
    }

    memset(&s_app_config.wifi, 0, sizeof(s_app_config.wifi));
    memset(&s_app_config.ntp, 0, sizeof(s_app_config.ntp));

    cJSON *wifi_json = cJSON_GetObjectItemCaseSensitive(main_root, "wifi");
    if (wifi_json) {
        cJSON *ssid_json = cJSON_GetObjectItemCaseSensitive(wifi_json, "ssid");
        cJSON *pass_json = cJSON_GetObjectItemCaseSensitive(wifi_json, "password");
        if (cJSON_IsString(ssid_json) && ssid_json->valuestring && strlen(ssid_json->valuestring) > 0) {
            strncpy(s_app_config.wifi.ssid, ssid_json->valuestring, MAX_SSID_LEN);
        } else {
            ESP_LOGE(TAG, "WiFi SSID not found or empty in config.json. Cannot continue. Halting.");
            cJSON_Delete(main_root);
            while(1) {
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
        }
        if (cJSON_IsString(pass_json) && pass_json->valuestring) {
            strncpy(s_app_config.wifi.password, pass_json->valuestring, MAX_PASSWORD_LEN);
        } else {
             ESP_LOGW(TAG, "WiFi password not found or not a string in config.json. Assuming empty password.");
             s_app_config.wifi.password[0] = '\0';
        }
    } else {
        ESP_LOGE(TAG, "WiFi configuration section not found in config.json. Cannot continue. Halting.");
        cJSON_Delete(main_root);
        while(1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    cJSON *ntp_json = cJSON_GetObjectItemCaseSensitive(main_root, "ntp");
    if (ntp_json) {
        cJSON *ntp_server_json = cJSON_GetObjectItemCaseSensitive(ntp_json, "ntp_server");
        if (cJSON_IsString(ntp_server_json) && ntp_server_json->valuestring && strlen(ntp_server_json->valuestring) > 0) {
            strncpy(s_app_config.ntp.ntp_servers[0], ntp_server_json->valuestring, MAX_NTP_SERVER_LEN);
            s_app_config.ntp.num_ntp_servers = 1;
        } else if (cJSON_IsArray(ntp_server_json)) {
            s_app_config.ntp.num_ntp_servers = 0;
            int array_size = cJSON_GetArraySize(ntp_server_json);
            for (int i = 0; i < array_size && i < MAX_NTP_SERVERS_CONFIG; ++i) {
                cJSON *item = cJSON_GetArrayItem(ntp_server_json, i);
                if (cJSON_IsString(item) && item->valuestring && strlen(item->valuestring) > 0) {
                    strncpy(s_app_config.ntp.ntp_servers[s_app_config.ntp.num_ntp_servers], item->valuestring, MAX_NTP_SERVER_LEN);
                    s_app_config.ntp.num_ntp_servers++;
                }
            }
            if (s_app_config.ntp.num_ntp_servers == 0) {
                 ESP_LOGW(TAG, "NTP server array in config was empty or invalid. Using default: pool.ntp.org");
                 strncpy(s_app_config.ntp.ntp_servers[0], "pool.ntp.org", MAX_NTP_SERVER_LEN);
                 s_app_config.ntp.num_ntp_servers = 1;
            }
        } else {
            ESP_LOGW(TAG, "NTP server not found or invalid in config.json. Using default: pool.ntp.org");
            strncpy(s_app_config.ntp.ntp_servers[0], "pool.ntp.org", MAX_NTP_SERVER_LEN);
            s_app_config.ntp.num_ntp_servers = 1;
        }

        cJSON *ntp_tz_json = cJSON_GetObjectItemCaseSensitive(ntp_json, "ntp_tz");
        if (cJSON_IsString(ntp_tz_json) && ntp_tz_json->valuestring && strlen(ntp_tz_json->valuestring) > 0) {
            strncpy(s_app_config.ntp.ntp_tz_display, ntp_tz_json->valuestring, MAX_TZ_LEN);
            util_format_tz_string(s_app_config.ntp.ntp_tz_display, s_app_config.ntp.ntp_tz_formatted, MAX_TZ_LEN);
             if (strcmp(s_app_config.ntp.ntp_tz_formatted, "UTC") == 0) {
                bool is_actually_utc_input = (strcmp(s_app_config.ntp.ntp_tz_display, "UTC") == 0);
                bool is_actually_zero_input = (strcmp(s_app_config.ntp.ntp_tz_display, "0") == 0);
                bool is_valid_hhmm = false;
                if(strlen(s_app_config.ntp.ntp_tz_display) == 5 && (s_app_config.ntp.ntp_tz_display[0] == '+' || s_app_config.ntp.ntp_tz_display[0] == '-') &&
                   isdigit((unsigned char)s_app_config.ntp.ntp_tz_display[1]) && isdigit((unsigned char)s_app_config.ntp.ntp_tz_display[2]) &&
                   isdigit((unsigned char)s_app_config.ntp.ntp_tz_display[3]) && isdigit((unsigned char)s_app_config.ntp.ntp_tz_display[4])) {
                    char temp_hh[3] = {s_app_config.ntp.ntp_tz_display[1], s_app_config.ntp.ntp_tz_display[2], '\0'};
                    char temp_mm[3] = {s_app_config.ntp.ntp_tz_display[3], s_app_config.ntp.ntp_tz_display[4], '\0'};
                    if (atoi(temp_mm) < 60 && atoi(temp_hh) < 24) is_valid_hhmm = true;
                }
                if (!is_actually_utc_input && !is_actually_zero_input && !is_valid_hhmm) {
                     ESP_LOGW(TAG, "Invalid display TZ string '%s' in config resulted in 'UTC'. Updating display string to 'UTC' as well.", s_app_config.ntp.ntp_tz_display);
                    strncpy(s_app_config.ntp.ntp_tz_display, "UTC", MAX_TZ_LEN);
                }
            }
        } else {
             ESP_LOGW(TAG, "NTP TZ not found or invalid in config.json. Using default: UTC");
        }
    } else {
        ESP_LOGW(TAG, "NTP configuration section not found in config.json. Using defaults for NTP.");
    }

    cJSON_Delete(main_root);
    s_app_config.is_loaded = true;
    ESP_LOGI(TAG, "Configuration loaded successfully.");
    ESP_LOGI(TAG, "WiFi SSID: %s", s_app_config.wifi.ssid);
    ESP_LOGI(TAG, "NTP Servers (%d): %s", s_app_config.ntp.num_ntp_servers, s_app_config.ntp.ntp_servers[0]);
    ESP_LOGI(TAG, "NTP TZ Display: %s, Formatted: %s", s_app_config.ntp.ntp_tz_display, s_app_config.ntp.ntp_tz_formatted);

    return ESP_OK;
}

const app_config_t* get_app_config(void) {
    return &s_app_config;
}
