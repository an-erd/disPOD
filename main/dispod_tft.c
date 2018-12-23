#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tftspi.h"
#include "tft.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "dispod_tft.h"

static const char* TAG = "DISPOD_TFT";

void dispod_display_initialize()
{
    esp_err_t ret;

    // set display configuration, SPI devices
	tft_disp_type = DISP_TYPE_ILI9341;
	_width = DEFAULT_TFT_DISPLAY_WIDTH;     // smaller dimension
	_height = DEFAULT_TFT_DISPLAY_HEIGHT;   // larger dimension
	max_rdclock = 8000000;
    TFT_PinsInit();
    spi_lobo_device_handle_t spi;
    spi_lobo_bus_config_t buscfg={
        .miso_io_num=PIN_NUM_MISO,				// set SPI MISO pin
        .mosi_io_num=PIN_NUM_MOSI,				// set SPI MOSI pin
        .sclk_io_num=PIN_NUM_CLK,				// set SPI CLK pin
        .quadwp_io_num=-1,
        .quadhd_io_num=-1,
		.max_transfer_sz = 6*1024,
    };
    spi_lobo_device_interface_config_t devcfg={
        .clock_speed_hz=8000000,                // Initial clock out at 8 MHz
        .mode=0,                                // SPI mode 0
        .spics_io_num=-1,                       // we will use external CS pin
		.spics_ext_io_num=PIN_NUM_CS,           // external CS pin
		.flags=LB_SPI_DEVICE_HALFDUPLEX,        // ALWAYS SET  to HALF DUPLEX MODE!! for display spi
    };

    vTaskDelay(500 / portTICK_RATE_MS);
    ESP_LOGI(TAG, "Display config, Pins used: miso=%d, mosi=%d, sck=%d, cs=%d", PIN_NUM_MISO, PIN_NUM_MOSI, PIN_NUM_CLK, PIN_NUM_CS);

    // Initialize SPI bus and attach the LCD to the SPI bus
	ret = spi_lobo_bus_add_device(SPI_BUS, &buscfg, &devcfg, &spi);
    assert(ret == ESP_OK);
    ESP_LOGI(TAG, "SPI: display device added to spi bus (%d)", SPI_BUS);
	disp_spi = spi;

	// ==== Test select/deselect ====
	ret = spi_lobo_device_select(spi, 1);
    assert(ret==ESP_OK);
	ret = spi_lobo_device_deselect(spi);
    assert(ret==ESP_OK);

    ESP_LOGI(TAG, "SPI: attached display device, speed=%u", spi_lobo_get_speed(spi));
	ESP_LOGI(TAG, "SPI: bus uses native pins: %s", spi_lobo_uses_native_pins(spi) ? "true" : "false");

	// Initialize the Display ====

	ESP_LOGI(TAG, "SPI: display init... ");
	TFT_display_init();
	ESP_LOGI(TAG, "OK");

	// Detect maximum read speed
	max_rdclock = find_rd_speed();
	ESP_LOGI(TAG, "SPI: Max rd speed = %u", max_rdclock);

    // Set SPI clock used for display operations
	spi_lobo_set_speed(spi, DEFAULT_SPI_CLOCK);
	ESP_LOGI(TAG,"SPI: Changed speed to %u", spi_lobo_get_speed(spi));

    // set orientation, etc.
	font_rotate = 0;
	text_wrap = 0;
	font_transparent = 0;
	font_forceFixed = 0;
	gray_scale = 0;
    TFT_setGammaCurve(DEFAULT_GAMMA_CURVE);
	TFT_setRotation(LANDSCAPE);
	TFT_setFont(DEFAULT_FONT, NULL);
	TFT_resetclipwin();
}


void dispod_screen_data_initialize(dispod_screen_info_t *params)
{
    dispod_screen_update_wifi(params, WIFI_NOT_CONNECTED, "n/a");
    dispod_screen_update_ntp(params, NTP_TIME_NOT_SET);             // TODO where to deactivate?
	dispod_screen_update_ble(params, BLE_NOT_CONNECTED, NULL);      // TODO where to deactivate?
    dispod_screen_update_sd(params, SD_DEACTIVATED);                // TODO where to deactivate?
    dispod_screen_update_button(params, BUTTON_A, true, "Retry Wifi");
    dispod_screen_update_button(params, BUTTON_B, true, "Retry Pod");
    dispod_screen_update_button(params, BUTTON_C, true, "Continue");
    dispod_screen_update_statustext(params, true, "... none ...");
}

