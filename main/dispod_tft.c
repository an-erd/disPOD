#include <string.h>
#include <math.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tftspi.h"
#include "tft.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "esp_err.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "dispod_main.h"
#include "dispod_tft.h"

static const char* TAG = "DISPOD_TFT";

// layout measures for status screen
#define XPAD		    10
#define YPAD		    10
#define BOX_FRAME	    2
#define X_BUTTON_A	    65          // display button x position (for center of button)
#define X_BUTTON_B	    160
#define X_BUTTON_C	    255

// layout measures for running screen
#define FIELD_MIN_X				        160
#define FIELD_MAX_X				        310
#define FIELD_BASE_Y			        10
#define FIELD_HALFHEIGHT		        8
#define FIELD_WIDTH				        (FIELD_MAX_X - FIELD_MIN_X)

// layout dimensions for indicator bar
#define INDICATOR_MIN_X					160
#define INDICATOR_MAX_X					310
#define INDICATOR_ADJ_MIN_X				(INDICATOR_MIN_X + INDICATOR_TARGET_CIRCLE_RADIUS)
#define INDICATOR_ADJ_MAX_X				(INDICATOR_MAX_X - INDICATOR_TARGET_CIRCLE_RADIUS)
#define INDICATOR_BASE_Y				10
#define INDICATOR_TARGET_CIRCLE_RADIUS	8

// Take from menuconfig
#define MIN_INTERVAL_CADENCE		CONFIG_RUNNING_MIN_INTERVAL_CADENCE
#define MAX_INTERVAL_CADENCE		CONFIG_RUNNING_MAX_INTERVAL_CADENCE
#define MIN_INTERVAL_STANCETIME		CONFIG_RUNNING_MIN_INTERVAL_GCT
#define MAX_INTERVAL_STANCETIME		CONFIG_RUNNING_MAX_INTERVAL_GCT

// initialize HW display
void dispod_display_initialize()
{
    ESP_LOGI(TAG, "dispod_display_initialize()");

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
        // .clock_speed_hz=8000000,                // Initial clock out at 8 MHz
        .clock_speed_hz=8000000,                // Initial clock out at 8 MHz
        .mode=0,                                // SPI mode 0
        .spics_io_num=-1,                       // we will use external CS pin
		.spics_ext_io_num=PIN_NUM_CS,           // external CS pin
]		.flags=LB_SPI_DEVICE_HALFDUPLEX,        // ALWAYS SET  to HALF DUPLEX MODE!! for display spi
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

// initialize all display structs
void dispod_screen_status_initialize(dispod_screen_status_t *params)
{
    ESP_LOGI(TAG, "dispod_screen_status_initialize()");

    // initialize dispod_screen_status_t struct
    params->current_screen = SCREEN_STATUS;
    params->screen_to_show = SCREEN_STATUS;
    dispod_screen_status_update_wifi(params, WIFI_NOT_CONNECTED, "n/a");
    dispod_screen_status_update_ntp(params, NTP_TIME_NOT_SET);             // TODO where to deactivate?
	dispod_screen_status_update_ble(params, BLE_NOT_CONNECTED, NULL);      // TODO where to deactivate?
    dispod_screen_status_update_sd(params, SD_DEACTIVATED);                // TODO where to deactivate?
    dispod_screen_status_update_button(params, BUTTON_A, false, "");
    dispod_screen_status_update_button(params, BUTTON_B, false, "");
    dispod_screen_status_update_button(params, BUTTON_C, false, "");
    dispod_screen_status_update_statustext(params, false, "");
}

// function to change screen
void dispod_screen_change(dispod_screen_status_t *params, display_screen_t new_screen)
{
    params->screen_to_show = new_screen;
}

// functions to update status screen data
void dispod_screen_status_update_wifi(dispod_screen_status_t *params, display_wifi_status_t new_status, const char* new_ssid)
{
    params->wifi_status = new_status;
	memcpy(params->wifi_ssid, new_ssid, strlen(new_ssid) + 1);
}

void dispod_screen_status_update_ntp(dispod_screen_status_t *params, display_ntp_status_t new_status)
{
    params->ntp_status = new_status;
}

void dispod_screen_status_update_ble(dispod_screen_status_t *params, display_ble_status_t new_status, const char* new_name)
{
	params->ble_status = new_status;
    if(new_name)
        memcpy(params->ble_name, new_name, strlen(new_name) + 1);
}

