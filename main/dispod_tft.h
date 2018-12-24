#ifndef __DISPOD_TFT_H__
#define __DISPOD_TFT_H__

#define SPI_BUS TFT_HSPI_HOST       // spi bus to use TFT_VSPI_HOST or TFT_HSPI_HOST



// buttons for array access
#define BUTTON_A        0
#define BUTTON_B        1
#define BUTTON_C        2
#define NUM_BUTTONS     3

// disPOD display event group
#define DISPOD_DISPLAY_UPDATE_BIT               (BIT0)
#define DISPOD_DISPLAY_COMPLETE_UPDATE_BIT      (BIT1)      // if UPDATE & COMPLETE -> redraw display completely
#define DISPOD_DISPLAY_CHANGESCREEN_BIT         (BIT2)
#define DISPOD_DISPLAY_SCREEN_SCREENSAVER_BIT   (BIT3)
#define DISPOD_DISPLAY_SCREEN_POWEROFF_BIT      (BIT4)
#define DISPOD_DISPLAY_SCREEN_POWERON_BIT       (BIT5)
EventGroupHandle_t dispod_display_evg;

typedef enum {
    SCREEN_SPLASH,
    SCREEN_STATUS,
    SCREEN_RUNNING,
    SCREEN_CONFIG,
    SCREEN_OTA,
    SCREEN_SCREENSAVER,
    SCREEN_POWEROFF,
    SCREEN_POWERON
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


// status screen values
typedef struct {
    display_screen_t        current_screen;
    display_screen_t        screen_to_show;
    display_wifi_status_t   wifi_status;
	char		            wifi_ssid[32];
    display_ntp_status_t    ntp_status;
    display_ble_status_t    ble_status;
    char                    ble_name[32];
    display_sd_status_t     sd_status;
    bool                    show_button[NUM_BUTTONS];
	char		            button_text[NUM_BUTTONS][20];
	bool                    show_status_text;
	char                    status_text[32];
} dispod_screen_status_t;

// initialize HW display
void dispod_display_initialize();

// initialize all display structs
void dispod_screen_data_initialize          (dispod_screen_status_t *params);

// functions to update status screen data
void dispod_screen_status_update_wifi       (dispod_screen_status_t *params, display_wifi_status_t new_status, const char* new_ssid);
void dispod_screen_status_update_ntp        (dispod_screen_status_t *params, display_ntp_status_t new_status);
void dispod_screen_status_update_ble        (dispod_screen_status_t *params, display_ble_status_t new_status, const char* new_name);
void dispod_screen_status_update_sd         (dispod_screen_status_t *params, display_sd_status_t new_status);
void dispod_screen_status_update_button     (dispod_screen_status_t *params, uint8_t change_button, bool new_status, char* new_button_text);
void dispod_screen_status_update_statustext (dispod_screen_status_t *params, bool new_show_text, char* new_status_text);

// functins to draw interval fields, indicator bar, ota progress bar
static void dispod_screen_draw_fields       (uint8_t line, uint8_t numLines, char* name, uint8_t numFields, uint8_t current);
static void dispod_screen_draw_indicator    (uint8_t line, uint8_t numLines, char* name,
	                                         int16_t valMin, int16_t valMax, int16_t curVal,
	                                        int16_t lowInterval, int16_t highInterval);
static void dispod_screen_update_OTA        (otaUpdate_t otaUpdate, bool clearScreen);

// update TFT with the status screen data
static void dispod_screen_status_update_display    (dispod_screen_status_t *params, bool complete);
static void dispod_screen_running_update_display   (dispod_screen_status_t *params, bool complete);
static void dispod_screen_ota_update_display       (dispod_screen_status_t *params, bool complete);

// function to run in a separate process to update the display using event groups
void dispod_screen_task(void *pvParameters);

#endif // __DISPOD_TFT_H__
