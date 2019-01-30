#include <string.h>
#include <math.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// #include "tftspi.h"
// #include "tft.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "esp_err.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "Free_Fonts.h"
#include "dispod_main.h"
#include "dispod_tft.h"
#include "dispod_gattc.h"

static const char* TAG = "DISPOD_TFT";

// layout measures for status screen
#define XPAD		    10
#define YPAD		    10
#define XCEN            160
#define BOX_FRAME	    2
#define BOX_SIZE        18
#define BOX_X           10
#define TEXT_X          38
#define STATUS_SPRITE_WIDTH     320
#define STATUS_SPRITE_HEIGHT    22
#define TEXT_HEIGHT     22

#define X_BUTTON_A	    65          // display button x position (for center of button)
#define X_BUTTON_B	    160
#define X_BUTTON_C	    255

// layout measures for running screen
#define FIELD_MIN_X				        170
#define FIELD_MAX_X				        310
#define FIELD_BASE_Y			        14
#define FIELD_HALFHEIGHT		        11
#define FIELD_WIDTH				        (FIELD_MAX_X - FIELD_MIN_X)

// layout dimensions for indicator bar
#define INDICATOR_MIN_X					170
#define INDICATOR_MAX_X					310
#define INDICATOR_ADJ_MIN_X				(INDICATOR_MIN_X + INDICATOR_TARGET_CIRCLE_RADIUS)
#define INDICATOR_ADJ_MAX_X				(INDICATOR_MAX_X - INDICATOR_TARGET_CIRCLE_RADIUS)
#define INDICATOR_BASE_Y				14
#define INDICATOR_TARGET_CIRCLE_RADIUS	11

// Take from menuconfig
#define MIN_INTERVAL_CADENCE		CONFIG_RUNNING_MIN_INTERVAL_CADENCE
#define MAX_INTERVAL_CADENCE		CONFIG_RUNNING_MAX_INTERVAL_CADENCE
#define MIN_INTERVAL_STANCETIME		CONFIG_RUNNING_MIN_INTERVAL_GCT
#define MAX_INTERVAL_STANCETIME		CONFIG_RUNNING_MAX_INTERVAL_GCT


EventGroupHandle_t dispod_display_evg;

// sprites for blocks in status screen and running screen
typedef enum {
    SCREEN_BLOCK_STATUS_WIFI = 0,
    SCREEN_BLOCK_STATUS_NTP,
    SCREEN_BLOCK_STATUS_SD,
    SCREEN_BLOCK_STATUS_BLE,
    SCREEN_BLOCK_RUNNING_CAD,
    SCREEN_BLOCK_RUNNING_GCT,
    SCREEN_BLOCK_RUNNING_STR,
    SCREEN_OVERARCHING_STATUS,
    SCREEN_OVERARCHING_STATS,
    SCREEN_OVERARCHING_BUTTON,
    SCREEN_NUM_SPRITES,             // number of sprites necessary
} screen_block_t;

TFT_eSprite* spr[SCREEN_NUM_SPRITES] = { 0 };

// TFT_eSprite spr = TFT_eSprite(&M5.Lcd);