void dispod_screen_status_update_sd(dispod_screen_status_t *params, display_sd_status_t new_status)
{
		params->sd_status = new_status;
}

void dispod_screen_status_update_button(dispod_screen_status_t *params, uint8_t change_button, bool new_status, char* new_button_text)
{
    params->show_button[change_button] = new_status;
    if(new_button_text)
        memcpy(params->button_text[change_button], new_button_text, strlen(new_button_text) + 1);
}

void dispod_screen_status_update_statustext(dispod_screen_status_t *params, bool new_show_text, char* new_status_text)
{
    params->show_status_text = new_show_text;
    if(new_status_text)
    	memcpy(params->status_text, new_status_text, strlen(new_status_text) + 1);
    else
        memcpy(params->status_text, "", strlen("")+1);
}

static void dispod_screen_status_update_display(dispod_screen_status_t *params, bool complete)
{
	uint16_t    textHeight, boxSize, xpos, ypos, xpos2;
    color_t     tmp_color;
    // arg bool complete unused

	// Preparation
    TFT_fillScreen(TFT_BLACK);
	TFT_resetclipwin();
    TFT_setFont(DEJAVU18_FONT, NULL);       // DEJAVU18_FONT
    _fg = TFT_WHITE;
	_bg = TFT_BLACK;                        // (color_t){ 64, 64, 64 };

	textHeight = TFT_getfontheight();
	boxSize = textHeight - 4;
	// ESP_LOGD(TAG, "screen textHeight %u", textHeight);

	// Title
	ypos = 10;
    TFT_print("Starting disPOD...", CENTER, ypos);
    // afterwards orientation -> set to "top left"

	// 1) WiFi
	xpos = XPAD;
	ypos += textHeight + 15;
    TFT_drawRect(xpos, ypos, boxSize, boxSize, TFT_WHITE);
	switch (params->wifi_status) {
	case WIFI_DEACTIVATED:      tmp_color = TFT_LIGHTGREY;  break;
	case WIFI_NOT_CONNECTED:    tmp_color = TFT_RED;        break;
    case WIFI_SCANNING:         tmp_color = TFT_CYAN;       break;
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
    case BLE_SEARCHING:         tmp_color = TFT_CYAN;       break;
    case BLE_CONNECTING:        tmp_color = TFT_YELLOW;     break;
    case BLE_CONNECTED:         tmp_color = TFT_GREEN;      break;
	default:                    tmp_color = TFT_PURPLE;     break;
	}
    TFT_fillRect(xpos + BOX_FRAME, ypos + BOX_FRAME, boxSize - 2 * BOX_FRAME, boxSize - 2 * BOX_FRAME, tmp_color);
	xpos += boxSize + XPAD;
	TFT_print("MilestonePod", xpos, ypos);

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
    // ESP_LOGD(TAG, "5) status text litle, show %u x %u, y %u, text %s", (params->show_status_text?1:0), xpos, ypos, params->status_text);
    if(params->show_status_text){
	    TFT_print(params->status_text, CENTER, ypos);
    } else {
        ;    // TODO no status text
    }

	// 6) Button label
	// ypos += textHeight + YPAD;          // ypos = 240 - YPAD;
    ypos = 240 - textHeight - YPAD;

    xpos = X_BUTTON_A - TFT_getStringWidth(params->button_text[BUTTON_A])/2;
    // ESP_LOGD(TAG, "6) button label, show A %u x %u, y %u, text %s", (params->show_button[BUTTON_A]?1:0), xpos, ypos, params->button_text[BUTTON_A]);
	if (params->show_button[BUTTON_A])
		TFT_print(params->button_text[BUTTON_A], xpos, ypos);

    xpos = X_BUTTON_B - TFT_getStringWidth(params->button_text[BUTTON_B])/2;
    // ESP_LOGD(TAG, "6) button label, show B %u x %u, y %u, text %s", (params->show_button[BUTTON_B]?1:0), xpos, ypos, params->button_text[BUTTON_B]);
	if (params->show_button[BUTTON_B])
		TFT_print(params->button_text[BUTTON_B], xpos, ypos);

    xpos = X_BUTTON_C - TFT_getStringWidth(params->button_text[BUTTON_C])/2;
    // ESP_LOGD(TAG, "6) button label, show C %u x %u, y %u, text %s", (params->show_button[BUTTON_C]?1:0), xpos, ypos, params->button_text[BUTTON_C]);
	if (params->show_button[BUTTON_C])
		TFT_print(params->button_text[BUTTON_C], xpos, ypos);
}

