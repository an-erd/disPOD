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

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about one event
 * - are we connected to the AP with an IP? */
const int WIFI_CONNECTED_BIT = BIT0;

static const char* TAG = "DISPOD_WIFI";

static int s_retry_num = 0;

const int SCAN_BIT = BIT0;
const int STRT_BIT = BIT1;
const int DISC_BIT = BIT2;
const int DHCP_BIT = BIT3;

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
    wifi_config_t wifi_config;

    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        xEventGroupSetBits(dispod_event_group, DISPOD_WIFI_CONNECTING_BIT);

        dispod_screen_status_update_wifi(&dispod_screen_status, WIFI_CONNECTING, "");
        xEventGroupSetBits(dispod_display_evg, DISPOD_DISPLAY_UPDATE_BIT);

        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        ESP_LOGI(TAG, "got ip:%s",
                 ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);

        esp_wifi_get_config(WIFI_IF_STA, &wifi_config);
        dispod_screen_status_update_wifi(&dispod_screen_status, WIFI_CONNECTED, (char*) wifi_config.sta.ssid);
        xEventGroupClearBits(dispod_event_group, DISPOD_WIFI_CONNECTING_BIT);
        xEventGroupSetBits(dispod_event_group, DISPOD_WIFI_CONNECTED_BIT);
        xEventGroupSetBits(dispod_display_evg, DISPOD_DISPLAY_UPDATE_BIT);

        ESP_ERROR_CHECK(esp_event_post_to(dispod_loop_handle, WORKFLOW_EVENTS, DISPOD_WIFI_GOT_IP_EVT, NULL, 0, portMAX_DELAY));
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        {
            if (s_retry_num < CONFIG_WIFI_MAXIMUM_RETRY) {
                esp_wifi_connect();
                xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
                dispod_screen_status_update_wifi(&dispod_screen_status, WIFI_NOT_CONNECTED, "");
                xEventGroupClearBits(dispod_event_group, DISPOD_WIFI_CONNECTED_BIT);
                xEventGroupSetBits(dispod_display_evg, DISPOD_DISPLAY_UPDATE_BIT);
                s_retry_num++;
                ESP_LOGI(TAG,"retry to connect to the AP");
            }
            ESP_LOGI(TAG,"connect to the AP fail\n");
            break;
        }
    default:
        break;
    }
    return ESP_OK;
}

void wifi_init_sta()
{
    s_wifi_event_group = xEventGroupCreate();

    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL) );

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid       = CONFIG_WIFI_AP1_SSID,
            .password   = CONFIG_WIFI_AP1_PASSWORD
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");
    // ESP_LOGI(TAG, "connect to ap SSID:%s password:%s", CONFIG_WIFI_AP1_SSID, CONFIG_WIFI_AP1_PASSWORD);
}

/*
// code segment from multi WiFi handler:
static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    EventGroupHandle_t evg = (EventGroupHandle_t)ctx;

    switch(event->event_id) {
        case SYSTEM_EVENT_STA_START:
            ESP_LOGD(TAG, "started");
            xEventGroupSetBits(evg, STRT_BIT);
            ESP_ERROR_CHECK(esp_event_post_to(dispod_loop_handle, ACTIVITY_EVENTS, DISPOD_WIFI_SCANNING_EVT, NULL, 0, portMAX_DELAY));
            break;
        case SYSTEM_EVENT_SCAN_DONE:
            ESP_LOGD(TAG, "Scan done");
            // TODO if no know WiFi is available
            ESP_ERROR_CHECK(esp_event_post_to(dispod_loop_handle, ACTIVITY_EVENTS, DISPOD_WIFI_CONNECTING_EVT, NULL, 0, portMAX_DELAY));
            xEventGroupSetBits(evg, SCAN_BIT);
            break;
        case SYSTEM_EVENT_STA_CONNECTED:
            ESP_LOGD(TAG, "connected");
            // our DISPOD_WIFI_CONNECTED is including an IP address, so ignore this step
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            ESP_LOGD(TAG, "dhcp");
            ESP_ERROR_CHECK(esp_event_post_to(dispod_loop_handle, ACTIVITY_EVENTS, DISPOD_WIFI_CONNECTED_EVT, NULL, 0, portMAX_DELAY));
            xEventGroupSetBits(evg, DHCP_BIT);
            break;
        case SYSTEM_EVENT_STA_STOP:
            ESP_LOGD(TAG, "stopped");
            // TODO STA_STOP and STA_DISCONNECTED are different things
            ESP_ERROR_CHECK(esp_event_post_to(dispod_loop_handle, ACTIVITY_EVENTS, DISPOD_WIFI_DISCONNECTED_EVT, NULL, 0, portMAX_DELAY));
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            ESP_LOGD(TAG, "disconnected");
            ESP_ERROR_CHECK(esp_event_post_to(dispod_loop_handle, ACTIVITY_EVENTS, DISPOD_WIFI_DISCONNECTED_EVT, NULL, 0, portMAX_DELAY));
            xEventGroupSetBits(evg, DISC_BIT);
            break;
        default:
            ESP_LOGW(TAG, "Unhandled event (%d)", event->event_id);
            break;
    }
    return ESP_OK;
}

esp_err_t dispod_wifi_network_up()
{
    esp_err_t error;
    uint16_t apCount;
    wifi_ap_record_t *list = NULL;
    int i = 0, j, f, match;
    // int retry = 0;
    wifi_config_t wifi_config;

    EventGroupHandle_t evg = xEventGroupCreate();
    wifi_scan_config_t scanConf =
        { .ssid = NULL, .bssid = NULL, .channel = 0, .show_hidden = true };
    tcpip_adapter_init();
    error = esp_event_loop_init(event_handler, evg);
    if (error != ESP_OK) return error;
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_set_mode(WIFI_MODE_STA);
    xEventGroupClearBits(evg, STRT_BIT);
    esp_wifi_start();

    int state = 0;
    int done = 0;
    while (!done) {
        switch (state) {
        case 0:
            while (!(xEventGroupWaitBits(evg, STRT_BIT , pdFALSE, pdFALSE, portMAX_DELAY) & STRT_BIT));
            xEventGroupClearBits(evg, SCAN_BIT);
            ESP_ERROR_CHECK(esp_wifi_scan_start(&scanConf, 0));
            while (!(xEventGroupWaitBits(evg, SCAN_BIT , pdFALSE, pdFALSE, portMAX_DELAY) & SCAN_BIT));
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
            if (!match) break;
            state = 1;
            strncpy((char *)wifi_config.sta.ssid, (char *)list[i-1].ssid, 32);
            strncpy((char *)wifi_config.sta.password, known_aps[j-1].passwd, 64);
            ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", (char *)wifi_config.sta.ssid);
            esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
            xEventGroupClearBits(evg, DISC_BIT | DHCP_BIT);
            esp_wifi_connect();
            ESP_LOGI(TAG, "Waiting for IP address...");
            state = 2;
        case 2:
            f = xEventGroupWaitBits(evg, DHCP_BIT|DISC_BIT , pdFALSE, pdFALSE, portMAX_DELAY);
            if (f & DISC_BIT) state = 1;
            done = f & DHCP_BIT;
        }
    }
    free(list);
    return ESP_OK;
}
*/
