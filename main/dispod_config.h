#ifndef __DISPOD_CONFIG_H__
#define __DISPOD_CONFIG_H__

#include "esp_event.h"
#include "esp_event_loop.h"
#include "freertos/event_groups.h"

// disPOD event group
#define DISPOD_WIFI_CONNECTING_BIT	        (BIT0)
#define DISPOD_WIFI_CONNECTED_BIT		    (BIT1)
#define DISPOD_SNTP_UPDATING_BIT    	    (BIT2)
#define DISPOD_SNTP_UPDATED_BIT			    (BIT3)
#define DISPOD_BLE_SCANNING_BIT     	    (BIT4)
#define DISPOD_BLE_CONNECTING_BIT    	    (BIT5)
#define DISPOD_BLE_CONNECTED_BIT	 	    (BIT6)
#define DISPOD_SD_AVAILABLE_BIT			    (BIT7)

EventGroupHandle_t dispod_event_group;

// SD card
#define sdPIN_NUM_MISO 19
#define sdPIN_NUM_MOSI 23
#define sdPIN_NUM_CLK  18
#define sdPIN_NUM_CS   4

// Buttons
#define BTN_A 0
#define BTN_B 1
#define BTN_C 2
#define BUTTON_A 0
#define BUTTON_B 1
#define BUTTON_C 2
#define BUTTON_A_PIN 39
#define BUTTON_B_PIN 38
#define BUTTON_C_PIN 37
#define BUTTON_ACTIVE_LEVEL 0


// BEEP PIN
#define SPEAKER_PIN 25
#define TONE_PIN_CHANNEL 0


// UART
#define USE_SERIAL Serial

#endif // __DISPOD_CONFIG_H__