static void dispod_screen_draw_fields(uint8_t line, char* name, uint8_t numFields, float f_current)
{
    uint16_t    textHeight;
    uint16_t    xPad = 10, yPad = 10, yLine, xVal;
    char        buffer[32];
    uint8_t     current;

	textHeight = TFT_getfontheight();
    yLine = yPad + (textHeight  + yPad) * line;

    // field title
    TFT_print(name, xPad, yLine);
    sprintf(buffer, "%1.1f", f_current);
    xVal = 10 + 60 + 10 + 60 - TFT_getStringWidth(buffer);
    TFT_print(buffer, xVal, yLine);

	uint8_t y0 = yLine + FIELD_BASE_Y;
	// frame
	TFT_drawRect(FIELD_MIN_X, y0 - FIELD_HALFHEIGHT, FIELD_WIDTH, 2 * FIELD_HALFHEIGHT, TFT_WHITE);

	// ticks
	uint8_t deltaX = FIELD_WIDTH / numFields;
	for (int i = 0; i < numFields; i++)
		TFT_drawLine(FIELD_MIN_X + i * deltaX, y0 - FIELD_HALFHEIGHT, FIELD_MIN_X + i * deltaX, y0 + FIELD_HALFHEIGHT - 1, TFT_WHITE);

    current = (uint8_t) round(f_current);
	bool inInterval = (current >= 1) && (current <= 2);

	// show name
    _fg = TFT_WHITE;
    TFT_print(name, xPad, yLine);

	if (inInterval)
		_fg = TFT_GREEN;
	else
		_fg = TFT_RED;

	// mark current
	TFT_fillRect(FIELD_MIN_X + current * deltaX + 2, y0 - FIELD_HALFHEIGHT + 2, deltaX - 4, 2 * FIELD_HALFHEIGHT - 4, _fg);
    _fg = TFT_WHITE;

}

