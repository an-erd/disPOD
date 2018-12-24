#ifndef __DISPOD_CONFIG_H__
#define __DISPOD_CONFIG_H__

#include "esp_event.h"
#include "esp_event_loop.h"
#include "esp_err.h"
#include "freertos/event_groups.h"
#include "dispod_runvalues.h"
#include "dispod_tft.h"

// disPOD event group
#define DISPOD_WIFI_ACTIVATED_BIT                   (BIT0)
#define DISPOD_WIFI_SCANNING_BIT                    (BIT1)
#define DISPOD_WIFI_CONNECTING_BIT	                (BIT2)
#define DISPOD_WIFI_CONNECTED_BIT		            (BIT3)
#define DISPOD_NTP_ACTIVATED_BIT                    (BIT4)
#define DISPOD_NTP_UPDATING_BIT    	                (BIT5)
#define DISPOD_NTP_UPDATED_BIT			            (BIT6)
#define DISPOD_BLE_ACTIVATED_BIT                    (BIT7)
#define DISPOD_BLE_SCANNING_BIT     	            (BIT8)
#define DISPOD_BLE_CONNECTING_BIT    	            (BIT9)
#define DISPOD_BLE_CONNECTED_BIT                    (BIT10)
#define DISPOD_SD_ACTIVATED_BIT                     (BIT11)
#define DISPOD_SD_AVAILABLE_BIT                     (BIT12)
#define DISPOD_SCREEN_STATUS_BIT                    (BIT13)
#define DISPOD_SCREEN_RUNNING_BIT                   (BIT14)
#define DISPOD_SCREEN_CONFIG_BIT                    (BIT15)
#define DISPOD_SCREEN_OTA_BIT                       (BIT16)
#define DISPOD_SCREEN_COMPLETE_BIT                  (BIT17)     // complete/first display update necessary
EventGroupHandle_t dispod_event_group;

