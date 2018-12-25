#include "dispod_archiver.h"
#include "dispod_config.h"

// data buffers
static buffer_element_t buffers[CONFIG_SDCARD_NUM_BUFFERS][CONFIG_SDCARD_BUFFER_SIZE];
static uint8_t current_buffer          = 0;
static uint8_t next_buffer_to_write    = 0;
static uint8_t seconds_to_write        = 0;
static uint32_t used_in_current_buffer = 0;

void dispod_archiver_task(void *pvParameters)
{
    ESP_LOGI(TAG, "dispod_archiver_task: started");
    EventBits_t uxBits;
    bool complete;
    // dispod_screen_status_t* params = (dispod_screen_status_t*)pvParameters;

    for (;;)
    {
        // while(!(xEventGroupWaitBits(dispod_display_evg, DISPOD_DISPLAY_UPDATE_BIT,
        //         pdTRUE, pdFALSE, portMAX_DELAY) & DISPOD_DISPLAY_UPDATE_BIT));
        // uxBits = xEventGroupWaitBits(dispod_display_evg, DISPOD_DISPLAY_COMPLETE_UPDATE_BIT,
        //         pdTRUE, pdFALSE, 0);
        // ESP_LOGI(TAG, "uxBits = xEventGroupWaitBits(dispod_display_evg, DISPOD_DISPLAY_COMPLETE_UPDATE_BIT = %u", uxBits);
        complete = (bool) (uxBits & DISPOD_DISPLAY_COMPLETE_UPDATE_BIT);

    }
}