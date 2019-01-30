#ifndef __DISPOD_TFT_H__
#define __DISPOD_TFT_H__

#include "dispod_main.h"

#define SPI_BUS TFT_HSPI_HOST       // spi bus to use TFT_VSPI_HOST or TFT_HSPI_HOST

// buttons for array access     // TODO put in dispod_main.h
#define BUTTON_A        0
#define BUTTON_B        1
#define BUTTON_C        2
#define NUM_BUTTONS     3

// disPOD display event group
#define DISPOD_DISPLAY_UPDATE_BIT           (BIT0)  // display needs an update -> display task
#define DISPOD_DISPLAY_RUNNING_RSC_BIT      (BIT1)  // new RSC data available
extern EventGroupHandle_t dispod_display_evg;

typedef enum {
    SCREEN_SPLASH,
    SCREEN_STATUS,
    SCREEN_RUNNING,
    SCREEN_CONFIG,
    SCREEN_OTA,
    SCREEN_SCREENSAVER,
    SCREEN_POWEROFF,
    SCREEN_POWERON,
    SCREEN_NONE,
} display_screen_t;

typedef enum {
    WIFI_DEACTIVATED,
    WIFI_NOT_CONNECTED,
    WIFI_SCANNING,
    WIFI_CONNECTING,
    WIFI_CONNECTED
} display_wifi_status_t;

typedef enum {
    NTP_DEACTIVATED,
    NTP_TIME_NOT_SET,
    NTP_UPDATING,
    NTP_UPDATED
} display_ntp_status_t;

typedef enum {
    BLE_DEACTIVATED,
    BLE_NOT_CONNECTED,
    BLE_SEARCHING,
    BLE_CONNECTING,
    BLE_CONNECTED
} display_ble_status_t;

typedef enum {
    SD_DEACTIVATED,
    SD_NOT_AVAILABLE,
    SD_AVAILABLE
} display_sd_status_t;

typedef struct {
    uint8_t                 max_len;
    uint16_t                messages_send;
    uint16_t                messages_received;
    uint16_t                messages_failed;
} queue_status_t;

// status screen values
typedef struct {
    display_screen_t        current_screen;
    display_screen_t        screen_to_show;
    display_wifi_status_t   wifi_status;
	char		            wifi_ssid[32];
    display_ntp_status_t    ntp_status;
    display_ble_status_t    ble_status;
    char                    ble_name[64];
    display_sd_status_t     sd_status;
    bool                    show_button[NUM_BUTTONS];
	char		            button_text[NUM_BUTTONS][20];
	bool                    show_status_text;
	char                    status_text[32];
    bool                    show_q_status;
    queue_status_t          q_status;
} dispod_screen_status_t;

// initialize all display structs
void dispod_screen_status_initialize(dispod_screen_status_t *params);

// switch screen
void dispod_screen_change(dispod_screen_status_t *params, display_screen_t new_screen);

// functions to update status screen data
void dispod_screen_status_update_wifi       (dispod_screen_status_t *params, display_wifi_status_t new_status, const char* new_ssid);
void dispod_screen_status_update_ntp        (dispod_screen_status_t *params, display_ntp_status_t new_status);
void dispod_screen_status_update_ble        (dispod_screen_status_t *params, display_ble_status_t new_status, const char* new_name);
void dispod_screen_status_update_sd         (dispod_screen_status_t *params, display_sd_status_t new_status);
void dispod_screen_status_update_button     (dispod_screen_status_t *params, uint8_t change_button, bool new_status, const char* new_button_text);
void dispod_screen_status_update_statustext (dispod_screen_status_t *params, bool new_show_text, char* new_status_text);

// running screen
void dispod_screen_running_update_display();

// queue information
void dispod_screen_status_update_queue_status(dispod_screen_status_t *params, bool new_show_status);
void dispod_screen_status_update_queue      (dispod_screen_status_t *params, uint8_t cur_len, bool inc_send, bool inc_received, bool inc_failed);

// function to run in a separate process to update the display using event groups
void dispod_screen_task(void *pvParameters);

#endif // __DISPOD_TFT_H__