void dispod_screen_update_wifi(dispod_screen_info_t *params, display_wifi_status_t new_status, const char* new_ssid)
{
    params->wifi_status = new_status;
	memcpy(params->wifi_ssid, new_ssid, strlen(new_ssid) + 1);
}

void dispod_screen_update_ntp(dispod_screen_info_t *params, display_ntp_status_t new_status)
{
    params->ntp_status = new_status;
}

void dispod_screen_update_ble(dispod_screen_info_t *params, display_ble_status_t new_status, const char* new_name)
{
	params->ble_status = new_status;
    if(new_name)
        memcpy(params->ble_name, new_name, strlen(new_name) + 1);
}

void dispod_screen_update_sd(dispod_screen_info_t *params, display_sd_status_t new_status)
{
		params->sd_status = new_status;
}

void dispod_screen_update_button(dispod_screen_info_t *params, uint8_t change_button, bool new_status, char* new_button_text)
{
    params->show_button[change_button] = new_status;
    if(new_button_text)
        memcpy(params->button_text[change_button], new_button_text, strlen(new_button_text) + 1);
}

void dispod_screen_update_statustext(dispod_screen_info_t *params, bool new_show_text, char* new_status_text)
{
    params->show_status_text = new_show_text;
    if(new_status_text)
    	memcpy(params->status_text, new_status_text, strlen(new_status_text) + 1);
    else
        memcpy(params->status_text, "", strlen("")+1);
}

