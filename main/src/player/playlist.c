#include "playlist.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include <unistd.h>

static const char *TAG = "PLAYLIST";

typedef struct {
    long inf_offset;
} playlist_index_entry_t;

static playlist_index_entry_t *s_channel_index = NULL;
static int s_channel_count = 0;
static int s_channel_capacity = 0;
static bool s_playlist_initialized = false;

static const char *PLAYLIST_URL = "https://raw.githubusercontent.com/RootShell-coder/pcradio.m3u/refs/heads/master/pcradio.m3u";
static const char *PLAYLIST_FILENAME = "/storage/pcradio.m3u";

#define INITIAL_CAPACITY 4500
#define MAX_LINE_LENGTH 1024

static esp_err_t add_channel_to_index(long inf_offset) {
    if (s_channel_count >= s_channel_capacity) {
        int new_capacity = (s_channel_capacity == 0) ? INITIAL_CAPACITY : s_channel_capacity * 2;
        playlist_index_entry_t* new_index = heap_caps_malloc(new_capacity * sizeof(playlist_index_entry_t), MALLOC_CAP_SPIRAM);
        if (!new_index) {
            ESP_LOGE(TAG, "Failed to allocate memory for channel index in PSRAM");
            return ESP_ERR_NO_MEM;
        }
        if (s_channel_index) {
            memcpy(new_index, s_channel_index, s_channel_count * sizeof(playlist_index_entry_t));
            heap_caps_free(s_channel_index);
        }
        s_channel_index = new_index;
        s_channel_capacity = new_capacity;
    }

    s_channel_index[s_channel_count].inf_offset = inf_offset;
    s_channel_count++;
    return ESP_OK;
}

static esp_err_t download_playlist_file() {
    ESP_LOGI(TAG, "Starting playlist download from %s", PLAYLIST_URL);
    esp_http_client_config_t config = {
        .url = PLAYLIST_URL,
        .use_global_ca_store = false,
        .skip_cert_common_name_check = true,
        .timeout_ms = 30000,
        .buffer_size = 2048,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return ESP_FAIL;
    }

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    int content_length = esp_http_client_fetch_headers(client);
    int status_code = esp_http_client_get_status_code(client);

    if (status_code != 200) {
        ESP_LOGE(TAG, "HTTP GET request failed with status code: %d", status_code);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "HTTP GET status = %d, content_length = %d", status_code, content_length);

    FILE* f = fopen(PLAYLIST_FILENAME, "wb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing: %s", PLAYLIST_FILENAME);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    char buffer[1024];
    int total_read_len = 0;
    int read_len;
    err = ESP_OK;

    ESP_LOGI(TAG, "Downloading playlist body...");
    while (true) {
        read_len = esp_http_client_read(client, buffer, sizeof(buffer));
        if (read_len < 0) {
            ESP_LOGE(TAG, "Error during HTTP read: %s", esp_err_to_name(esp_http_client_get_errno(client)));
            err = ESP_FAIL;
            break;
        }
        if (read_len == 0) {
             if (esp_http_client_is_complete_data_received(client)) {
                ESP_LOGI(TAG, "HTTP stream finished and data completely received.");
            } else {
                ESP_LOGW(TAG, "HTTP stream finished, but data might be incomplete.");
            }
            break;
        }
        if (fwrite(buffer, 1, read_len, f) != read_len) {
            ESP_LOGE(TAG, "Error writing to file %s", PLAYLIST_FILENAME);
            err = ESP_FAIL;
            break;
        }
        total_read_len += read_len;
    }

    ESP_LOGI(TAG, "Flushing data to flash storage...");
    fflush(f);
    int fd = fileno(f);
    if (fsync(fd) == -1) {
        ESP_LOGW(TAG, "fsync failed. Data might not be fully persisted on flash.");
    }

    fclose(f);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Playlist download failed. Deleting partial file.");
        remove(PLAYLIST_FILENAME);
        return err;
    }

    if (content_length > 0 && total_read_len != content_length) {
        ESP_LOGW(TAG, "Downloaded size mismatch. Read: %d, Expected: %d. File might be incomplete.", total_read_len, content_length);
    }


    ESP_LOGI(TAG, "Playlist downloaded successfully to %s (%d bytes)", PLAYLIST_FILENAME, total_read_len);
    return ESP_OK;
}