// initialize all display structs
void dispod_screen_status_initialize(dispod_screen_status_t *params)
{
    char buffer[64];
    ESP_LOGD(TAG, "dispod_screen_status_initialize()");

    // initialize dispod_screen_status_t struct
    params->current_screen = SCREEN_NONE;
    params->screen_to_show = SCREEN_STATUS;
    dispod_screen_status_update_wifi        (params, WIFI_NOT_CONNECTED, "n/a");
    dispod_screen_status_update_ntp         (params, NTP_TIME_NOT_SET);             // TODO where to deactivate?
    snprintf(buffer, 64, BLE_NAME_FORMAT, "-");
	dispod_screen_status_update_ble         (params, BLE_NOT_CONNECTED, buffer);      // TODO where to deactivate?
    dispod_screen_status_update_sd          (params, SD_NOT_AVAILABLE);                // TODO where to deactivate?
    dispod_screen_status_update_button      (params, BUTTON_A, false, "");
    dispod_screen_status_update_button      (params, BUTTON_B, false, "");
    dispod_screen_status_update_button      (params, BUTTON_C, false, "");
    dispod_screen_status_update_statustext  (params, false, "");

    // queue
    params->q_status.max_len            = 0;
    params->q_status.messages_received  = 0;
    params->q_status.messages_failed    = 0;
    params->show_q_status               = false;

    for (int i = 0; i < SCREEN_NUM_SPRITES; i++){
        spr[i] = new TFT_eSprite(&M5.Lcd);
    }
    spr[SCREEN_BLOCK_STATUS_WIFI]->createSprite(STATUS_SPRITE_WIDTH, STATUS_SPRITE_HEIGHT);
    spr[SCREEN_BLOCK_STATUS_NTP]->createSprite(STATUS_SPRITE_WIDTH, STATUS_SPRITE_HEIGHT);
    spr[SCREEN_BLOCK_STATUS_SD]->createSprite(STATUS_SPRITE_WIDTH, STATUS_SPRITE_HEIGHT);
    spr[SCREEN_BLOCK_STATUS_BLE]->createSprite(STATUS_SPRITE_WIDTH, STATUS_SPRITE_HEIGHT);

    spr[SCREEN_OVERARCHING_STATUS]->createSprite(STATUS_SPRITE_WIDTH, STATUS_SPRITE_HEIGHT);
    spr[SCREEN_OVERARCHING_STATS]->createSprite(STATUS_SPRITE_WIDTH, STATUS_SPRITE_HEIGHT);
    spr[SCREEN_OVERARCHING_BUTTON]->createSprite(STATUS_SPRITE_WIDTH, STATUS_SPRITE_HEIGHT);

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

void dispod_screen_status_update_button(dispod_screen_status_t *params, uint8_t change_button, bool new_status, const char* new_button_text)
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

// the status block consists of
//  a) a colored status box
//      a1) white frame     -> draw only once
//      a2) colored inside  -> update by status_color
//  b) a status text
static void s_draw_status_block(TFT_eSprite *spr, uint16_t status_color, char* status_text, bool first_sprite_draw)
{
    if(first_sprite_draw){
        spr->setFreeFont(FF17);
        spr->setTextColor(TFT_WHITE, TFT_BLACK);
        spr->setTextDatum(TL_DATUM);
        spr->fillSprite(TFT_BLACK);

        // draw frame
        spr->drawRect(BOX_X, 0, BOX_SIZE, BOX_SIZE, TFT_WHITE);
    }

    // draw inlay
	spr->fillRect(BOX_X + BOX_FRAME, 0 + BOX_FRAME, BOX_SIZE - 2 * BOX_FRAME, BOX_SIZE - 2 * BOX_FRAME, status_color);

    // clear and draw text
    spr->fillRect(TEXT_X, 0, 320 - TEXT_X, STATUS_SPRITE_HEIGHT, TFT_BLACK);
    // ESP_LOGI(TAG, "s_draw_status_block(), text %s", status_text);
	spr->drawString(status_text, TEXT_X, 1, GFXFF);
}

static void s_draw_status_text(TFT_eSprite *spr, bool show_status_text, char* status_text, bool first_sprite_draw)
{
    if(first_sprite_draw){
        spr->setFreeFont(FF17);
        spr->setTextColor(TFT_WHITE, TFT_BLACK);
        spr->setTextDatum(TC_DATUM);
        spr->fillSprite(TFT_BLACK);
    }
    // clear and draw text
    spr->fillSprite(TFT_BLACK);
    if(show_status_text)
    	spr->drawString(status_text, XCEN, 0, GFXFF);
}

static void s_draw_button_label(TFT_eSprite *spr,
    bool show_A, char* text_A, bool show_B, char* text_B, bool show_C, char* text_C, bool first_sprite_draw)
{
    if(first_sprite_draw){
        spr->setFreeFont(FF17);
        spr->setTextColor(TFT_WHITE, TFT_BLACK);
        spr->setTextDatum(TC_DATUM);
        spr->fillSprite(TFT_BLACK);
    }

	if (show_A) spr->drawString(text_A, X_BUTTON_A, 0, GFXFF);
	if (show_B) spr->drawString(text_B, X_BUTTON_B, 0, GFXFF);
	if (show_C) spr->drawString(text_C, X_BUTTON_C, 0, GFXFF);
}

static void dispod_screen_status_update_display(dispod_screen_status_t *params, bool complete)
{
	uint16_t ypos, xpos2;
    uint16_t tmp_color;
    char     buffer[64];

    //  FF17 &FreeSans9pt7b
    //  FF18 &FreeSans12pt7b
    //  FF19 &FreeSans18pt7b
    //  FF20 &FreeSans24pt7b

    if(complete) {
        M5.Lcd.fillScreen(BLACK);
        M5.Lcd.setFreeFont(FF17);
        M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);

    	// Title
        M5.Lcd.setTextDatum(TC_DATUM);
        M5.Lcd.drawString("Starting disPOD...", XCEN, YPAD, GFXFF);
    }

	// 1) WiFi
	switch (params->wifi_status) {
	case WIFI_DEACTIVATED:      tmp_color = TFT_LIGHTGREY;  break;
	case WIFI_NOT_CONNECTED:    tmp_color = TFT_RED;        break;
    case WIFI_SCANNING:         tmp_color = TFT_CYAN;       break;
	case WIFI_CONNECTING:       tmp_color = TFT_YELLOW;     break;
    case WIFI_CONNECTED:        tmp_color = TFT_GREEN;      break;
	default:                    tmp_color = TFT_PURPLE;     break;
	}

    if( (params->wifi_status == WIFI_CONNECTING) || (params->wifi_status == WIFI_CONNECTED) ){
        snprintf(buffer, 64, WIFI_NAME_FORMAT, params->wifi_ssid);
    } else if(params->wifi_status == WIFI_SCANNING){
        snprintf(buffer, 64, WIFI_NAME_FORMAT, "scanning");
    } else {
        snprintf(buffer, 64, WIFI_NAME_FORMAT, "-");
    }
    s_draw_status_block(spr[SCREEN_BLOCK_STATUS_WIFI], tmp_color, buffer, complete);

	// 2) NTP
	switch (params->ntp_status) {
    case NTP_DEACTIVATED:       tmp_color = TFT_LIGHTGREY;  break;
    case NTP_TIME_NOT_SET:      tmp_color = TFT_RED;        break;
    case NTP_UPDATING:          tmp_color = TFT_YELLOW;     break;
    case NTP_UPDATED:           tmp_color = TFT_GREEN;      break;
	default:                    tmp_color = TFT_PURPLE;     break;
	}
    s_draw_status_block(spr[SCREEN_BLOCK_STATUS_NTP], tmp_color, "Time set", complete);

	// 3) SD Card storage
	switch (params->sd_status) {
    case SD_DEACTIVATED:        tmp_color = TFT_LIGHTGREY;  break;
    case SD_NOT_AVAILABLE:      tmp_color = TFT_RED;        break;
    case SD_AVAILABLE:          tmp_color = TFT_GREEN;      break;
	default:                    tmp_color = TFT_PURPLE;     break;
	}
    s_draw_status_block(spr[SCREEN_BLOCK_STATUS_SD], tmp_color, "SD Card", complete);

	// 4) BLE devices
	switch (params->ble_status) {
    case BLE_DEACTIVATED:       tmp_color = TFT_LIGHTGREY;  break;
    case BLE_NOT_CONNECTED:     tmp_color = TFT_RED;        break;
    case BLE_SEARCHING:         tmp_color = TFT_CYAN;       break;
    case BLE_CONNECTING:        tmp_color = TFT_YELLOW;     break;
    case BLE_CONNECTED:         tmp_color = TFT_GREEN;      break;
	default:                    tmp_color = TFT_PURPLE;     break;
	}
    s_draw_status_block(spr[SCREEN_BLOCK_STATUS_BLE], tmp_color, params->ble_name, complete);

	// 5) Status text line
    s_draw_status_text(spr[SCREEN_OVERARCHING_STATUS], params->show_status_text, params->status_text, complete);

	// 6) Button label
    s_draw_button_label(spr[SCREEN_OVERARCHING_BUTTON],
        params->show_button[BUTTON_A], params->button_text[BUTTON_A],
        params->show_button[BUTTON_B], params->button_text[BUTTON_B],
        params->show_button[BUTTON_C], params->button_text[BUTTON_C], complete);

    ESP_LOGI(TAG,"dispod_screen_status_update_display(), run before push" );
    // push all sprites
    ypos = YPAD + TEXT_HEIGHT + YPAD;
    spr[SCREEN_BLOCK_STATUS_WIFI]->pushSprite(0, ypos);

	ypos += STATUS_SPRITE_HEIGHT + YPAD;
    spr[SCREEN_BLOCK_STATUS_NTP]->pushSprite(0, ypos);

    ypos += STATUS_SPRITE_HEIGHT + YPAD;
    spr[SCREEN_BLOCK_STATUS_SD]->pushSprite(0, ypos);

    ypos += STATUS_SPRITE_HEIGHT + YPAD;
    spr[SCREEN_BLOCK_STATUS_BLE]->pushSprite(0, ypos);

    ypos += STATUS_SPRITE_HEIGHT + YPAD;
    spr[SCREEN_OVERARCHING_STATUS]->pushSprite(0, ypos);

    ypos = 240 - STATUS_SPRITE_HEIGHT;
    spr[SCREEN_OVERARCHING_BUTTON]->pushSprite(0, ypos);
}

