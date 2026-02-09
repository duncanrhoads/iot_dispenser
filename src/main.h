#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "esp_netif.h"
#include "esp_http_server.h"



void sta_init();
void ap_init();

esp_err_t enter_config_mode();
esp_err_t open_ap();
httpd_handle_t start_webpage();

void dispense();
esp_err_t connect_wifi();
void go_to_bed();




static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data);

esp_err_t save_data(const char* key, const uint8_t* value, const size_t len);
esp_err_t save_data(const char* key, const int32_t value);
esp_err_t load_data(const char* key, uint8_t* value, size_t len);
esp_err_t load_data(const char* key, int32_t* value);
