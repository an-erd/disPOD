#include "esp_log.h"

#include "dispod_main.h"
#include "dispod_update.h"
#include "dispod_runvalues.h"
#include "dispod_archiver.h"

static const char* TAG = "DISPOD_UPDATER";

void dispod_check_and_update_display()
{
    if(dispod_runvalues_get_update_display_available(&running_values)){
        xEventGroupSetBits(dispod_display_evg, DISPOD_DISPLAY_UPDATE_BIT);
    }
}

void dispod_update_task(void *pvParameters)
{
    ESP_LOGI(TAG, "dispod_update_task: started");
    running_values_queue_element_t new_queue_element;
	char strftime_buf[64];

    for(;;) {
        if(xQueueReceive(running_values_queue, (void * )&new_queue_element, (portTickType)portMAX_DELAY)) {
            uint8_t q_wait = uxQueueMessagesWaiting(running_values_queue);
            dispod_screen_status_update_queue(&dispod_screen_status, q_wait, pdFALSE, pdTRUE, pdFALSE);

            switch(new_queue_element.id) {
            case ID_RSC:
	            ESP_LOGD(TAG, "received from queue: 0x2a53: C %3u", new_queue_element.data.rsc.cadance);
                dispod_runvalues_update_RSCValues(&running_values, new_queue_element.data.rsc.cadance);
                dispod_check_and_update_display();
                dispod_archiver_add_RSCValues(new_queue_element.data.rsc.cadance);
                break;
            case ID_CUSTOM:
    	        ESP_LOGD(TAG, "received from queue: 0xFF00: Str %1u Sta %4u", new_queue_element.data.custom.str, new_queue_element.data.custom.GCT);
                dispod_runvalues_update_customValues(&running_values, new_queue_element.data.custom.GCT, new_queue_element.data.custom.str);
                dispod_check_and_update_display();
                dispod_archiver_add_customValues(new_queue_element.data.custom.GCT, new_queue_element.data.custom.str);
                break;
            case ID_TIME:
			    strftime(strftime_buf, sizeof(strftime_buf), "%c", &new_queue_element.data.time.timeinfo);
    	        ESP_LOGD(TAG, "received from queue: TIME: %s", strftime_buf);
                dispod_archiver_add_time(new_queue_element.data.time.timeinfo);
                xEventGroupSetBits(dispod_display_evg, DISPOD_DISPLAY_UPDATE_BIT);
                break;
			default:
                ESP_LOGI(TAG, "unknown event id");
                break;
            }
        }
    }
}