static esp_err_t index_playlist_file() {
    ESP_LOGI(TAG, "Indexing playlist file: %s", PLAYLIST_FILENAME);
    FILE* f = fopen(PLAYLIST_FILENAME, "r");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open playlist file for indexing: %s", PLAYLIST_FILENAME);
        return ESP_FAIL;
    }

    char line[MAX_LINE_LENGTH];
    long current_offset = 0;

    if (s_channel_index) {
        heap_caps_free(s_channel_index);
        s_channel_index = NULL;
    }
    s_channel_count = 0;
    s_channel_capacity = 0;


    current_offset = ftell(f);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "#EXTINF:", strlen("#EXTINF:")) == 0) {
            esp_err_t add_err = add_channel_to_index(current_offset);
            if (add_err != ESP_OK) {
                fclose(f);
                return add_err;
            }
        }
        current_offset = ftell(f);
    }

    fclose(f);
    ESP_LOGI(TAG, "Playlist indexed. Found %d channels.", s_channel_count);
    if (s_channel_count == 0) {
        ESP_LOGW(TAG, "No channels found during indexing. Playlist might be empty or malformed.");
    }
    return ESP_OK;
}

esp_err_t playlist_update_sync(void) {
    ESP_LOGI(TAG, "Force updating playlist...");

    esp_err_t err_download = download_playlist_file();
    if (err_download != ESP_OK) {
        ESP_LOGE(TAG, "Failed to download new playlist during update.");
        return err_download;
    }

    esp_err_t err_index = index_playlist_file();
    if (err_index != ESP_OK) {
        ESP_LOGE(TAG, "Failed to index new playlist during update.");
        return err_index;
    }

    ESP_LOGI(TAG, "Playlist updated and re-indexed successfully.");
    s_playlist_initialized = true;
    return ESP_OK;
}

static void playlist_update_task_runner(void *pvParameters) {
    ESP_LOGI(TAG, "Playlist update task started on core %d", xPortGetCoreID());

    playlist_update_sync();

    vTaskDelete(NULL);
}

esp_err_t playlist_update(void) {
    ESP_LOGI(TAG, "Requesting playlist update. Creating background task on core 0.");

    BaseType_t task_created = xTaskCreatePinnedToCore(
        playlist_update_task_runner,
        "playlist_update",
        8192,
        NULL,
        1,
        NULL,
        0
    );

    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create playlist update task.");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t playlist_init(void) {
    if (s_playlist_initialized) {
        ESP_LOGI(TAG, "Playlist module already initialized (index exists).");
        return ESP_OK;
    }

    FILE* f_check = fopen(PLAYLIST_FILENAME, "r");
    if (f_check) {
        fclose(f_check);
        ESP_LOGI(TAG, "Playlist file %s found. Proceeding to index.", PLAYLIST_FILENAME);
    } else {
        ESP_LOGI(TAG, "Playlist file %s not found. Downloading...", PLAYLIST_FILENAME);
        esp_err_t err_download = download_playlist_file();
        if (err_download != ESP_OK) {
            ESP_LOGE(TAG, "Failed to download playlist.");
            return err_download;
        }
    }

    esp_err_t err_index = index_playlist_file();
    if (err_index != ESP_OK) {
        ESP_LOGE(TAG, "Failed to index playlist.");
        playlist_cleanup();
        return err_index;
    }

    s_playlist_initialized = true;
    return ESP_OK;
}

static char *strdup_psram(const char *str) {
    if (!str) {
        return NULL;
    }
    size_t len = strlen(str) + 1;
    char *new_str = heap_caps_malloc(len, MALLOC_CAP_SPIRAM);
    if (new_str) {
        memcpy(new_str, str, len);
    } else {
        ESP_LOGE(TAG, "Failed to allocate memory for string in PSRAM");
    }
    return new_str;
}

