#ifndef __DISPOD_WIFI_H__
#define __DISPOD_WIFI_H__


#include "esp_err.h"
#include "freertos/event_groups.h"

// WiFi event group
// EventGroupHandle_t wifi_event_group;     // ready to make a request
// int wifi_retry_num = 0;
// int WIFI_CONNECTED_BIT = BIT1;     // WiFi connected to AP

// WiFi config params
// #define DISPOD_WIFI_SSID1        CONFIG_ESP_WIFI_SSID
// #define DISPOD_ESP_WIFI_PASS        CONFIG_ESP_WIFI_PASSWORD
// #define DISPOD_ESP_MAXIMUM_RETRY    CONFIG_ESP_MAXIMUM_RETRY

// esp_err_t dispod_wifi_network_up();
void wifi_init_sta();
// void initialize_wifi(void);
// void stop_wifi(();

#endif // __DISPOD_WIFI_H__
