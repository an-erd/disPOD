#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/event_groups.h"

#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_event_loop.h"

#include "dispod_main.h"
#include "dispod_wifi.h"
#include "dispod_tft.h"

static const char* TAG = "DISPOD_WIFI";

static int s_retry_num = 0;
static EventGroupHandle_t evg;

#define STRT_BIT    (BIT0)
#define SCAN_BIT    (BIT1)
#define DISC_BIT    (BIT2)
#define DHCP_BIT    (BIT3)

struct known_ap {
    char *ssid;
    char *passwd;
};

struct known_ap known_aps[CONFIG_WIFI_AP_COUNT] = {
#ifdef CONFIG_WIFI_AP1
      {.ssid = CONFIG_WIFI_AP1_SSID, .passwd = CONFIG_WIFI_AP1_PASSWORD}
#ifdef CONFIG_WIFI_AP2
    , {.ssid = CONFIG_WIFI_AP2_SSID, .passwd = CONFIG_WIFI_AP2_PASSWORD}
#ifdef CONFIG_WIFI_AP3
    , {.ssid = CONFIG_WIFI_AP3_SSID, .passwd = CONFIG_WIFI_AP3_PASSWORD}
#ifdef CONFIG_WIFI_AP4
    , {.ssid = CONFIG_WIFI_AP4_SSID, .passwd = CONFIG_WIFI_AP4_PASSWORD}
#ifdef CONFIG_WIFI_AP5
    , {.ssid = CONFIG_WIFI_AP5_SSID, .passwd = CONFIG_WIFI_AP5_PASSWORD}
#endif
#endif
#endif
#endif
#endif
};

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    EventGroupHandle_t evg = (EventGroupHandle_t) ctx;
    wifi_config_t wifi_config;

    // see https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/wifi.html#esp32-wi-fi-event-description
    switch(event->event_id) {
        case SYSTEM_EVENT_SCAN_DONE:
            ESP_LOGD(TAG, "SYSTEM_EVENT_SCAN_DONE");
            xEventGroupSetBits(evg, SCAN_BIT);
            break;
        case SYSTEM_EVENT_STA_START:
            ESP_LOGD(TAG, "SYSTEM_EVENT_STA_START");

            xEventGroupSetBits(dispod_event_group, DISPOD_WIFI_SCANNING_BIT);
            dispod_screen_status_update_wifi(&dispod_screen_status, WIFI_SCANNING, "scanning");
            xEventGroupSetBits(dispod_display_evg, DISPOD_DISPLAY_UPDATE_BIT);

            xEventGroupSetBits(evg, STRT_BIT);
            break;
        case SYSTEM_EVENT_STA_STOP:
            ESP_LOGD(TAG, "SYSTEM_EVENT_STA_STOP");
            break;
        case SYSTEM_EVENT_STA_CONNECTED:
            ESP_LOGD(TAG, "SYSTEM_EVENT_STA_CONNECTED");
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            ESP_LOGD(TAG, "SYSTEM_EVENT_STA_DISCONNECTED");
            dispod_screen_status_update_wifi(&dispod_screen_status, WIFI_NOT_CONNECTED, "-");
            xEventGroupClearBits(dispod_event_group, DISPOD_WIFI_CONNECTING_BIT);
            xEventGroupSetBits(dispod_display_evg, DISPOD_DISPLAY_UPDATE_BIT);

            xEventGroupSetBits(evg, DISC_BIT);
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            ESP_LOGD(TAG, "SYSTEM_EVENT_STA_GOT_IP");

            ESP_LOGI(TAG, "got ip:%s", ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
            s_retry_num = 0;

            ESP_ERROR_CHECK(esp_wifi_get_config(WIFI_IF_STA, &wifi_config));
            dispod_screen_status_update_wifi(&dispod_screen_status, WIFI_CONNECTED, (char*) wifi_config.sta.ssid);
            xEventGroupClearBits(dispod_event_group, DISPOD_WIFI_CONNECTING_BIT);
            xEventGroupSetBits(dispod_event_group, DISPOD_WIFI_CONNECTED_BIT);
            xEventGroupSetBits(dispod_display_evg, DISPOD_DISPLAY_UPDATE_BIT);

            // xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
            xEventGroupSetBits(evg, DHCP_BIT);
            ESP_ERROR_CHECK(esp_event_post_to(dispod_loop_handle, WORKFLOW_EVENTS, DISPOD_WIFI_GOT_IP_EVT, NULL, 0, portMAX_DELAY));
            break;
        case SYSTEM_EVENT_STA_LOST_IP:
            ESP_LOGD(TAG, "SYSTEM_EVENT_STA_LOST_IP");
            break;
        default:
            ESP_LOGW(TAG, "Unhandled event (%d)", event->event_id);
            break;
    }
    return ESP_OK;
}


