#ifndef __DISPOD_CONFIG_H__
#define __DISPOD_CONFIG_H__

#include "esp_event.h"
#include "esp_event_loop.h"
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

// disPOD event loop


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
