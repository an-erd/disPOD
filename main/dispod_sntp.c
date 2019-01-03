#include <time.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "lwip/apps/sntp.h"
#include "freertos/event_groups.h"
#include "dispod_main.h"
#include "dispod_wifi.h"
#include "dispod_sntp.h"

static const char* TAG = "DISPOD_SNTP";

static void s_initialize_sntp(void);
static void s_obtain_time(void);


void dispod_sntp_check_time()
{
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    // Is time set? If not, tm_year will be (1970 - 1900).
    if (timeinfo.tm_year < (2016 - 1900)) {
        ESP_LOGI(TAG, "Time is not set yet. Getting time over NTP.");

        xEventGroupSetBits(dispod_event_group, DISPOD_NTP_UPDATING_BIT);

        dispod_screen_status_update_ntp(&dispod_screen_status, NTP_UPDATING);
        xEventGroupSetBits(dispod_display_evg, DISPOD_DISPLAY_UPDATE_BIT);

        s_obtain_time();
        // update 'now' variable with current time
        time(&now);
    }

    if (timeinfo.tm_year >= (2016 - 1900)) {
        ESP_LOGI(TAG, "Time set yet.");

        xEventGroupClearBits(dispod_event_group, DISPOD_NTP_UPDATING_BIT);
        xEventGroupSetBits  (dispod_event_group, DISPOD_NTP_UPDATED_BIT);

        dispod_screen_status_update_ntp(&dispod_screen_status, NTP_UPDATED);
        xEventGroupSetBits(dispod_display_evg, DISPOD_DISPLAY_UPDATE_BIT);
    } else {
        ESP_LOGI(TAG, "Time still not set.");

        xEventGroupClearBits(dispod_event_group, DISPOD_NTP_UPDATING_BIT);
        xEventGroupClearBits(dispod_event_group, DISPOD_NTP_UPDATED_BIT);

        dispod_screen_status_update_ntp(&dispod_screen_status, NTP_TIME_NOT_SET);
        xEventGroupSetBits(dispod_display_evg, DISPOD_DISPLAY_UPDATE_BIT);
    }

    char strftime_buf[64];

    // Set timezone to German time and print local time
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current local date/time is: %s", strftime_buf);
}

static void s_initialize_sntp()
{
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();
}

static void s_obtain_time()
{
    s_initialize_sntp();

    // wait for time to be set
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 10;
    while(timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }
}
