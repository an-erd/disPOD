#include "esp_log.h"

#include "dispod_main.h"

static const char* TAG = "DISPOD_TIMER";

EventGroupHandle_t dispod_timer_evg;


void dispod_timer_task(void *pvParameters)
{
    EventBits_t     uxBits;
    static int      pixelNumber = 0;
#ifdef ADAFRUIT_NEOPIXEL
    static uint32_t color_on = pixels.Color(255, 255, 255);
#endif
#ifdef NEOPIXELBUS
    RgbColor NEOPIXEL_white(colorSaturation);
    RgbColor NEOPIXEL_black(0);
#endif

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
#ifdef ADAFRUIT_NEOPIXEL
                    pixels.clear();
#endif
#ifdef NEOPIXELBUS
                    pixels.ClearTo(NEOPIXEL_black);
#endif
                } else {
#ifdef ADAFRUIT_NEOPIXEL
                    pixels.fill(color_on, 0, 10);
#endif
#ifdef NEOPIXELBUS
                    pixels.ClearTo(NEOPIXEL_white);
#endif
                }
                pixelNumber++;

#ifdef ADAFRUIT_NEOPIXEL
                pixels.show();
#endif
#ifdef NEOPIXELBUS
                pixels.Show();
#endif
            }
        }
    }
}
