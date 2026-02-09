#include "main.h"

static const char *TAG = "WiFi";
static const char *SER = "Server";

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");

const uint64_t micro_conversion_factor = 1000000ULL;

static const httpd_uri_t config_page = { // add other requests to update settings, test basic server works
    .uri = "/",
    .method = HTTP_GET,
    .handler = config_handler,
    .user_ctx = NULL
};


extern "C" void app_main(void) {
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    esp_err_t err;

    if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
        // dispense();
        ESP_LOGI("DATA", "timer wakeup");
        go_to_bed();
    }

    enter_config_mode();
    while (1) { // chill until user starts from config
        vTaskDelay(10000);
    }
}

void sta_init() {
    // 1. Initialize NVS (needed for WiFi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        ret = nvs_flash_init();
    }
    
    // 2. Initialize TCP/IP stack
    esp_netif_init();
    
    // 3. Create event loop
    esp_event_loop_create_default();
    
    // 4. Create WiFi station interface
    esp_netif_create_default_wifi_sta();
    
    // 5. Initialize WiFi with default config
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    
    // 6. Register event handlers
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL);
}

void go_to_bed() {
    uint64_t seconds = 8;
    esp_sleep_enable_timer_wakeup(seconds * micro_conversion_factor);
    esp_deep_sleep_start();
}

httpd_handle_t start_webpage() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) {
        ESP_LOGE(SER, "Http server started successfully");
        return server;
    }

    ESP_LOGE(SER, "Http server failed to start");
    return NULL;
}

static esp_err_t config_handler(httpd_req_t *req) {
    const size_t len = index_html_start - index_html_end;
    httpd_resp_send(req, (char *) index_html_start, len);
    return ESP_OK;
}

esp_err_t enter_config_mode() {
    // go into ap mode
    // set up small web service to change dispense times, wifi ssid pass, etc.

    open_ap();
    httpd_handle_t server = start_webpage();
    
    if (server == NULL) {
        return ESP_FAIL;
    }

    httpd_register_uri_handler(server, &config_page);

    return ESP_OK;
}

void ap_init() {
    // 1. Initialize NVS (needed for WiFi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        ret = nvs_flash_init();
    }

    esp_netif_init(); // look into changing default ip of "192.168.4.1"
    
    esp_event_loop_create_default();

    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT(); 

    esp_wifi_init(&cfg);

    esp_event_handler_instance_register(WIFI_EVENT,ESP_EVENT_ANY_ID,&wifi_event_handler,NULL,NULL);
}

esp_err_t open_ap() {
    ap_init();
    
    uint8_t ssid[32] = {0};
    uint8_t pass[64] = {0};

    esp_err_t err1;
    esp_err_t err2;

    err1 = load_data("ap_ssid", ssid, 32);
    err2 = load_data("ap_password", pass, 64);

    // if eithe load fails, use defaults
    if (err1 != ESP_OK || err2 != ESP_OK) {
        strcpy((char *) ssid, "Not a dispenser");
        strcpy((char *) pass, "Not the password");
    }

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = *ssid,
            .password = *pass,
            .ssid_len = strlen((char *) ssid) 
        },
    };

    
    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI("DATA", "AP set up with ssid: '%s', pass: '%s'", ssid, pass);

    return ESP_OK;
}

esp_err_t connect_wifi() {
    uint8_t ssid[32] = {0};
    uint8_t pass[64] = {0};


    ESP_ERROR_CHECK(load_data("wifi_ssid", ssid, 32));
    ESP_ERROR_CHECK(load_data("wifi_password", pass, 64));
  

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = *ssid,
            .password = *pass, 
        },
    };

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    
    ESP_ERROR_CHECK(esp_wifi_start());
    return ESP_OK;
}


static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data) {
    if (event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Disconnected from wifi, retrying...");
        esp_wifi_connect();
    } else if (event_id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "Got IP! WiFi connected!");
    } else if (event_id = WIFI_EVENT_AP_STACONNECTED) {
        
        ESP_LOGI(TAG, "Sta connected");
    }
}


// is there a way to check if data type is correct
esp_err_t save_data(const char* key, const uint8_t* value, const size_t len) {
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open("storage", NVS_READWRITE, &nvs_handle);

    if (err != ESP_OK) return err;

    nvs_set_blob(nvs_handle, key, value, len);

    nvs_commit(nvs_handle);

    ESP_LOGI("DATA", "%s saved with key %s", value, key);
    return ESP_OK;
}

esp_err_t load_data(const char* key, uint8_t* value, size_t len) {
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open("storage", NVS_READONLY, &nvs_handle);

    if (err != ESP_OK) return err;

    err = nvs_get_blob(nvs_handle, key, value, &len);
    if (err != ESP_OK) return err;


    ESP_LOGI("DATA", "%s loaded with key %s", value, key);
    return ESP_OK;
}

esp_err_t save_data(const char* key, const int32_t value) {
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open("storage", NVS_READWRITE, &nvs_handle);

    if (err != ESP_OK) return err;

    nvs_set_i32(nvs_handle, key, value);

    nvs_commit(nvs_handle);

    ESP_LOGI("DATA", "%s saved with key %s", value, key);
    return ESP_OK;
}

esp_err_t load_data(const char* key, int32_t* value) {
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open("storage", NVS_READONLY, &nvs_handle);

    if (err != ESP_OK) return err;

    err = nvs_get_i32(nvs_handle, key, value);
    if (err != ESP_OK) return err;

    ESP_LOGI("DATA", "%s loaded with key %s", value, key);
    return ESP_OK;
}