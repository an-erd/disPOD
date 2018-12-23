#ifndef __DISPOD_TFT_H__
#define __DISPOD_TFT_H__

#define SPI_BUS TFT_HSPI_HOST       // spi bus to use TFT_VSPI_HOST or TFT_HSPI_HOST

// layout measures
#define XPAD		    10
#define YPAD		    10
#define BOX_FRAME	    2

// display button position
#define X_BUTTON_A	    65
#define X_BUTTON_B	    160
#define X_BUTTON_C	    255

// buttons for array access
#define BUTTON_A        0
#define BUTTON_B        1
#define BUTTON_C        2
#define NUM_BUTTONS     3

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
} dispod_screen_info_t;

// functions to set the status to display when calling dispod_screen_update_display()
void dispod_display_initialize();
void dispod_screen_data_initialize  (dispod_screen_info_t *params);
void dispod_screen_update_wifi      (dispod_screen_info_t *params, display_wifi_status_t new_status, const char* new_ssid);
void dispod_screen_update_ntp       (dispod_screen_info_t *params, display_ntp_status_t new_status);
void dispod_screen_update_ble       (dispod_screen_info_t *params, display_ble_status_t new_status, const char* new_name);
void dispod_screen_update_sd        (dispod_screen_info_t *params, display_sd_status_t new_status);
void dispod_screen_update_button    (dispod_screen_info_t *params, uint8_t change_button, bool new_status, char* new_button_text);
void dispod_screen_update_statustext(dispod_screen_info_t *params, bool new_show_text, char* new_status_text);

// update TFT with the screen info data
void dispod_screen_update_display   (dispod_screen_info_t *params, bool complete);


#endif // __DISPOD_TFT_H__
