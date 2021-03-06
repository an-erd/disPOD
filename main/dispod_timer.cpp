#include "esp_timer.h"
#include "esp_log.h"

#include "dispod_main.h"
#include "dispod_timer.h"
#include "dispod_ledc.h"

static const char* TAG = "DISPOD_TIMER";
EventGroupHandle_t dispod_timer_evg;

static void IRAM_ATTR timer_metronome_callback(void* arg);
static void IRAM_ATTR timer_heartbeat_callback(void* arg);

// create 4 timer args and handles
// - periodic timer for metronome on/cadence = 180 bpm
// - timer for metronome off/light, one-shot timer fired by periodic timer
// - timer for metronome off/sound, one-shot timer fired by periodic timer
// - timer for heartbeat to have time stamps in the running values log files
typedef enum {
	TIMER_METRONOM_PERIODIC = 0,
	TIMER_METRONOM_OFF_LIGHT,
	TIMER_METRONOM_OFF_SOUND,
	TIMER_HEARTBEAT_PERIODIC,
	MAX_TIMER_NUMBER,
} timer_dispod_id;

static esp_timer_handle_t timer_handles[MAX_TIMER_NUMBER] = { 0 };
static uint8_t			  timer_num    [MAX_TIMER_NUMBER]
	= { TIMER_METRONOM_PERIODIC, TIMER_METRONOM_OFF_LIGHT, TIMER_METRONOM_OFF_SOUND, TIMER_HEARTBEAT_PERIODIC,};


void dispod_timer_initialize()
{
    const esp_timer_create_args_t periodic_timer_metronome_on_args = {
        &timer_metronome_callback,
         (void*) &timer_num[TIMER_METRONOM_PERIODIC],
         ESP_TIMER_TASK,
        "metronome_tick"
    };
    const esp_timer_create_args_t timer_metronome_off_light_args = {
        &timer_metronome_callback,
        (void*) &timer_num[TIMER_METRONOM_OFF_LIGHT],
         ESP_TIMER_TASK,
        "metronome_off_light"
	};
    const esp_timer_create_args_t timer_metronome_off_sound_args = {
        &timer_metronome_callback,
        (void*) &timer_num[TIMER_METRONOM_OFF_SOUND],
        ESP_TIMER_TASK,
        "metronome_off_sound"
    };
    const esp_timer_create_args_t periodic_timer_heartbeat_args = {
        &timer_heartbeat_callback,
        (void*) &timer_num[TIMER_HEARTBEAT_PERIODIC],
        ESP_TIMER_TASK,
        "heartbeat"
    };


	ESP_LOGD(TAG, "dispod_timer_initialize() started");
	ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_metronome_on_args, &timer_handles[TIMER_METRONOM_PERIODIC]));
	ESP_ERROR_CHECK(esp_timer_create(&timer_metronome_off_light_args,   &timer_handles[TIMER_METRONOM_OFF_LIGHT]));
	ESP_ERROR_CHECK(esp_timer_create(&timer_metronome_off_sound_args,   &timer_handles[TIMER_METRONOM_OFF_SOUND]));
	ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_heartbeat_args,   	&timer_handles[TIMER_HEARTBEAT_PERIODIC]));

	// should be empty
    // ESP_ERROR_CHECK(esp_timer_dump(stdout));
}

void dispod_timer_start_metronome()
{
    // Start the metronom_on timer, which will fire the other both one-shot timer
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer_handles[TIMER_METRONOM_PERIODIC], METRONOME_TIMER_US));

	ESP_LOGD(TAG, "dispod_timer_start_metronome(), time since boot: %lld us", esp_timer_get_time());
    // ESP_ERROR_CHECK(esp_timer_dump(stdout));
}

void dispod_timer_stop_metronome()
{
    // Stop the metronom_on timer, we let the on-shot timer go to turn off sound and light
    ESP_ERROR_CHECK(esp_timer_stop(timer_handles[TIMER_METRONOM_PERIODIC]));

	ESP_LOGV(TAG, "dispod_timer_stop_metronome(), time since boot: %lld us", esp_timer_get_time());
    // ESP_ERROR_CHECK(esp_timer_dump(stdout));
}