void dispod_screen_update_display   (dispod_screen_info_t *params, bool complete)
{
	uint16_t    textHeight, boxSize, xpos, ypos, xpos2;
    color_t     tmp_color;
    // arg bool complete unused

	// Preparation
    TFT_fillScreen(TFT_BLACK);
	TFT_resetclipwin();
    // TFT_setFont(DEFAULT_FONT, NULL);     // TODO M5.Lcd.setFreeFont(FF17);
    TFT_setFont(DEJAVU18_FONT, NULL);       // DEJAVU18_FONT
    _fg = TFT_WHITE;
	_bg = TFT_BLACK;                        // (color_t){ 64, 64, 64 };

	textHeight = TFT_getfontheight();
	boxSize = textHeight - 4;
	ESP_LOGI(TAG, "screen textHeight %u", textHeight);

	// Title
	ypos = 10;
    TFT_print("Starting disPOD...", CENTER, ypos);
    // afterwards orientation -> set to "top left"

	// 1) WiFi
	xpos = XPAD;
	ypos += textHeight + 5;
    TFT_drawRect(xpos, ypos, boxSize, boxSize, TFT_WHITE);
	switch (params->wifi_status) {
	case WIFI_DEACTIVATED:      tmp_color = TFT_LIGHTGREY;  break;
	case WIFI_NOT_CONNECTED:    tmp_color = TFT_RED;        break;
    case WIFI_SCANNING:         tmp_color = TFT_BLUE;       break;
	case WIFI_CONNECTING:       tmp_color = TFT_YELLOW;     break;
    case WIFI_CONNECTED:        tmp_color = TFT_GREEN;      break;
	default:                    tmp_color = TFT_PURPLE;     break;
	}
	TFT_fillRect(xpos + BOX_FRAME, ypos + BOX_FRAME, boxSize - 2 * BOX_FRAME, boxSize - 2 * BOX_FRAME, tmp_color);
	xpos += boxSize + XPAD;
	TFT_print("Wifi ", xpos, ypos);
	xpos2 = xpos + 40 + XPAD;   // TODO
	TFT_print(params->wifi_ssid, xpos2, ypos);

	// 2) NTP
	xpos = XPAD;
	ypos += textHeight;
	TFT_drawRect(xpos, ypos, boxSize, boxSize, TFT_WHITE);
	switch (params->ntp_status) {
    case NTP_DEACTIVATED:       tmp_color = TFT_LIGHTGREY;  break;
    case NTP_TIME_NOT_SET:      tmp_color = TFT_RED;        break;
    case NTP_UPDATING:          tmp_color = TFT_YELLOW;     break;
    case NTP_UPDATED:           tmp_color = TFT_GREEN;      break;
	default:                    tmp_color = TFT_PURPLE;     break;
	}

	TFT_fillRect(xpos + BOX_FRAME, ypos + BOX_FRAME, boxSize - 2 * BOX_FRAME, boxSize - 2 * BOX_FRAME, tmp_color);
    xpos += boxSize + XPAD;
	TFT_print("Internet time (NTP)", xpos, ypos);

	// 3) BLE devices
	xpos = XPAD;
	ypos += textHeight;
	TFT_drawRect(xpos, ypos, boxSize, boxSize, TFT_WHITE);
	switch (params->ble_status) {
    case BLE_DEACTIVATED:       tmp_color = TFT_LIGHTGREY;  break;
    case BLE_NOT_CONNECTED:     tmp_color = TFT_RED;        break;
    case BLE_SEARCHING:         tmp_color = TFT_BLUE;       break;
    case BLE_CONNECTING:        tmp_color = TFT_YELLOW;     break;
    case BLE_CONNECTED:         tmp_color = TFT_GREEN;      break;
	default:                    tmp_color = TFT_PURPLE;     break;
	}
    TFT_fillRect(xpos + BOX_FRAME, ypos + BOX_FRAME, boxSize - 2 * BOX_FRAME, boxSize - 2 * BOX_FRAME, tmp_color);
	xpos += boxSize + XPAD;
	TFT_print(params->ble_name, xpos, ypos);

	// 4) SD Card storage
	xpos = XPAD;
	ypos += textHeight;
	TFT_drawRect(xpos, ypos, boxSize, boxSize, TFT_WHITE);
	switch (params->sd_status) {
    case SD_DEACTIVATED:        tmp_color = TFT_LIGHTGREY;  break;
    case SD_NOT_AVAILABLE:      tmp_color = TFT_RED;        break;
    case SD_AVAILABLE:          tmp_color = TFT_GREEN;      break;
	default:                    tmp_color = TFT_PURPLE;     break;
	}
    TFT_fillRect(xpos + BOX_FRAME, ypos + BOX_FRAME, boxSize - 2 * BOX_FRAME, boxSize - 2 * BOX_FRAME, tmp_color);
	xpos += boxSize + XPAD;
	TFT_print("SD Card", xpos, ypos);

	// 5) Status text line
	ypos += textHeight + YPAD;
    ESP_LOGI(TAG, "5) status text litle, show %u x %u, y %u, text %s", (params->show_status_text?1:0), xpos, ypos, params->status_text);
    if(params->show_status_text){
	    TFT_print(params->status_text, CENTER, ypos);
    } else {
        ;    // TODO no status text
    }

	// 6) Button label
	ypos += textHeight + YPAD;          // ypos = 240 - XPAD;

    xpos = X_BUTTON_A - TFT_getStringWidth(params->button_text[BUTTON_A])/2;
    ESP_LOGI(TAG, "6) button label, show A %u x %u, y %u, text %s", (params->show_button[BUTTON_A]?1:0), xpos, ypos, params->button_text[BUTTON_A]);
	if (params->show_button[BUTTON_A])
		TFT_print(params->button_text[BUTTON_A], xpos, ypos);

    xpos = X_BUTTON_B - TFT_getStringWidth(params->button_text[BUTTON_B])/2;
    ESP_LOGI(TAG, "6) button label, show B %u x %u, y %u, text %s", (params->show_button[BUTTON_B]?1:0), xpos, ypos, params->button_text[BUTTON_B]);
	if (params->show_button[BUTTON_B])
		TFT_print(params->button_text[BUTTON_B], xpos, ypos);

    xpos = X_BUTTON_C - TFT_getStringWidth(params->button_text[BUTTON_C])/2;
    ESP_LOGI(TAG, "6) button label, show C %u x %u, y %u, text %s", (params->show_button[BUTTON_C]?1:0), xpos, ypos, params->button_text[BUTTON_C]);
	if (params->show_button[BUTTON_C])
		TFT_print(params->button_text[BUTTON_C], xpos, ypos);
}
