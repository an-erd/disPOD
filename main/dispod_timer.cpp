#include "esp_log.h"

#include "dispod_main.h"

static const char* TAG = "DISPOD_TIMER";

EventGroupHandle_t dispod_timer_evg;


void dispod_timer_task(void *pvParameters)
{
    EventBits_t     uxBits;
    static int      pixelNumber = 0;
    static uint32_t color_on = pixels.Color(255, 255, 255);

    ESP_LOGI(TAG, "dispod_timer_task: started");

    for (;;)
    {
        uxBits = xEventGroupWaitBits(dispod_timer_evg, DISPOD_TIMER_METRONOME_BIT ,pdTRUE, pdFALSE, portMAX_DELAY);

        if(uxBits & DISPOD_TIMER_METRONOME_BIT){
            ESP_LOGV(TAG, "dispod_timer_task: DISPOD_TIMER_METRONOME_BIT");

            if((xEventGroupWaitBits(dispod_event_group, DISPOD_METRO_SOUND_ACT_BIT, pdFALSE, pdFALSE, 0) & DISPOD_METRO_SOUND_ACT_BIT)){
                ESP_LOGV(TAG, "dispod_timer_task: DISPOD_METRO_SOUND_ACT_BIT");
                M5.Speaker.beep();
            }

            if((xEventGroupWaitBits(dispod_event_group, DISPOD_METRO_LIGHT_ACT_BIT, pdFALSE, pdFALSE, 0) & DISPOD_METRO_LIGHT_ACT_BIT)){
                ESP_LOGV(TAG, "dispod_timer_task: DISPOD_METRO_LIGHT_ACT_BIT");
                // pixels.setPixelColor(pixelNumber, pixels.Color(0, 0, 0));
                // pixelNumber = (pixelNumber + 1) % 10;
                // pixels.setPixelColor(pixelNumber, pixels.Color(255, 255, 255));
                if(pixelNumber%2){
                    pixels.clear();
                } else {
                    pixels.fill(color_on, 0, 10);
                }
                pixelNumber++;
                pixels.show();
            }
        }
    }
}