static void dispod_screen_draw_fields(uint8_t line, char* name, uint8_t numFields, float f_current)
{
    uint16_t    textHeight;
    uint16_t    xPad = 10, yPad = 6, yLine, xVal;
    char        buffer[32];
    uint16_t    current;

	textHeight = M5.Lcd.fontHeight(GFXFF);
    yLine = yPad + (textHeight  + yPad) * line;

    // field title
    M5.Lcd.setTextDatum(TL_DATUM);
    M5.Lcd.drawString(name, xPad, yLine, GFXFF);

    current = (uint16_t) round(f_current);
	bool inInterval = (current >= 1) && (current <= 2);
	if (inInterval)
        M5.Lcd.setTextColor(TFT_GREEN, TFT_BLACK);
	else
		M5.Lcd.setTextColor(TFT_RED, TFT_BLACK);
    sprintf(buffer, "%1.1f", f_current);

    xVal = 10 + 75 + 10 + 60;
    M5.Lcd.setTextDatum(TR_DATUM);
    M5.Lcd.drawString(buffer, xVal, yLine, GFXFF);
    M5.Lcd.setTextDatum(TL_DATUM);

	uint8_t y0 = yLine + FIELD_BASE_Y;
	// frame
	M5.Lcd.drawRect(FIELD_MIN_X, y0 - FIELD_HALFHEIGHT, FIELD_WIDTH, 2 * FIELD_HALFHEIGHT, TFT_WHITE);

	// ticks
	uint8_t deltaX = FIELD_WIDTH / numFields;
	for (int i = 0; i < numFields; i++)
		M5.Lcd.drawLine(FIELD_MIN_X + i * deltaX, y0 - FIELD_HALFHEIGHT, FIELD_MIN_X + i * deltaX, y0 + FIELD_HALFHEIGHT - 1, TFT_WHITE);

	// mark current
    uint16_t tmp_width = deltaX - 3 + (current == (numFields-1) ? 1 : 0);
    uint32_t tmp_color = (inInterval ? TFT_GREEN : TFT_RED);
	M5.Lcd.fillRect(FIELD_MIN_X + current * deltaX + 2, y0 - FIELD_HALFHEIGHT + 2, tmp_width, 2 * FIELD_HALFHEIGHT - 4, tmp_color);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
}

