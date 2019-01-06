#include "esp_log.h"

#include "dispod_main.h"
#include "dispod_update.h"
#include "dispod_runvalues.h"
#include "dispod_archiver.h"

static const char* TAG = "DISPOD_UPDATER";

void dispod_check_and_update_display()
{
    if(dispod_runvalues_get_update_display_available(&running_values)){
        running_values.update_display_available = false;
        xEventGroupSetBits(dispod_display_evg, DISPOD_DISPLAY_UPDATE_BIT);
    }
}

void dispod_update_task(void *pvParameters)
{
    ESP_LOGI(TAG, "dispod_update_task: started");
    running_values_queue_element_t new_queue_element;

    for(;;) {
        if(xQueueReceive(running_values_queue, (void * )&new_queue_element, (portTickType)portMAX_DELAY)) {
            switch(new_queue_element.id) {
            case ID_RSC:
	            ESP_LOGI(TAG, "received from queue: 0x2a53: C %3u", new_queue_element.data.rsc.cadance);
                dispod_runvalues_update_RSCValues(&running_values, new_queue_element.data.rsc.cadance);
                dispod_runvalues_calculate_display_values(&running_values);
                dispod_archiver_add_RSCValues(new_queue_element.data.rsc.cadance);
                dispod_check_and_update_display();
                break;
            case ID_CUSTOM:
    	        ESP_LOGI(TAG, "received from queue: 0xFF00: Str %1u Sta %4u", new_queue_element.data.custom.str, new_queue_element.data.custom.GCT);
                dispod_runvalues_update_customValues(&running_values, new_queue_element.data.custom.GCT, new_queue_element.data.custom.str);
                dispod_runvalues_calculate_display_values(&running_values);
                dispod_archiver_add_customValues(new_queue_element.data.custom.GCT, new_queue_element.data.custom.str);
                dispod_check_and_update_display();
                break;
            default:
                ESP_LOGI(TAG, "unknown event id");
                break;
            }
        }
    }
}