static void IRAM_ATTR timer_metronome_callback(void* arg)
{
	uint8_t timer_nr = *((uint8_t *) arg);
	EventBits_t     uxBits = 0;
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;	// xHigherPriorityTaskWoken must be initialised to pdFALSE.
	BaseType_t xResult;

    int64_t time_since_boot = esp_timer_get_time();
    ESP_LOGD(TAG, "timer_metronome_callback, time since boot: %lld us", time_since_boot);

	switch(timer_nr){
		case TIMER_METRONOM_PERIODIC:  uxBits = DISPOD_TIMER_METRONOME_ON_BIT;	          break;
		case TIMER_METRONOM_OFF_LIGHT: uxBits = DISPOD_TIMER_METRONOME_OFF_LIGHT_BIT;     break;
		case TIMER_METRONOM_OFF_SOUND: uxBits = DISPOD_TIMER_METRONOME_OFF_SOUND_BIT;     break;
		default: ESP_LOGW(TAG, "timer_metronome_callback, unhandled timer %u", timer_nr); break;
	}

	ESP_LOGD(TAG, "timer_metronome_callback, handled %u", timer_nr);

	xResult = xEventGroupSetBitsFromISR(dispod_timer_evg, uxBits, &xHigherPriorityTaskWoken);

    // message posted successfully?
    if( xResult == pdPASS ){
        portYIELD_FROM_ISR();
    }
}

void dispod_timer_start_heartbeat()
{
	// Start the heartbeat timer
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer_handles[TIMER_HEARTBEAT_PERIODIC], CONFIG_RUNNING_LOGFILE_HEARTBEAT_INTERVAL * 1e+6)); // sec -> usec

	ESP_LOGD(TAG, "dispod_timer_start_heartbeat(), time since boot: %lld us", esp_timer_get_time());
    // ESP_ERROR_CHECK(esp_timer_dump(stdout));
}

void dispod_timer_stop_heartbeat()
{
    ESP_ERROR_CHECK(esp_timer_stop(timer_handles[TIMER_HEARTBEAT_PERIODIC]));

	ESP_LOGD(TAG, "dispod_timer_stop_heartbeat(), time since boot: %lld us", esp_timer_get_time());
    // ESP_ERROR_CHECK(esp_timer_dump(stdout));
}

static void IRAM_ATTR timer_heartbeat_callback(void* arg)
{
	uint8_t timer_nr = *((uint8_t *) arg);
	EventBits_t     uxBits = 0;
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;	// xHigherPriorityTaskWoken must be initialised to pdFALSE.
	BaseType_t xResult;

    int64_t time_since_boot = esp_timer_get_time();
    ESP_LOGD(TAG, "timer_heartbeat_callback, time since boot: %lld us", time_since_boot);

	switch(timer_nr){
		case TIMER_HEARTBEAT_PERIODIC:  uxBits = DISPOD_TIMER_HEARTBEAT_BIT;	          break;
		default: ESP_LOGW(TAG, "timer_heartbeat_callback, unhandled timer %u", timer_nr); break;
	}

	ESP_LOGD(TAG, "timer_heartbeat_callback, handled %u", timer_nr);

	xResult = xEventGroupSetBitsFromISR(dispod_timer_evg, uxBits, &xHigherPriorityTaskWoken);

    // message posted successfully?
    if( xResult == pdPASS ){
        portYIELD_FROM_ISR();
    }
}