static void dispod_screen_draw_indicator(uint8_t line, char* name, bool print_value,
	int16_t valMin, int16_t valMax, int16_t curVal,
	int16_t lowInterval, int16_t highInterval)
{
    uint16_t    textHeight;
    uint16_t    xPad = 10, yPad = 6, yLine, xVal;
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

	textHeight = M5.Lcd.fontHeight(GFXFF);
    ESP_LOGD(TAG, "textHeight = %u", textHeight);
    yLine = yPad + (textHeight  + yPad) * line;

	// current values out of range -> move into range
	uint16_t adjCurVal = curVal;
	if (curVal < valMin)
		adjCurVal = valMin;
	if (curVal > valMax)
		adjCurVal = valMax;

	// calculate x base coordinates
	uint16_t xLowInterval   = __map(lowInterval,  valMin, valMax, INDICATOR_ADJ_MIN_X,     INDICATOR_ADJ_MAX_X);
	uint16_t xHighInterval  = __map(highInterval, valMin, valMax, INDICATOR_ADJ_MIN_X,     INDICATOR_ADJ_MAX_X);
	uint16_t xTarget        = __map(adjCurVal,    valMin, valMax, INDICATOR_ADJ_MIN_X + 2, INDICATOR_ADJ_MAX_X - 2);
	// calculate y base coordinates
	uint16_t yBaseline = yLine + INDICATOR_BASE_Y;		// center/base line to display indicator

	ESP_LOGD(TAG, "displayDrawIndicator: indMinX %u, indMaxX %u, indAdjMinX %u, indAdjMaxX %u, xLowInt %u, xHighInt %u, xTarget %u, yPad %u, yLine %u, yBaseLine %u",
		INDICATOR_MIN_X, INDICATOR_MAX_X, INDICATOR_ADJ_MIN_X, INDICATOR_ADJ_MAX_X, xLowInterval, xHighInterval, xTarget, yPad, yLine, yBaseline);

	// check if the current value is in target interval
	bool inInterval = (curVal >= lowInterval) && (curVal <= highInterval);

	// show name
    M5.Lcd.setTextDatum(TL_DATUM);
    M5.Lcd.drawString(name, xPad, yLine, GFXFF);

	if (inInterval)
        M5.Lcd.setTextColor(TFT_GREEN, TFT_BLACK);
	else
		M5.Lcd.setTextColor(TFT_RED, TFT_BLACK);

    // show value
    if(print_value){
        sprintf(buffer, "%u", curVal);
        xVal = 10 + 75 + 10 + 60;
        M5.Lcd.setTextDatum(TR_DATUM);
        M5.Lcd.drawString(buffer, xVal, yLine, GFXFF);
        M5.Lcd.setTextDatum(TL_DATUM);
    }
	M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);

    // ESP_LOGD(TAG, "drawindicator textwidth %u for '%s', textheight %u", TFT_getStringWidth(name), name, textHeight);

	// baseline, from INDICATOR_MIN_X to INDICATOR_MAX_X
	M5.Lcd.drawLine(INDICATOR_MIN_X+2, yBaseline, INDICATOR_MAX_X-2, yBaseline, TFT_WHITE);
	//DEBUGLOG("displayDrawIndicator: drawLine %u, %u, %u, %u, %u", INDICATOR_ADJ_MIN_X, yBaseline, INDICATOR_ADJ_MAX_X, yBaseline, color);

	// show rounded rectangle (first fill w/background, then draw w/foreground color)
	M5.Lcd.fillRoundRect(
		xLowInterval - INDICATOR_TARGET_CIRCLE_RADIUS - 2, yBaseline - INDICATOR_TARGET_CIRCLE_RADIUS-2,		        // x0, y0 (top left corner)
		xHighInterval - xLowInterval + 2 * INDICATOR_TARGET_CIRCLE_RADIUS + 4, 5 + 2 * INDICATOR_TARGET_CIRCLE_RADIUS,	// w, h
		INDICATOR_TARGET_CIRCLE_RADIUS, TFT_BLACK);														            // radius, color
	M5.Lcd.drawRoundRect(
		xLowInterval - INDICATOR_TARGET_CIRCLE_RADIUS-2, yBaseline - INDICATOR_TARGET_CIRCLE_RADIUS-2,		            // x0, y0 (top left corner)
		xHighInterval - xLowInterval + 2 * INDICATOR_TARGET_CIRCLE_RADIUS + 4, 5 + 2 * INDICATOR_TARGET_CIRCLE_RADIUS,  // w, h
		INDICATOR_TARGET_CIRCLE_RADIUS, TFT_WHITE);														            // radius, color
    // ESP_LOGD(TAG, "M5.Lcd.drawRoundRect x0 %u y0 %u w %u h %u r %u",
    //     xLowInterval - INDICATOR_TARGET_CIRCLE_RADIUS, yBaseline - INDICATOR_TARGET_CIRCLE_RADIUS-2,		            // x0, y0 (top left corner)
	// 	xHighInterval - xLowInterval + 2 * INDICATOR_TARGET_CIRCLE_RADIUS, 5 + 2 * INDICATOR_TARGET_CIRCLE_RADIUS,  // w, h
	// 	INDICATOR_TARGET_CIRCLE_RADIUS);

	// middle circle, filled = in target range
	M5.Lcd.fillCircle(xTarget, yBaseline, INDICATOR_TARGET_CIRCLE_RADIUS, TFT_BLACK); // delete (background) first
	if (inInterval) {
		M5.Lcd.fillCircle(xTarget, yBaseline, INDICATOR_TARGET_CIRCLE_RADIUS, TFT_GREEN);
	}
	else {
		M5.Lcd.fillCircle(xTarget, yBaseline, INDICATOR_TARGET_CIRCLE_RADIUS, TFT_RED);
	}
    ESP_LOGD(TAG, "M5.Lcd.fillCircle x0 %u y0 %u r %u",xTarget, yBaseline, INDICATOR_TARGET_CIRCLE_RADIUS);
}