static void dispod_screen_draw_indicator(uint8_t line, char* name, bool print_value,
	int16_t valMin, int16_t valMax, int16_t curVal,
	int16_t lowInterval, int16_t highInterval)
{
    uint16_t    textHeight;
    uint16_t    xPad = 10, yPad = 10, yLine, xVal;
    char        buffer[32];

#ifdef DEBUG_DISPOD
	// make some checks:
	// - indicator adjusted min/max to make place for circle (center <-> target)
	if (INDICATOR_ADJ_MIN_X >= INDICATOR_ADJ_MAX_X)
		DEBUGLOG("ERROR: displayDrawIndicator: Indicator min/max values inconsistent");
	// - indicator adjusted min/max to make place for circle (center <-> target)
	if (lowInterval >= highInterval)
		DEBUGLOG("ERROR: displayDrawIndicator: low/high interval values inconsistent");
#endif // DEBUG_DISPOD

	textHeight = TFT_getfontheight();
    yLine = yPad + (textHeight  + yPad) * line;

	// current values out of range -> move into range
	uint8_t adjCurVal = curVal;
	if (curVal < valMin)
		adjCurVal = valMin;
	if (curVal > valMax)
		adjCurVal = valMax;

	// calculate x base coordinates
	uint16_t xLowInterval   = map(lowInterval,  valMin, valMax, INDICATOR_ADJ_MIN_X,     INDICATOR_ADJ_MAX_X);
	uint16_t xHighInterval  = map(highInterval, valMin, valMax, INDICATOR_ADJ_MIN_X,     INDICATOR_ADJ_MAX_X);
	uint16_t xTarget        = map(adjCurVal,    valMin, valMax, INDICATOR_ADJ_MIN_X + 2, INDICATOR_ADJ_MAX_X - 2);
	// calculate y base coordinates
	uint16_t yBaseline = yLine + INDICATOR_BASE_Y;		// center/base line to display indicator

	//DEBUGLOG("displayDrawIndicator: indMinX %u, indMaxX %u, indAdjMinX %u, indAdjMaxX %u, xLowInt %u, xHighInt %u, xTarget %u, yPad %u, yLine %u, yBaseLine %u",
	//	INDICATOR_MIN_X, INDICATOR_MAX_X, INDICATOR_ADJ_MIN_X, INDICATOR_ADJ_MAX_X, xLowInterval, xHighInterval, xTarget, yPad, yLine, yBaseline);

	// check if the current value is in target interval
	bool inInterval = (curVal >= lowInterval) && (curVal <= highInterval);

	// show name
    TFT_print(name, xPad, yLine);

	if (inInterval)
		_fg = TFT_GREEN;
	else
		_fg = TFT_RED;

    // show value
    if(print_value){
        sprintf(buffer, "%u", curVal);
        xVal = 10 + 60 + 10 + 60 - TFT_getStringWidth(buffer);
        TFT_print(buffer, xVal, yLine);
    }
    _fg = TFT_WHITE;

    ESP_LOGI(TAG, "drawindicator textwidth %u for '%s', textheight %u", TFT_getStringWidth(name), name, textHeight);

	// baseline, from INDICATOR_MIN_X to INDICATOR_MAX_X
	TFT_drawLine(INDICATOR_MIN_X+2, yBaseline, INDICATOR_MAX_X-2, yBaseline, TFT_WHITE);
	//DEBUGLOG("displayDrawIndicator: drawLine %u, %u, %u, %u, %u", INDICATOR_ADJ_MIN_X, yBaseline, INDICATOR_ADJ_MAX_X, yBaseline, color);

	// show rounded rectangle (first fill w/background, then draw w/foreground color)
	TFT_fillRoundRect(
		xLowInterval - INDICATOR_TARGET_CIRCLE_RADIUS, yBaseline - INDICATOR_TARGET_CIRCLE_RADIUS-2,		        // x0, y0 (top left corner)
		xHighInterval - xLowInterval + 2 * INDICATOR_TARGET_CIRCLE_RADIUS, 4 + 2 * INDICATOR_TARGET_CIRCLE_RADIUS,	// w, h
		INDICATOR_TARGET_CIRCLE_RADIUS, TFT_BLACK);														            // radius, color
	TFT_drawRoundRect(
		xLowInterval - INDICATOR_TARGET_CIRCLE_RADIUS, yBaseline - INDICATOR_TARGET_CIRCLE_RADIUS-2,		            // x0, y0 (top left corner)
		xHighInterval - xLowInterval + 2 * INDICATOR_TARGET_CIRCLE_RADIUS, 4 + 2 * INDICATOR_TARGET_CIRCLE_RADIUS,  // w, h
		INDICATOR_TARGET_CIRCLE_RADIUS, TFT_WHITE);														            // radius, color
    ESP_LOGI(TAG, "TFT_drawRoundRect x0 %u y0 %u w %u h %u r %u",
        xLowInterval - INDICATOR_TARGET_CIRCLE_RADIUS, yBaseline - INDICATOR_TARGET_CIRCLE_RADIUS-2,		            // x0, y0 (top left corner)
		xHighInterval - xLowInterval + 2 * INDICATOR_TARGET_CIRCLE_RADIUS, 4 + 2 * INDICATOR_TARGET_CIRCLE_RADIUS,  // w, h
		INDICATOR_TARGET_CIRCLE_RADIUS);

	// middle circle, filled = in target range
	TFT_fillCircle(xTarget, yBaseline, INDICATOR_TARGET_CIRCLE_RADIUS, TFT_BLACK); // delete (background) first
	if (inInterval) {
		TFT_fillCircle(xTarget, yBaseline, INDICATOR_TARGET_CIRCLE_RADIUS, TFT_GREEN);
	}
	else {
		TFT_fillCircle(xTarget, yBaseline, INDICATOR_TARGET_CIRCLE_RADIUS, TFT_RED);
	}
    ESP_LOGI(TAG, "TFT_fillCircle x0 %u y0 %u r %u",xTarget, yBaseline, INDICATOR_TARGET_CIRCLE_RADIUS);
}


// OTA display update function
/*#define BAR_PAD		3
static void dispod_screen_ota_update_display(otaUpdate_t otaUpdate, bool clearScreen)
{
	int ypos;
	int x0 = 20, x1 = 300, y0 = 60, y1 = 100;	// gauge corner
	int mapx;

	// Preparation
    TFT_fillScreen(TFT_BLACK);
	TFT_resetclipwin();
    TFT_setFont(DEJAVU18_FONT, NULL);       // DEJAVU18_FONT
    _fg = TFT_WHITE;
	_bg = TFT_BLACK;                        // (color_t){ 64, 64, 64 };

	// Title
	ypos = 10;
    TFT_print("OTA Update...", CENTER, ypos);

	ypos = 180;
	if (otaUpdate.otaUpdateError_) {
		// Error Message
		// TFT_print(otaErrorNames[otaUpdate.otaUpdateErrorNr_], CENTER, ypos);
	}
	else if (otaUpdate.otaUpdateEnd_) {
		TFT_print("Done, rebooting...", CENTER, ypos);
	}
	else {
		if (clearScreen)
			TFT_drawRect(x0, y0, x1, y1, TFT_WHITE);
		mapx = map(otaUpdate.otaUpdateProgress_, 0, 100, 0, x1 - x0 - 2 * BAR_PAD);
		TFT_fillRect(x0 + BAR_PAD, y0 + BAR_PAD, mapx, y1 - y0 - 2 * BAR_PAD, TFT_LIGHTGREY);
	}
}
*/

