#include "dispod_main.h"
#include "dispod_idle_timer.h"

static const char* TAG = "DISPOD_IDLE_TIMER";

static uint32_t s_count = 0;
static uint32_t s_duration = 0;
static bool s_running = false;

void dispod_idle_timer_set(uint32_t duration_ms)
{
    s_duration = duration_ms;
    dispod_touch_timer();
    s_running = true;

    ESP_LOGI(TAG, "dispod_idle_timer_set(): %u", s_duration);
}

void dispod_idle_timer_stop()
{
    s_running = false;
    ESP_LOGI(TAG, "dispod_idle_timer_stop()");
}

void dispod_touch_timer()
{
    s_count = millis() + s_duration;
    ESP_LOGI(TAG, "dispod_touch_timer(): millis = %lu, s_dur = %u, s_count = %u", millis(), s_duration, s_count );
}

bool dispod_idle_timer_expired()
{
    bool expired;

    if(!s_running)
        return false;

    expired = millis() >= s_count;
    if(expired)
        ESP_LOGI(TAG, "dispod_idle_timer_expired(): expired");

    return expired;
}