static void dispod_screen_draw_status_line_running(uint8_t line, dispod_screen_status_t *params)
{
    uint16_t    textHeight;
    uint16_t    xPad = 10, yPad = 6, yLine, xVal, ypos;
    char        buffer[64];

    snprintf(buffer, 64, "Q: max %u, snd %u, rec %u, fail %u",
        params->q_status.max_len, params->q_status.messages_send, params->q_status.messages_received, params->q_status.messages_failed);
	if(params->show_q_status){
        M5.Lcd.setFreeFont(FF17);
        textHeight = M5.Lcd.fontHeight(GFXFF);
        // yLine = yPad + (textHeight  + yPad) * line;

	    // 5) Status text line (copied from above, needs cleanup)
	    // ypos = yLine + YPAD;
        ypos = 240 - 2 * (textHeight + YPAD) - YPAD;
        M5.Lcd.setTextDatum(TL_DATUM);
        M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
        M5.Lcd.drawString(buffer, xPad, ypos, GFXFF);
    }
    ESP_LOGD(TAG, "dispod_screen_draw_status_line_running(): %s", buffer);
}

static void dispod_screen_draw_footer(uint8_t line, dispod_screen_status_t *params)
{
	uint16_t textHeight, boxSize, xpos, ypos, xpos2, xcen = 160;
    uint16_t tmp_color;
    char     buffer[64];

    M5.Lcd.setFreeFont(FF17);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
	textHeight = M5.Lcd.fontHeight(GFXFF);

    // copied from above -> needs cleanup and remove duplicate
	// 6) Button label
	// ypos += textHeight + YPAD;          // ypos = 240 - YPAD;
    M5.Lcd.setTextDatum(TC_DATUM);
    xpos = X_BUTTON_A;
    ypos = 240 - textHeight; // - YPAD;

    // ESP_LOGD(TAG, "6) button label, show A %u x %u, y %u, text %s", (params->show_button[BUTTON_A]?1:0), xpos, ypos, params->button_text[BUTTON_A]);
	if (params->show_button[BUTTON_A])
		M5.Lcd.drawString(params->button_text[BUTTON_A], xpos, ypos, GFXFF);

    xpos = X_BUTTON_B;
    // ESP_LOGD(TAG, "6) button label, show B %u x %u, y %u, text %s", (params->show_button[BUTTON_B]?1:0), xpos, ypos, params->button_text[BUTTON_B]);
	if (params->show_button[BUTTON_B])
		M5.Lcd.drawString(params->button_text[BUTTON_B], xpos, ypos, GFXFF);

    xpos = X_BUTTON_C;
    // ESP_LOGD(TAG, "6) button label, show C %u x %u, y %u, text %s", (params->show_button[BUTTON_C]?1:0), xpos, ypos, params->button_text[BUTTON_C]);
	if (params->show_button[BUTTON_C])
		M5.Lcd.drawString(params->button_text[BUTTON_C], xpos, ypos, GFXFF);
}