esp_err_t playlist_get_channel_data(int channel_number, playlist_channel_data_t *channel_data) {
    int array_index = channel_number - 1;

    if (!s_playlist_initialized || array_index < 0 || array_index >= s_channel_count || !channel_data) {
        if (channel_data) {
            memset(channel_data, 0, sizeof(playlist_channel_data_t));
        }
        return ESP_ERR_NOT_FOUND;
    }

    memset(channel_data, 0, sizeof(playlist_channel_data_t));

    FILE* f = fopen(PLAYLIST_FILENAME, "r");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open playlist file for reading channel data: %s", PLAYLIST_FILENAME);
        return ESP_FAIL;
    }

    if (fseek(f, s_channel_index[array_index].inf_offset, SEEK_SET) != 0) {
        ESP_LOGE(TAG, "Failed to seek to channel offset %ld for channel number %d (index %d)",
                 s_channel_index[array_index].inf_offset, channel_number, array_index);
        fclose(f);
        return ESP_FAIL;
    }

    char line_buffer[MAX_LINE_LENGTH];
    bool inf_read = false;
    bool url_read = false;

    if (fgets(line_buffer, sizeof(line_buffer), f)) {
        line_buffer[strcspn(line_buffer, "\r\n")] = 0;

        if (strncmp(line_buffer, "#EXTINF:", strlen("#EXTINF:")) == 0) {
            channel_data->inf = strdup_psram(line_buffer + strlen("#EXTINF:"));
            if (!channel_data->inf) goto mem_error;
            inf_read = true;
        } else {
            ESP_LOGE(TAG, "Expected #EXTINF at offset %ld for channel number %d (index %d), but found: %s",
                     s_channel_index[array_index].inf_offset, channel_number, array_index, line_buffer);
            goto read_error;
        }
    } else {
        goto read_error;
    }

    while (fgets(line_buffer, sizeof(line_buffer), f)) {
        line_buffer[strcspn(line_buffer, "\r\n")] = 0;

        if (strncmp(line_buffer, "#EXTVLCOPT:", strlen("#EXTVLCOPT:")) == 0) {
            if (channel_data->opt) {
                ESP_LOGW(TAG, "Multiple #EXTVLCOPT found for one channel, using the first one.");
            } else {
                channel_data->opt = strdup_psram(line_buffer + strlen("#EXTVLCOPT:"));
                if (!channel_data->opt) goto mem_error;
            }
        } else if (strlen(line_buffer) > 0 && line_buffer[0] != '#') {
            channel_data->url = strdup_psram(line_buffer);
            if (!channel_data->url) goto mem_error;
            url_read = true;
            break;
        } else if (strncmp(line_buffer, "#EXTINF:", strlen("#EXTINF:")) == 0) {
            ESP_LOGW(TAG, "Found next #EXTINF before URL for current channel number %d (index %d)", channel_number, array_index);
            break;
        }
    }

    fclose(f);

    if (!inf_read || !url_read) {
        ESP_LOGE(TAG, "Failed to read complete channel data for channel number %d (index %d) (inf_read: %d, url_read: %d)",
                 channel_number, array_index, inf_read, url_read);
        playlist_free_channel_data(channel_data);
        return ESP_FAIL;
    }

    return ESP_OK;

mem_error:
    ESP_LOGE(TAG, "Memory allocation failed while reading channel data.");
read_error:
    if (f) fclose(f);
    playlist_free_channel_data(channel_data);
    return ESP_ERR_NO_MEM;
}

void playlist_free_channel_data(playlist_channel_data_t *channel_data) {
    if (channel_data) {
        heap_caps_free(channel_data->inf);
        channel_data->inf = NULL;
        heap_caps_free(channel_data->opt);
        channel_data->opt = NULL;
        heap_caps_free(channel_data->url);
        channel_data->url = NULL;
    }
}

int playlist_get_channel_count(void) {
    return s_playlist_initialized ? s_channel_count : 0;
}

void playlist_log_channel_info(int channel_number) {
    if (!s_playlist_initialized) {
        ESP_LOGE(TAG, "Playlist not initialized. Cannot log channel info.");
        return;
    }
    if (channel_number <= 0 || channel_number > s_channel_count) {
        ESP_LOGW(TAG, "Channel number %d is out of bounds (total channels: %d).", channel_number, s_channel_count);
        return;
    }

    playlist_channel_data_t channel_data;
    esp_err_t err = playlist_get_channel_data(channel_number, &channel_data);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "--- Channel %d Info ---", channel_number);
        ESP_LOGI(TAG, "INF: %s", channel_data.inf ? channel_data.inf : "N/A");
        ESP_LOGI(TAG, "OPT: %s", channel_data.opt ? channel_data.opt : "N/A");
        ESP_LOGI(TAG, "URL: %s", channel_data.url ? channel_data.url : "N/A");
        ESP_LOGI(TAG, "----------------------");
        playlist_free_channel_data(&channel_data);
    } else {
        ESP_LOGE(TAG, "Failed to get data for channel %d: %s", channel_number, esp_err_to_name(err));
    }
}

void playlist_cleanup(void) {
    if (s_channel_index) {
        heap_caps_free(s_channel_index);
        s_channel_index = NULL;
    }
    s_channel_count = 0;
    s_channel_capacity = 0;
    s_playlist_initialized = false;
    ESP_LOGI(TAG, "Playlist index cleaned up.");
}
