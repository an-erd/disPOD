#ifndef __DISPOD_CONFIG_H__
#define __DISPOD_CONFIG_H__

#include "esp_event.h"
#include "esp_event_loop.h"
#include "esp_err.h"
#include "freertos/event_groups.h"
#include "dispod_runvalues.h"

// disPOD event group
#define DISPOD_WIFI_CONNECTING_BIT	        (BIT0)
#define DISPOD_WIFI_CONNECTED_BIT		    (BIT1)
#define DISPOD_SNTP_UPDATING_BIT    	    (BIT2)
#define DISPOD_SNTP_UPDATED_BIT			    (BIT3)
#define DISPOD_BLE_SCANNING_BIT     	    (BIT4)
#define DISPOD_BLE_CONNECTING_BIT    	    (BIT5)
#define DISPOD_BLE_CONNECTED_BIT	 	    (BIT6)
#define DISPOD_SD_AVAILABLE_BIT			    (BIT7)
extern EventGroupHandle_t dispod_event_group;

// disPOD Client callback function events
typedef enum {
    DISPOD_WIFI_ACT_EVT                 = 0,        /*!< When WIFI has been activated, the event comes */
    DISPOD_WIFI_DEACT_EVT               = 1,        /*!< When WIFI has been deactivated, the event comes */
    DISPOD_WIFI_CONNECTING_EVT          = 2,        /*!< When WIFI is starting connecting, the event comes */
    DISPOD_WIFI_CONNECTED_EVT           = 3,        /*!< When WIFI got connected, the event comes */
    DISPOD_WIFI_DISCONNECTED_EVT        = 4,        /*!< When WIFI got disconnected, the event comes */
    DISPOD_WIFI_INTERNET_EVT            = 5,        /*!< When internet gets available, the event comes */
    DISPOD_NTP_ACT_EVT                  = 6,        /*!< When NTP has been activated, the event comes */
    DISPOD_NTP_DEACT_EVT                = 7,        /*!< When NTP has been deactivated, the event comes */
    DISPOD_NTP_NO_TIME_AVAIL_EVT        = 8,        /*!< When NTP has been activated and no time avail, the event comes */
    DISPOD_NTP_UPDATING_EVT             = 9,        /*!< When NTP updated has started, the event comes */
    DISPOD_NTP_TIME_AVAIL_EVT           = 10,       /*!< When NTP time has been sucessfully retrieved, the event comes */
    DISPOD_BLE_ACT_EVT                  = 11,       /*!< When BLE has been activated, the event comes */
    DISPOD_BLE_DEACT_EVT                = 12,       /*!< When BLE has been deactivated, the event comes */
    DISPOD_BLE_NO_CONNECTION_EVT        = 13,       /*!< When BLE activated but no connection, the event comes */
    DISPOD_BLE_SCAN_STARTED_EVT         = 14,       /*!< When BLE scan started, the event comes */
    DISPOD_BLE_CONNECTING_DEVICE_EVT    = 15,       /*!< When BLE found a device and started to connect, the event comes */
    DISPOD_BLE_CONNECTED_EVT            = 16,       /*!< When BLE gets connected to target device, the event comes */
    DISPOD_BLE_DISCONNECTED_EVT         = 17,       /*!< When BLE got's disconnected from target device, the event comes */
    DISPOD_BLE_NOTIFICATION_EVT         = 18,       /*!< When BLE notification has been received, the event comes */
    DISPOD_SD_ACT_EVT                   = 19,       /*!< When SD card has been activated, the event comes */
    DISPOD_SD_DEACT_EVT                 = 20,       /*!< When SD card has been deactivated, the event comes */
    DISPOD_SD_NO_CARD_AVAIL_EVT         = 21,       /*!< When SD card has been activated but no card avail, the event comes */
    DISPOD_SD_CARD_AVAIL_EVT            = 22,       /*!< When SD card is ready for use, the event comes */
    DISPOD_BUTTON_2SEC_PRESS_EVT        = 23,       /*!< When a button has been pressed for 2s, the event comes */
    DISPOD_BUTTON_5SEC_PRESS_EVT        = 24,       /*!< When a button has been pressed for 5s, the event comes */
    DISPOD_BUTTON_EVT                   = 25,       /*!< When a button has been PUSH, RELEASE, TAP event, the event comes */
    DISPOD_STARTUP_EVT                  = 26,
    DISPOD_BASIC_INIT_DONE_EVT          = 27,
    DISPOD_DISPLAY_INIT_DONE            = 28,
    DISPOD_WIFI_INIT_DONE               = 29,
    DISPOD_NTP_INIT_DONE                = 30,
    DISPOD_BLE_DEVICE_DONE              = 31,
} dispod_cb_event_t;


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