// OTA display update function
/*#define BAR_PAD		3
static void dispod_screen_ota_update_display(otaUpdate_t otaUpdate, bool clearScreen)
{
	int ypos;
	int x0 = 20, x1 = 300, y0 = 60, y1 = 100;	// gauge corner
	int mapx;

	// Preparation
    M5.Lcd.fillScreen(TFT_BLACK);
	TFT_resetclipwin();
    TFT_setFont(DEJAVU18_FONT, NULL);       // DEJAVU18_FONT
    _fg = TFT_WHITE;
	_bg = TFT_BLACK;                        // (color_t){ 64, 64, 64 };

	// Title
	ypos = 10;
    M5.Lcd.drawString("OTA Update...", CENTER, ypos, GFXFF);

	ypos = 180;
	if (otaUpdate.otaUpdateError_) {
		// Error Message
		// M5.Lcd.drawString(otaErrorNames[otaUpdate.otaUpdateErrorNr_], CENTER, ypos, GFXFF);
	}
	else if (otaUpdate.otaUpdateEnd_) {
		M5.Lcd.drawString("Done, rebooting...", CENTER, ypos, GFXFF);
	}
	else {
		if (clearScreen)
			TFT_drawRect(x0, y0, x1, y1, TFT_WHITE);
		mapx = map(otaUpdate.otaUpdateProgress_, 0, 100, 0, x1 - x0 - 2 * BAR_PAD);
		TFT_fillRect(x0 + BAR_PAD, y0 + BAR_PAD, mapx, y1 - y0 - 2 * BAR_PAD, TFT_LIGHTGREY);
	}
}
*/