void dispod_screen_running_update_display() {
	runningValuesStruct_t* values = &running_values;

	// Preparation
    TFT_fillScreen(TFT_BLACK);
	TFT_resetclipwin();
    TFT_setFont(DEJAVU24_FONT, NULL);
    _fg = TFT_WHITE;
	_bg = TFT_BLACK;

	dispod_screen_draw_indicator(0, "Cad", true, MIN_INTERVAL_CADENCE - 20, MAX_INTERVAL_CADENCE + 20, values->values_to_display.cad, MIN_INTERVAL_CADENCE, MAX_INTERVAL_CADENCE);
	dispod_screen_draw_indicator(1, "GCT", true, MIN_INTERVAL_STANCETIME, 260, values->values_to_display.GCT, MIN_INTERVAL_STANCETIME, MAX_INTERVAL_STANCETIME);
    dispod_screen_draw_fields   (2, "Str", 3, values->values_to_display.str / 10.);

	ESP_LOGI(TAG, "updateDisplayWithRunningValues: cad %3u stance %3u strike %1u", values->values_to_display.cad, values->values_to_display.GCT, values->values_to_display.str);
}

void dispod_screen_task(void *pvParameters)
{
    // EventBits_t uxBits;
    bool complete;
    dispod_screen_status_t* params = (dispod_screen_status_t*)pvParameters;

    ESP_LOGI(TAG, "dispod_screen_task: started");

    for (;;)
    {
        while(!(xEventGroupWaitBits(dispod_display_evg, DISPOD_DISPLAY_UPDATE_BIT,
                pdTRUE, pdFALSE, portMAX_DELAY) & DISPOD_DISPLAY_UPDATE_BIT));

        ESP_LOGI(TAG, "dispod_screen_task: update display, screen_to_show %u", (uint8_t) params->screen_to_show);
        if(params->current_screen != params->screen_to_show){
                ESP_LOGI(TAG, "dispod_screen_task: switching from '%u' to '%u'", params->current_screen, params->screen_to_show);
        }

        switch(params->screen_to_show){
        case SCREEN_SPLASH:
            ESP_LOGI(TAG, "dispod_screen_task: SCREEN_SPLASH - not available yet");
            break;
        case SCREEN_STATUS:
            ESP_LOGI(TAG, "dispod_screen_task: SCREEN_STATUS");
            if(params->current_screen != params->screen_to_show)
                params->current_screen = params->screen_to_show;
            dispod_screen_status_update_display(params, complete);
            break;
        case SCREEN_RUNNING:
            ESP_LOGI(TAG, "dispod_screen_task: SCREEN_RUNNING");
            if(params->current_screen != params->screen_to_show)
                params->current_screen = params->screen_to_show;
            dispod_screen_running_update_display();
            break;
        case SCREEN_CONFIG:
            ESP_LOGI(TAG, "dispod_screen_task: SCREEN_CONFIG - not available yet");
            break;
        case SCREEN_OTA:
            ESP_LOGI(TAG, "dispod_screen_task: SCREEN_OTA");
            // dispod_screen_ota_update_display();
            break;
        case SCREEN_SCREENSAVER:
            ESP_LOGI(TAG, "dispod_screen_task: SCREEN_SCREENSAVER - not available yet");
            break;
        case SCREEN_POWEROFF:
            ESP_LOGI(TAG, "dispod_screen_task: SCREEN_POWEROFF - not available yet");
            break;
        case SCREEN_POWERON:
            ESP_LOGI(TAG, "dispod_screen_task: SCREEN_POWERON - not available yet");
            break;
        default:
            ESP_LOGI(TAG, "dispod_screen_task: unhandled: %d", params->screen_to_show);
            break;
        }
    }
}