void dispod_wifi_network_init()
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    tcpip_adapter_init();

    evg = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, evg));
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    xEventGroupClearBits(evg, STRT_BIT);
    ESP_ERROR_CHECK(esp_wifi_start());
}

esp_err_t dispod_wifi_network_up()
{
    // esp_err_t error;
    int i = 0, j = 0, f, match;
    // int retry = 0;

    uint16_t apCount;
    wifi_ap_record_t *list = NULL;
    wifi_config_t wifi_config = {0};
    wifi_scan_config_t scanConf = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true
    };

    int state = 0;
    int done = 0;
    while (!done) {
        switch (state) {
        case 0:
            while (!(xEventGroupWaitBits(evg, STRT_BIT , pdFALSE, pdFALSE, portMAX_DELAY) & STRT_BIT));
            ESP_LOGD(TAG, "case 0: xEventGroupWaitBits(evg, STRT_BIT ...); state = %d", state);
            xEventGroupClearBits(evg, SCAN_BIT);

            ESP_ERROR_CHECK(esp_wifi_scan_start(&scanConf, 0));
            while (!(xEventGroupWaitBits(evg, SCAN_BIT , pdFALSE, pdFALSE, portMAX_DELAY) & SCAN_BIT));
            xEventGroupClearBits(dispod_event_group, DISPOD_WIFI_SCANNING_BIT);
            apCount = 0;
            esp_wifi_scan_get_ap_num(&apCount);
            if (!apCount) {
                // retry++;
                break;
            }
            free(list);
            list = (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * apCount);
            ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&apCount, list));
            for (i = 0; i < apCount; i++) {
                ESP_LOGD(TAG, "%d %s", list[i].rssi, (char *)list[i].ssid);
            }
            i = 0;
        case 1:
            state = 0;
            for (match = 0; i < apCount && !match; i++) {
                for (j = 0; j < CONFIG_WIFI_AP_COUNT && !match; j++) {
                    match = !strcasecmp((char *)list[i].ssid, known_aps[j].ssid);
                }
            }
            if (!match) {
                ESP_LOGD(TAG, "case 1: not match, break; state = %d", state);
                break;
            }
            state = 1;

            strncpy((char *)wifi_config.sta.ssid, (char *)list[i-1].ssid, 32);
            strncpy((char *)wifi_config.sta.password, known_aps[j-1].passwd, 64);

            ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", (char *)wifi_config.sta.ssid);
            wifi_config.sta.scan_method = WIFI_FAST_SCAN;

            xEventGroupClearBits(dispod_event_group, DISPOD_WIFI_SCANNING_BIT);
            xEventGroupSetBits(dispod_event_group, DISPOD_WIFI_CONNECTING_BIT);
            dispod_screen_status_update_wifi(&dispod_screen_status, WIFI_CONNECTING, (char *)wifi_config.sta.ssid);
            xEventGroupSetBits(dispod_display_evg, DISPOD_DISPLAY_UPDATE_BIT);

            ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
            xEventGroupClearBits(evg, DISC_BIT | DHCP_BIT);

            ESP_ERROR_CHECK(esp_wifi_connect());
            ESP_LOGI(TAG, "Waiting for IP address...");
            state = 2;
            ESP_LOGD(TAG, "case 1: esp_wifi_connect() called; state = %d", state);
        case 2:
            f = xEventGroupWaitBits(evg, DHCP_BIT|DISC_BIT , pdFALSE, pdFALSE, portMAX_DELAY);
            ESP_LOGD(TAG, "case 2: xEventGroupWaitBits(evg, DHCP_BIT|DISC_BIT...); state = %d, DISC_BIT %d, DHCP_BIT %d", state, f|DISC_BIT, f|DHCP_BIT);
            if (f & DISC_BIT) state = 1;
            done = f & DHCP_BIT;
            ESP_LOGD(TAG, "case 2: xEventGroupWaitBits(evg, DHCP_BIT|DISC_BIT; state = %d, done = %d", state, done);
        }
    }
    free(list);

    return ESP_OK;
}