void dispod_screen_running_update_display(dispod_screen_status_t *params, bool complete) {
	runningValuesStruct_t* values = &running_values;

	// Preparation
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setFreeFont(FF19);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);

    dispod_screen_draw_indicator(0, "Cad", true, MIN_INTERVAL_CADENCE - 20, MAX_INTERVAL_CADENCE + 20, values->values_to_display.cad, MIN_INTERVAL_CADENCE, MAX_INTERVAL_CADENCE);
	dispod_screen_draw_indicator(1, "GCT", true, MIN_INTERVAL_STANCETIME, 260, values->values_to_display.GCT, MIN_INTERVAL_STANCETIME, MAX_INTERVAL_STANCETIME);
    dispod_screen_draw_fields   (2, "Str", 3, values->values_to_display.str / 10.);
    dispod_screen_draw_status_line_running(3, params);
    dispod_screen_draw_footer             (4, params);
	// ESP_LOGD(TAG, "updateDisplayWithRunningValues: cad %3u stance %3u strike %1u", values->values_to_display.cad, values->values_to_display.GCT, values->values_to_display.str);

    // testing:
    // ESP_LOGI(TAG, "dispod_screen_draw_indicator(): textWidth('GCT') = %u", M5.Lcd.textWidth("GCT"));
    // ESP_LOGI(TAG, "dispod_screen_draw_indicator(): textWidth('222') = %u", M5.Lcd.textWidth("222"));
    // dispod_screen_draw_indicator(0, "Cad", true, MIN_INTERVAL_CADENCE - 20, MAX_INTERVAL_CADENCE + 20, 1, MIN_INTERVAL_CADENCE, MAX_INTERVAL_CADENCE);
    // dispod_screen_draw_fields   (1, "Str", 3, 0);
    // dispod_screen_draw_fields   (2, "Str", 3, 1);
    // dispod_screen_draw_fields   (3, "Str", 3, 2);
    // dispod_screen_draw_indicator(1, "Cad", true, MIN_INTERVAL_CADENCE - 20, MAX_INTERVAL_CADENCE + 20, 300, MIN_INTERVAL_CADENCE, MAX_INTERVAL_CADENCE);
    // dispod_screen_draw_indicator(2, "Cad", true, MIN_INTERVAL_CADENCE - 20, MAX_INTERVAL_CADENCE + 20, MIN_INTERVAL_CADENCE, MIN_INTERVAL_CADENCE, MAX_INTERVAL_CADENCE);
    // dispod_screen_draw_indicator(3, "Cad", true, MIN_INTERVAL_CADENCE - 20, MAX_INTERVAL_CADENCE + 20, MAX_INTERVAL_CADENCE, MIN_INTERVAL_CADENCE, MAX_INTERVAL_CADENCE);
	// dispod_screen_draw_indicator(0, "GCT", true, MIN_INTERVAL_STANCETIME, 260, 10, MIN_INTERVAL_STANCETIME, MAX_INTERVAL_STANCETIME);
	// dispod_screen_draw_indicator(1, "STR", true, MIN_INTERVAL_STANCETIME, 260, MAX_INTERVAL_STANCETIME-1, MIN_INTERVAL_STANCETIME, MAX_INTERVAL_STANCETIME);
	// dispod_screen_draw_indicator(2, "CAD", true, MIN_INTERVAL_STANCETIME, 260, MAX_INTERVAL_STANCETIME, MIN_INTERVAL_STANCETIME, MAX_INTERVAL_STANCETIME);
	// dispod_screen_draw_indicator(3, "GCT", true, MIN_INTERVAL_STANCETIME, 260, MAX_INTERVAL_STANCETIME+1, MIN_INTERVAL_STANCETIME, MAX_INTERVAL_STANCETIME);
}

