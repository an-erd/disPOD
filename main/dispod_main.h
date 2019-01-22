#ifndef __DISPOD_MAIN_H__
#define __DISPOD_MAIN_H__

#include "sdkconfig.h"
#include "esp_attr.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "esp_err.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "driver/ledc.h"
#include <M5Stack.h>
#include <NeoPixelBus.h>
#include "dispod_button.h"
#include "dispod_runvalues.h"
#include "dispod_tft.h"
#include "dispod_timer.h"

// disPOD event group
#define DISPOD_WIFI_ACTIVATED_BIT                   (BIT0)      // WiFi is activated
#define DISPOD_WIFI_SCANNING_BIT                    (BIT1)      // WiFi scanning for APs
#define DISPOD_WIFI_CONNECTING_BIT	                (BIT2)      // WiFi connecting to appropriate AP
#define DISPOD_WIFI_CONNECTED_BIT		            (BIT3)      // WiFi got IP
#define DISPOD_WIFI_RETRY_BIT                       (BIT4)      // Try WiFi (and NTP if necessary) again
#define DISPOD_NTP_ACTIVATED_BIT                    (BIT5)      // NTP is activated
#define DISPOD_NTP_UPDATING_BIT    	                (BIT6)      // NTP update process running
#define DISPOD_NTP_UPDATED_BIT			            (BIT7)      // NTP time set
#define DISPOD_BLE_ACTIVATED_BIT                    (BIT8)      // BLE is activated
#define DISPOD_BLE_SCANNING_BIT     	            (BIT9)      // BLE scanning for devices
#define DISPOD_BLE_CONNECTING_BIT    	            (BIT10)     // BLE connecting to appropriate device
#define DISPOD_BLE_CONNECTED_BIT                    (BIT11)     // BLE connected to device
#define DISPOD_BLE_RETRY_BIT                        (BIT12)     // Try BLE again
#define DISPOD_SD_ACTIVATED_BIT                     (BIT13)     // SD card is activated
#define DISPOD_SD_AVAILABLE_BIT                     (BIT14)     // SD function available
#define DISPOD_METRO_SOUND_ACT_BIT                  (BIT15)     // Metronome w/sound output activated
#define DISPOD_METRO_LIGHT_ACT_BIT                  (BIT16)     // Metronome w/light output activated (both sides)
#define DISPOD_METRO_LIGHT_TOGGLE_ACT_BIT           (BIT17)     // Metronome w/sound output activated (alternate sides)
#define DISPOD_BTN_A_RETRY_WIFI_BIT                 (BIT18)
#define DISPOD_BTN_B_RETRY_BLE_BIT                  (BIT19)
#define DISPOD_BTN_C_CNT_BIT                        (BIT20)
#define DISPOD_RUNNING_SCREEN_BIT                   (BIT21)
extern EventGroupHandle_t dispod_event_group;

// disPOD SD card event group
#define DISPOD_SD_WRITE_COMPLETED_BUFFER_EVT        (BIT0)      // write completed buffers
#define DISPOD_SD_WRITE_ALL_BUFFER_EVT				(BIT1)		// write incompleted buffers, too
#define DISPOD_SD_PROBE_EVT                         (BIT2)      // check availability of card and function -> set Bits for status
#define DISPOD_SD_GENERATE_TESTDATA_EVT             (BIT3)      // fill the buffer array with test data
extern EventGroupHandle_t dispod_sd_evg;

// disPOD Client callback function events
typedef enum {
    // workflow events
    DISPOD_STARTUP_EVT              = 0,    /*!< When the disPOD event loop started, the event comes, last call from app_main() */
    DISPOD_BASIC_INIT_DONE_EVT,             /*!< When the basic init has completed, the event comes */
    DISPOD_SPLASH_AND_NETWORK_INIT_DONE_EVT, //DISPOD_DISPLAY_INIT_DONE_EVT,           /*!< When the display splash etc. (not coded yet) has completed, the event comes */
    DISPOD_WIFI_INIT_DONE_EVT,              /*!< When the WiFi init, not yet completed, the event comes */
    DISPOD_WIFI_GOT_IP_EVT,                 /*!< When the WiFi is connected and got an IP, the event comes */
    DISPOD_NTP_INIT_DONE_EVT,               /*!< When the NTP update (successfull or failed!) completed, the event comes */
    DISPOD_SD_INIT_DONE_EVT,                /*!< When the SD mount/probe/unmount is completed, the event comes */
    DISPOD_BLE_DEVICE_DONE_EVT,             /*!< When the BLE connect (successfull or failed!) completed, the event comes */
    DISPOD_STARTUP_COMPLETE_EVT,            /*!< When the startup sequence was run through, the event comes */
    DISPOD_RETRY_WIFI_EVT,                  /*!< When an retry of WiFi is requested, the event comes */
    DISPOD_RETRY_BLE_EVT,                   /*!< When an retry of BLE is requested, the event comes */
    DISPOD_GO_TO_RUNNING_SCREEN_EVT,        /*!< When starting the running screen is requested, the event comes */
    // activity events
    DISPOD_BUTTON_TAP_EVT,                  /*!< When a button has been TAP event, the event comes */
    DISPOD_BUTTON_2SEC_PRESS_EVT,           /*!< When a button has been pressed for 2s, the event comes */
    DISPOD_BUTTON_5SEC_PRESS_EVT,           /*!< When a button has been pressed for 5s, the event comes */
    //
    DISPOD_EVENT_MAX
} dispod_cb_event_t;

ESP_EVENT_DECLARE_BASE(ACTIVITY_EVENTS);
ESP_EVENT_DECLARE_BASE(WORKFLOW_EVENTS);
extern esp_event_loop_handle_t dispod_loop_handle;

// queue for runnning values
extern const TickType_t xTicksToWait;
extern QueueHandle_t running_values_queue;

// dispod screen data
extern dispod_screen_status_t dispod_screen_status;

// global running values data struct
extern runningValuesStruct_t running_values;

// SD card
#define sdPIN_NUM_MISO 19
#define sdPIN_NUM_MOSI 23
#define sdPIN_NUM_CLK  18
#define sdPIN_NUM_CS   4

// M5Stack NeoPixels
// NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(M5STACK_FIRE_NEO_NUM_LEDS, M5STACK_FIRE_NEO_DATA_PIN);
extern NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> pixels;
#define colorSaturation 128

// Speaker
#define SPEAKER_PIN 25

// UART
#define USE_SERIAL Serial

// OTA error codes and update status
extern const char* otaErrorNames[];

typedef struct {
	bool		chg_;
	bool		otaUpdateStarted_;
	bool		otaUpdateEnd_;
	unsigned int otaUpdateProgress_;
	bool		otaUpdateError_;
	int			otaUpdateErrorNr_;
} otaUpdate_t;

// missing functions
#define __min(a,b) \
    ({ __typeof__ (a) _a = (a); \
        __typeof__ (b) _b = (b); \
        _a < _b ? _a : _b; })

#define __max(a,b) \
    ({ __typeof__ (a) _a = (a); \
        __typeof__ (b) _b = (b); \
        _a > _b ? _a : _b; })

#define __map(x,in_min,in_max,out_min,out_max)\
    ({ __typeof__ (x) _x = (x); \
        __typeof__ (in_min) _in_min = (in_min); \
        __typeof__ (in_max) _in_max = (in_max); \
        __typeof__ (out_min) _out_min = (out_min); \
        __typeof__ (out_max) _out_max = (out_max); \
        (_x - _in_min) * (_out_max - _out_min) / (_in_max - _in_min) + _out_min; })

#endif // __DISPOD_MAIN_H__