// disPOD Client callback function events
typedef enum {
    // activities -> events
    DISPOD_WIFI_ACT_EVT = 0,                    /*!< When WIFI has been activated, the event comes */
    DISPOD_WIFI_DEACT_EVT,                  /*!< When WIFI has been deactivated, the event comes */
    DISPOD_WIFI_SCANNING_EVT,               /*!< When WIFI is starting scanning, the event comes */
    DISPOD_WIFI_CONNECTING_EVT,             /*!< When WIFI is starting connecting, the event comes */
    DISPOD_WIFI_CONNECTED_EVT,              /*!< When WIFI got connected, the event comes */
    DISPOD_WIFI_DISCONNECTED_EVT,           /*!< When WIFI got disconnected, the event comes */
    DISPOD_WIFI_INTERNET_EVT,               /*!< When internet gets available, the event comes */
    DISPOD_NTP_ACT_EVT,                     /*!< When NTP has been activated, the event comes */
    DISPOD_NTP_DEACT_EVT,                   /*!< When NTP has been deactivated, the event comes */
    DISPOD_NTP_NO_TIME_AVAIL_EVT,           /*!< When NTP has been activated and no time avail, the event comes */
    DISPOD_NTP_UPDATING_EVT,                /*!< When NTP updated has started, the event comes */
    DISPOD_NTP_TIME_AVAIL_EVT,              /*!< When NTP time has been sucessfully retrieved, the event comes */
    DISPOD_BLE_ACT_EVT,                     /*!< When BLE has been activated, the event comes */
    DISPOD_BLE_DEACT_EVT,                   /*!< When BLE has been deactivated, the event comes */
    DISPOD_BLE_NO_CONNECTION_EVT,           /*!< When BLE activated but no connection, the event comes */
    DISPOD_BLE_SCAN_STARTED_EVT,            /*!< When BLE scan started, the event comes */
    DISPOD_BLE_CONNECTING_DEVICE_EVT,       /*!< When BLE found a device and started to connect, the event comes */
    DISPOD_BLE_CONNECTED_EVT,               /*!< When BLE gets connected to target device, the event comes */
    DISPOD_BLE_DISCONNECTED_EVT,            /*!< When BLE got's disconnected from target device, the event comes */
    DISPOD_BLE_NOTIFICATION_EVT,            /*!< When BLE notification has been received, the event comes */
    DISPOD_SD_ACT_EVT,                      /*!< When SD card has been activated, the event comes */
    DISPOD_SD_DEACT_EVT,                    /*!< When SD card has been deactivated, the event comes */
    DISPOD_SD_NO_CARD_AVAIL_EVT,            /*!< When SD card has been activated but no card avail, the event comes */
    DISPOD_SD_CARD_AVAIL_EVT,               /*!< When SD card is ready for use, the event comes */
    DISPOD_BUTTON_2SEC_PRESS_EVT,           /*!< When a button has been pressed for 2s, the event comes */
    DISPOD_BUTTON_5SEC_PRESS_EVT,           /*!< When a button has been pressed for 5s, the event comes */
    DISPOD_BUTTON_EVT,                      /*!< When a button has been PUSH, RELEASE, TAP event, the event comes */
    DISPOD_DISPLAY_UPDATE_EVT,             /*!< When a display update is necessary, the event comes */
    // workflow -> events
    DISPOD_STARTUP_EVT,                     /*!< When the disPOD event loop started, the event comes */
    DISPOD_BASIC_INIT_DONE_EVT,             /*!< When the basic init is completed, the event comes */
    DISPOD_DISPLAY_INIT_DONE_EVT,           /*!< When the display init is completed, the event comes */
    DISPOD_WIFI_INIT_DONE_EVT,              /*!< When the WiFi connect (successfull or failed!) completed, the event comes */
    DISPOD_NTP_INIT_DONE_EVT,               /*!< When the NTP update (successfull or failed!) completed, the event comes */
    DISPOD_BLE_DEVICE_DONE_EVT,             /*!< When the BLE connect (successfull or failed!) completed, the event comes */
    DISPOD_LEAVE_SCREEN_EVT,                /*!< When the current screen should be left (& to determine next screen), the event comes */
    DISPOD_ENTER_SCREEN_EVT,                /*!< When the new screen should be entered, the event comes */
    DISPOD_GO_SHUTDOWN_EVT,                 /*!< When the device should be shutdowned, the event comes */
    DISPOD_GO_SLEEP_EVT,               /*!< When the device should be shutdowned, the event comes */
    DISPOD_EVENT_MAX
} dispod_cb_event_t;

ESP_EVENT_DECLARE_BASE(ACTIVITY_EVENTS);
ESP_EVENT_DECLARE_BASE(WORKFLOW_EVENTS);
extern esp_event_loop_handle_t dispod_loop_handle;

// dispod screen data
extern dispod_screen_status_t dispod_screen_info;

// global running values data struct
extern runningValuesStruct_t running_values;

// SD card
#define sdPIN_NUM_MISO 19
#define sdPIN_NUM_MOSI 23
#define sdPIN_NUM_CLK  18
#define sdPIN_NUM_CS   4

// Buttons
#define BUTTON_A 0
#define BUTTON_B 1
#define BUTTON_C 2
#define BUTTON_A_PIN 39
#define BUTTON_B_PIN 38
#define BUTTON_C_PIN 37
#define BUTTON_ACTIVE_LEVEL 0

// Speaker
#define SPEAKER_PIN 25

// UART
#define USE_SERIAL Serial

// missing functions
#define min(a,b) \
    ({ __typeof__ (a) _a = (a); \
        __typeof__ (b) _b = (b); \
        _a < _b ? _a : _b; })

#define max(a,b) \
    ({ __typeof__ (a) _a = (a); \
        __typeof__ (b) _b = (b); \
        _a > _b ? _a : _b; })

#define map(x,in_min,in_max,out_min,out_max)\
    ({ __typeof__ (x) _x = (x); \
        __typeof__ (in_min) _in_min = (in_min); \
        __typeof__ (in_max) _in_max = (in_max); \
        __typeof__ (out_min) _out_min = (out_min); \
        __typeof__ (out_max) _out_max = (out_max); \
        (_x - _in_min) * (_out_max - _out_min) / (_in_max - _in_min) + _out_min; })

#endif // __DISPOD_CONFIG_H__