void dispod_screen_status_update_queue(dispod_screen_status_t *params, uint8_t cur_len, bool inc_send, bool inc_received, bool inc_failed)
{
    if(cur_len >  params->q_status.max_len)
        params->q_status.max_len = cur_len;
    if(inc_send)
        params->q_status.messages_send++;
    if(inc_received)
        params->q_status.messages_received++;
    if(inc_failed)
        params->q_status.messages_failed++;

    ESP_LOGV(TAG, "queue status: max_len=%u received=%u failed=%u",
        params->q_status.max_len, params->q_status.messages_received, params->q_status.messages_failed);
}

void dispod_screen_status_update_queue_status(dispod_screen_status_t *params, bool new_show_status)
{
    params->show_q_status = new_show_status;
}


void dispod_screen_task(void *pvParameters)
{
    bool complete = false;
    dispod_screen_status_t* params = (dispod_screen_status_t*)pvParameters;

    ESP_LOGI(TAG, "dispod_screen_task: started");

    for (;;)
    {
        while(!(xEventGroupWaitBits(dispod_display_evg, DISPOD_DISPLAY_UPDATE_BIT,
                pdTRUE, pdFALSE, portMAX_DELAY) & DISPOD_DISPLAY_UPDATE_BIT));

        ESP_LOGD(TAG, "dispod_screen_task: update display, screen_to_show %u", (uint8_t) params->screen_to_show);
        if(params->current_screen != params->screen_to_show){
                ESP_LOGD(TAG, "dispod_screen_task: switching from '%u' to '%u'", params->current_screen, params->screen_to_show);
        }

        switch(params->screen_to_show){
        case SCREEN_SPLASH:
            ESP_LOGW(TAG, "dispod_screen_task: SCREEN_SPLASH - not available yet");
            break;
        case SCREEN_STATUS:
            ESP_LOGD(TAG, "dispod_screen_task: SCREEN_STATUS");
            if(params->current_screen != params->screen_to_show){
                params->current_screen = params->screen_to_show;
                complete = true;
            }
            dispod_screen_status_update_display(params, complete);
            break;
        case SCREEN_RUNNING:
            ESP_LOGD(TAG, "dispod_screen_task: SCREEN_RUNNING");
            if(params->current_screen != params->screen_to_show){
                params->current_screen = params->screen_to_show;
                complete = true;
            }
            dispod_screen_running_update_display(params, complete);
            break;
        case SCREEN_CONFIG:
            ESP_LOGW(TAG, "dispod_screen_task: SCREEN_CONFIG - not available yet");
            break;
        case SCREEN_OTA:
            ESP_LOGW(TAG, "dispod_screen_task: SCREEN_OTA - not available yet");
            // dispod_screen_ota_update_display();
            break;
        case SCREEN_SCREENSAVER:
            ESP_LOGW(TAG, "dispod_screen_task: SCREEN_SCREENSAVER - not available yet");
            break;
        case SCREEN_POWEROFF:
            ESP_LOGW(TAG, "dispod_screen_task: SCREEN_POWEROFF - not available yet");
            break;
        case SCREEN_POWERON:
            ESP_LOGW(TAG, "dispod_screen_task: SCREEN_POWERON - not available yet");
            break;
        default:
            ESP_LOGW(TAG, "dispod_screen_task: unhandled: %d", params->screen_to_show);
            break;
        }

        complete = false;
    }
}