void dispod_timer_task(void *pvParameters)
{
    EventBits_t     uxBits;
    static int      pixelNumber = 0; // 0 left, 1 right

    RgbColor NEOPIXEL_white(colorSaturation);
    RgbColor NEOPIXEL_black(0);

    ESP_LOGI(TAG, "dispod_timer_task: started");

    for (;;)
    {
        uxBits = xEventGroupWaitBits(dispod_timer_evg,
					DISPOD_TIMER_METRONOME_ON_BIT | DISPOD_TIMER_METRONOME_OFF_LIGHT_BIT | DISPOD_TIMER_METRONOME_OFF_SOUND_BIT
					| DISPOD_TIMER_HEARTBEAT_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

        if(uxBits & DISPOD_TIMER_METRONOME_ON_BIT){
            ESP_LOGD(TAG, "dispod_timer_task: DISPOD_TIMER_METRONOME_ON_BIT");
			xEventGroupClearBits(dispod_timer_evg, DISPOD_TIMER_METRONOME_ON_BIT);

			// Light on, if activated
            if((xEventGroupWaitBits(dispod_event_group, DISPOD_METRO_LIGHT_ACT_BIT, pdFALSE, pdFALSE, 0) & DISPOD_METRO_LIGHT_ACT_BIT)){
                ESP_LOGD(TAG, "dispod_timer_task: DISPOD_METRO_LIGHT_ACT_BIT");
                pixels.ClearTo(NEOPIXEL_white);
                pixels.Show();
				ESP_ERROR_CHECK(esp_timer_start_once(timer_handles[TIMER_METRONOM_OFF_LIGHT], METRONOME_LIGHT_DURATION_US));
				ESP_LOGD(TAG, "Started timer TIMER_METRONOM_OFF_LIGHT, time since boot: %lld us", esp_timer_get_time());
			    // ESP_LOGI(TAG, "                                                         %lld us", esp_timer_get_time());
            } else if((xEventGroupWaitBits(dispod_event_group, DISPOD_METRO_LIGHT_TOGGLE_ACT_BIT, pdFALSE, pdFALSE, 0) & DISPOD_METRO_LIGHT_TOGGLE_ACT_BIT)){
                ESP_LOGD(TAG, "dispod_timer_task: DISPOD_METRO_LIGHT_TOGGLE_ACT_BIT");
                if(pixelNumber){
                    pixels.ClearTo(NEOPIXEL_white, 0, 4);
                } else {
                    pixels.ClearTo(NEOPIXEL_white, 5, 9);
                }
                pixels.Show();
                pixelNumber = 1 - pixelNumber;
				ESP_ERROR_CHECK(esp_timer_start_once(timer_handles[TIMER_METRONOM_OFF_LIGHT], METRONOME_LIGHT_DURATION_US));
				ESP_LOGD(TAG, "Started timer TIMER_METRONOM_OFF_LIGHT, time since boot: %lld us", esp_timer_get_time());
            }

			// Sound on, if activated
            if((xEventGroupWaitBits(dispod_event_group, DISPOD_METRO_SOUND_ACT_BIT, pdFALSE, pdFALSE, 0) & DISPOD_METRO_SOUND_ACT_BIT)){
                ESP_LOGD(TAG, "dispod_timer_task: DISPOD_METRO_SOUND_ACT_BIT");
                ESP_LOGD(TAG, "beep: vol = %u", dispod_screen_status.volume);
                dispod_beep(dispod_screen_status.volume);
				ESP_ERROR_CHECK(esp_timer_start_once(timer_handles[TIMER_METRONOM_OFF_SOUND], METRONOME_SOUND_DURATION_US));
				ESP_LOGD(TAG, "Started timer TIMER_METRONOM_OFF_SOUND, time since boot: %lld us", esp_timer_get_time());
            }
		}

		// Light off
		if(uxBits & DISPOD_TIMER_METRONOME_OFF_LIGHT_BIT){
            ESP_LOGD(TAG, "dispod_timer_task: DISPOD_TIMER_METRONOME_OFF_LIGHT_BIT");
			xEventGroupClearBits(dispod_timer_evg, DISPOD_TIMER_METRONOME_OFF_LIGHT_BIT);
            pixels.ClearTo(NEOPIXEL_black);
            pixels.Show();
			ESP_LOGD(TAG, "Switched off TIMER_METRONOM_OFF_LIGHT, time since boot: %lld us", esp_timer_get_time());
        }

		// Sound off
		if(uxBits & DISPOD_TIMER_METRONOME_OFF_SOUND_BIT){
            ESP_LOGD(TAG, "dispod_timer_task: DISPOD_TIMER_METRONOME_OFF_SOUND_BIT");
			xEventGroupClearBits(dispod_timer_evg, DISPOD_TIMER_METRONOME_OFF_SOUND_BIT);
            dispod_beep(0);
			ESP_LOGD(TAG, "Switched off TIMER_METRONOME_OFF_SOUND, time since boot: %lld us", esp_timer_get_time());
        }

		// Heartbeat
		if(uxBits & DISPOD_TIMER_HEARTBEAT_BIT){
			time_t now;
			running_values_queue_element_t new_queue_element;
			BaseType_t  xStatus;

            ESP_LOGD(TAG, "dispod_timer_task: DISPOD_TIMER_HEARTBEAT_BIT");

			xEventGroupClearBits(dispod_timer_evg, DISPOD_TIMER_HEARTBEAT_BIT);

			new_queue_element.id = ID_TIME;
			time(&now);
			localtime_r(&now, &new_queue_element.data.time.timeinfo);

			ESP_LOGD(TAG, "queue heartbeat, time since boot: %lld us", esp_timer_get_time());

            bool q_send_fail = pdFALSE;
            uint8_t q_wait = 0;
            xStatus = xQueueSendToBack(running_values_queue, &new_queue_element, xTicksToWait);
            if(xStatus != pdTRUE ){
                ESP_LOGW(TAG, "dispod_timer_task: DISPOD_TIMER_HEARTBEAT_BIT: cannot send to queue");
                q_send_fail = pdTRUE;
            }
            q_wait = uxQueueMessagesWaiting(running_values_queue);
            dispod_screen_status_update_queue(&dispod_screen_status, q_wait, pdTRUE, pdFALSE, q_send_fail);
        }
	}
}
