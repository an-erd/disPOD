// disPOD - connect to MilestonePod via BLE and read data and display on M5Stack-Fire

#include "esp_types.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/portmacro.h"
#include "soc/timer_group_struct.h"
#include "driver/periph_ctrl.h"
#include "driver/timer.h"
#include "esp32-hal-ledc.h"

#include <M5Stack.h>
#include "dispod_main.h"
#include "dispod_wifi.h"
#include "dispod_gattc.h"
#include "dispod_tft.h"
#include "dispod_sntp.h"
#include "dispod_ledc.h"
#include "dispod_runvalues.h"
#include "dispod_update.h"
#include "dispod_archiver.h"
#include "dispod_button.h"
#include "dispod_timer.h"

static const char* TAG = "DISPOD";

// Event group
EventGroupHandle_t dispod_event_group;

// Event loop
ESP_EVENT_DEFINE_BASE(WORKFLOW_EVENTS);
ESP_EVENT_DEFINE_BASE(ACTIVITY_EVENTS);
esp_event_loop_handle_t dispod_loop_handle;

// Storing values from BLE running device struct and queue
runningValuesStruct_t running_values;
QueueHandle_t running_values_queue;

// Storing screen information
dispod_screen_status_t dispod_screen_status;

// time to wait in
const TickType_t xTicksToWait = 100 / portTICK_PERIOD_MS;

// temp return value from xEventGroupWaitBits, ... functions
EventBits_t uxBits;

#define TIMER_DIVIDER         16  //  Hardware timer clock divider
#define TIMER_SCALE           (TIMER_BASE_CLK / TIMER_DIVIDER)  // convert counter value to seconds
#define TIMER_INTERVAL0_SEC   (0.3333) // cadence 180 = 1 min/180 = 3/sec
#define TEST_WITHOUT_RELOAD   0        // testing will be done without auto reload
#define TEST_WITH_RELOAD      1 // testing will be done with auto reload

/*
 * A sample structure to pass events
 * from the timer interrupt handler to the main program.
 */
typedef struct {
    int type;  // the type of timer's event
    int timer_group;
    int timer_idx;
    uint64_t timer_counter_value;
} timer_event_t;

// TODO Adafruit NeoPixels

#define M5STACK_FIRE_NEO_NUM_LEDS 10
#define M5STACK_FIRE_NEO_DATA_PIN 15

#ifdef ADAFRUIT_NEOPIXEL
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(M5STACK_FIRE_NEO_NUM_LEDS, M5STACK_FIRE_NEO_DATA_PIN, NEO_GRB + NEO_KHZ800);
#endif

#ifdef NEOPIXELBUS
// NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(M5STACK_FIRE_NEO_NUM_LEDS, M5STACK_FIRE_NEO_DATA_PIN);
NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> pixels(M5STACK_FIRE_NEO_NUM_LEDS, M5STACK_FIRE_NEO_DATA_PIN);
RgbColor NEOPIXEL_white(colorSaturation);
RgbColor NEOPIXEL_black(0);
#endif

// temp to don't have it twice
const char* otaErrorNames[] = {
	"Error: Auth Failed",		// OTA_AUTH_ERROR
	"Error: Begin Failed",		// OTA_BEGIN_ERROR
	"Error: Connect Failed",	// OTA_CONNECT_ERROR
	"Error: Receive Failed",	// OTA_RECEIVE_ERROR
	"Error: End Failed",		// OTA_END_ERROR
};

static void dispod_initialize()
{
    if(CONFIG_USE_WIFI)
        xEventGroupSetBits(dispod_event_group, DISPOD_WIFI_ACTIVATED_BIT);
    if(CONFIG_DISPOD_USE_SNTP)
        xEventGroupSetBits(dispod_event_group, DISPOD_NTP_ACTIVATED_BIT);
    if(CONFIG_DISPOD_USE_SD)
        xEventGroupSetBits(dispod_event_group, DISPOD_SD_ACTIVATED_BIT);
    if(CONFIG_USE_BLE)
        xEventGroupSetBits(dispod_event_group, DISPOD_BLE_ACTIVATED_BIT);
}

static void initialize_spiffs()
{
    esp_err_t ret;

    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = false
    };

    ESP_LOGI(TAG, "SPIFFS: calling esp_vfs_spiffs_register()");
    ret = esp_vfs_spiffs_register(&conf);
    ESP_LOGI(TAG, "SPIFFS: esp_vfs_spiffs_register() returned");

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "SPIFFS: Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "SPIFFS: Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "SPIFFS: Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPIFFS: Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "SPIFFS: Partition size: total: %d, used: %d", total, used);
    }
}

static void initialize_nvs()
{
    esp_err_t ret;

    // Initialize NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );
}

void dispod_m5stack_task(void *pvParameters){
    ESP_LOGI(TAG, "dispod_m5stack_task: started");

    for(;;){
        M5.update();
        dispod_m5_buttons_test();
    }
}

/*
 * Timer group0 ISR handler
 *
 * Note:
 * We don't call the timer API here because they are not declared with IRAM_ATTR.
 * If we're okay with the timer irq not being serviced while SPI flash cache is disabled,
 * we can allocate this interrupt without the ESP_INTR_FLAG_IRAM flag and use the normal API.
 */
void IRAM_ATTR timer_group0_isr(void *para)
{
    BaseType_t xHigherPriorityTaskWoken, xResult;

  /* xHigherPriorityTaskWoken must be initialised to pdFALSE. */
    xHigherPriorityTaskWoken = pdFALSE;

    int timer_idx = (int) para;

    /* Retrieve the interrupt status and the counter value
       from the timer that reported the interrupt */
    uint32_t intr_status = TIMERG0.int_st_timers.val;
    TIMERG0.hw_timer[timer_idx].update = 1;
    uint64_t timer_counter_value =
        ((uint64_t) TIMERG0.hw_timer[timer_idx].cnt_high) << 32
        | TIMERG0.hw_timer[timer_idx].cnt_low;

    /* Prepare basic event data
       that will be then sent back to the main program task */
    timer_event_t evt;
    evt.timer_group = 0;
    evt.timer_idx = timer_idx;
    evt.timer_counter_value = timer_counter_value;

    /* Clear the interrupt
       and update the alarm time for the timer with without reload */
    if ((intr_status & BIT(timer_idx)) && timer_idx == TIMER_0) {
        evt.type = TEST_WITHOUT_RELOAD;
        TIMERG0.int_clr_timers.t0 = 1;
        timer_counter_value += (uint64_t) (TIMER_INTERVAL0_SEC * TIMER_SCALE);
        TIMERG0.hw_timer[timer_idx].alarm_high = (uint32_t) (timer_counter_value >> 32);
        TIMERG0.hw_timer[timer_idx].alarm_low = (uint32_t) timer_counter_value;
    } else if ((intr_status & BIT(timer_idx)) && timer_idx == TIMER_1) {
        evt.type = TEST_WITH_RELOAD;
        TIMERG0.int_clr_timers.t1 = 1;
    } else {
        evt.type = -1; // not supported even type
    }

    /* After the alarm has been triggered
      we need enable it again, so it is triggered the next time */
    TIMERG0.hw_timer[timer_idx].config.alarm_en = TIMER_ALARM_EN;

    /* Now just send the event data back to the main program task */
    // xQueueSendFromISR(timer_queue, &evt, NULL);
    xResult = xEventGroupSetBitsFromISR(dispod_timer_evg, DISPOD_TIMER_METRONOME_BIT, &xHigherPriorityTaskWoken);

    /* Was the message posted successfully? */
    if( xResult == pdPASS )
    {
        /* If xHigherPriorityTaskWoken is now set to pdTRUE then a context
        switch should be requested.  The macro used is port specific and will
        be either portYIELD_FROM_ISR() or portEND_SWITCHING_ISR() - refer to
        the documentation page for the port being used. */
        // portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
        portYIELD_FROM_ISR();
  }
}

/*
 * Initialize selected timer of the timer group 0
 *
 * timer_idx - the timer number to initialize
 * auto_reload - should the timer auto reload on alarm?
 * timer_interval_sec - the interval of alarm to set
 */
static void example_tg0_timer_init(int timer_idx, bool auto_reload, double timer_interval_sec)
{
    /* Select and initialize basic parameters of the timer */
    timer_config_t config;
    config.divider      = TIMER_DIVIDER;
    config.counter_dir  = TIMER_COUNT_UP;
    config.counter_en   = TIMER_PAUSE;
    config.alarm_en     = TIMER_ALARM_EN;
    config.intr_type    = TIMER_INTR_LEVEL;
    config.auto_reload  = auto_reload;
    timer_init(TIMER_GROUP_0, (timer_idx_t) timer_idx, &config);

    /* Timer's counter will initially start from value below.
       Also, if auto_reload is set, this value will be automatically reload on alarm */
    timer_set_counter_value(TIMER_GROUP_0, (timer_idx_t) timer_idx, 0x00000000ULL);

    /* Configure the alarm value and the interrupt on alarm. */
    timer_set_alarm_value(TIMER_GROUP_0,(timer_idx_t)  timer_idx, timer_interval_sec * TIMER_SCALE);
    timer_enable_intr(TIMER_GROUP_0, (timer_idx_t) timer_idx);
    timer_isr_register(TIMER_GROUP_0, (timer_idx_t) timer_idx, timer_group0_isr,
        (void *) timer_idx, ESP_INTR_FLAG_IRAM, NULL);

    timer_start(TIMER_GROUP_0, (timer_idx_t) timer_idx);
}


static void run_on_event(void* handler_arg, esp_event_base_t base, int32_t id, void* event_data)
{
    switch(id){
    case DISPOD_STARTUP_EVT:
        ESP_LOGI(TAG, "DISPOD_STARTUP_EVT");

        // Initialize the M5Stack object and the M5Stack NeoPixels
        M5.begin(true, true, true); // LCD, SD, Serial
        M5.Speaker.setBeep(1000, 40);
        // ledcWrite(TONE_PIN_CHANNEL, 3);  // TODO DUTY
        xEventGroupClearBits(dispod_event_group, DISPOD_METRO_SOUND_ACT_BIT);
        xEventGroupClearBits(dispod_event_group, DISPOD_METRO_LIGHT_ACT_BIT);
#ifdef ADAFRUIT_NEOPIXEL
        pixels.begin();
        pixels.show();
#endif
#ifdef NEOPIXELBUS
        pixels.Begin();
        pixels.Show();
#endif
        dispod_initialize();
        dispod_screen_status_initialize(&dispod_screen_status);
        xEventGroupSetBits(dispod_display_evg, DISPOD_DISPLAY_UPDATE_BIT);
        gpio_install_isr_service(0);
        // dispod_button_initialize();
        dispod_runvalues_initialize(&running_values);
        dispod_archiver_initialize();
        ESP_ERROR_CHECK(esp_event_post_to(dispod_loop_handle, WORKFLOW_EVENTS, DISPOD_BASIC_INIT_DONE_EVT, NULL, 0, portMAX_DELAY));
        break;
    case DISPOD_BASIC_INIT_DONE_EVT:
        ESP_LOGI(TAG, "DISPOD_BASIC_INIT_DONE_EVT");
        dispod_wifi_network_init();
        // show splash and some info here
        ESP_ERROR_CHECK(esp_event_post_to(dispod_loop_handle, WORKFLOW_EVENTS, DISPOD_SPLASH_AND_NETWORK_INIT_DONE_EVT, NULL, 0, portMAX_DELAY));
        break;
    case DISPOD_SPLASH_AND_NETWORK_INIT_DONE_EVT:
        ESP_LOGI(TAG, "DISPOD_DISPLAY_INIT_DONE_EVT");
        uxBits = xEventGroupWaitBits(dispod_event_group, DISPOD_WIFI_ACTIVATED_BIT, pdFALSE, pdFALSE, 0);
        if(uxBits & DISPOD_WIFI_ACTIVATED_BIT){
            // WiFi activated -> connect to WiFi
            ESP_LOGI(TAG, "connect to WiFi");
            dispod_wifi_network_up();
            ESP_ERROR_CHECK(esp_event_post_to(dispod_loop_handle, WORKFLOW_EVENTS, DISPOD_WIFI_INIT_DONE_EVT, NULL, 0, portMAX_DELAY));
        } else {
            // WiFi not activated (and thus no NTP), jump to SD mount
            ESP_LOGI(TAG, "no WiFi configured (thus no NTP), mount SD next");
            ESP_ERROR_CHECK(esp_event_post_to(dispod_loop_handle, WORKFLOW_EVENTS, DISPOD_NTP_INIT_DONE_EVT, NULL, 0, portMAX_DELAY));
        }
        break;
    case DISPOD_WIFI_INIT_DONE_EVT:
        ESP_LOGI(TAG, "DISPOD_WIFI_INIT_DONE_EVT");
        break;
    case DISPOD_WIFI_GOT_IP_EVT:
        ESP_LOGI(TAG, "DISPOD_WIFI_GOT_IP_EVT");
        xEventGroupSetBits(dispod_display_evg, DISPOD_DISPLAY_UPDATE_BIT);
        uxBits = xEventGroupWaitBits(dispod_event_group, DISPOD_WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, 0);
        if(uxBits & DISPOD_WIFI_CONNECTED_BIT){
            ESP_LOGI(TAG, "WiFi connected, update NTP");
            dispod_sntp_check_time();
            ESP_ERROR_CHECK(esp_event_post_to(dispod_loop_handle, WORKFLOW_EVENTS, DISPOD_NTP_INIT_DONE_EVT, NULL, 0, portMAX_DELAY));
        } else {
            // WiFi configured, but not connected, jump to SD mount
            ESP_LOGI(TAG, "no WiFi connection thus no NTP, connect to BLE next");
            ESP_ERROR_CHECK(esp_event_post_to(dispod_loop_handle, WORKFLOW_EVENTS, DISPOD_NTP_INIT_DONE_EVT, NULL, 0, portMAX_DELAY));
        }
        break;
    case DISPOD_NTP_INIT_DONE_EVT:
        ESP_LOGI(TAG, "DISPOD_NTP_INIT_DONE_EVT");
        uxBits = xEventGroupWaitBits(dispod_event_group, DISPOD_SD_ACTIVATED_BIT, pdFALSE, pdFALSE, 0);
        ESP_LOGI(TAG, "uxBits: DISPOD_SD_ACTIVATED_BIT = %u, uxBits = %u", DISPOD_SD_ACTIVATED_BIT, uxBits);
        if( ( uxBits & DISPOD_SD_ACTIVATED_BIT) == DISPOD_SD_ACTIVATED_BIT){
            ESP_LOGI(TAG, "DISPOD_NTP_INIT_DONE_EVT: dispod_sd_evg DISPOD_SD_PROBE_EVT");
            xEventGroupSetBits(dispod_sd_evg, DISPOD_SD_PROBE_EVT);
        } else {
            ESP_LOGI(TAG, "DISPOD_NTP_INIT_DONE_EVT: skip DISPOD_SD_PROBE_EVT");
            ESP_ERROR_CHECK(esp_event_post_to(dispod_loop_handle, WORKFLOW_EVENTS, DISPOD_SD_INIT_DONE_EVT, NULL, 0, portMAX_DELAY));
        }
        break;
    case DISPOD_SD_INIT_DONE_EVT:
        ESP_LOGI(TAG, "DISPOD_SD_INIT_DONE_EVT");
        uxBits = xEventGroupWaitBits(dispod_event_group, DISPOD_BLE_RETRY_BIT, pdTRUE, pdFALSE, 0);
        ESP_LOGI(TAG, "uxBits: DISPOD_BLE_RETRY_BIT = %u, uxBits = %u", DISPOD_BLE_RETRY_BIT, uxBits);
        if(!(uxBits & DISPOD_BLE_RETRY_BIT)){
            dispod_ble_initialize();
            dispod_ble_app_register();
        } else {
            dispod_ble_start_scanning();    // TODO check whether it works with additional call to start_scanning.
        }
        break;
    case DISPOD_BLE_DEVICE_DONE_EVT:
        ESP_LOGI(TAG, "DISPOD_BLE_DEVICE_DONE_EVT");
        ESP_ERROR_CHECK(esp_event_post_to(dispod_loop_handle, WORKFLOW_EVENTS, DISPOD_STARTUP_COMPLETE_EVT, NULL, 0, portMAX_DELAY));
        break;
    case DISPOD_STARTUP_COMPLETE_EVT:{
        bool retryWifi = false;
        bool retryBLE = false;
        bool cont = false;
        ESP_LOGI(TAG, "DISPOD_STARTUP_COMPLETE_EVT");
        // at this point we've either
        // - no Wifi configured: no WiFi, no NTP, maybe BLE
        // - WiFi configured: maybe WiFi -> maybe updated NTP, maybe BLE

        // no WiFi but retry set -> jump to WiFi again (DISPOD_DISPLAY_INIT_DONE_EVT)
        if(!(xEventGroupWaitBits(dispod_event_group, DISPOD_WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, 0) & DISPOD_WIFI_CONNECTED_BIT)){
            // option: no WiFi, retry WiFi -> Button A
            dispod_screen_status_update_button(&dispod_screen_status, BUTTON_A, true, "WiFi");
            xEventGroupSetBits(dispod_event_group, DISPOD_BTN_A_RETRY_WIFI_BIT);
            retryWifi = true;
        }
        if(!(xEventGroupWaitBits(dispod_event_group, DISPOD_BLE_CONNECTED_BIT, pdFALSE, pdFALSE, 0) & DISPOD_BLE_CONNECTED_BIT)){
            // option: no BLE, retry BLE -> Button B
            dispod_screen_status_update_button(&dispod_screen_status, BUTTON_B, true, "BLE");
            xEventGroupSetBits(dispod_event_group, DISPOD_BTN_B_RETRY_BLE_BIT);
            retryBLE = true;
        }
        if((xEventGroupWaitBits(dispod_event_group, DISPOD_BLE_CONNECTED_BIT, pdFALSE, pdFALSE, 0) & DISPOD_BLE_CONNECTED_BIT)){
            // option: BLE avail, go to running screen -> Button C
            dispod_screen_status_update_button(&dispod_screen_status, BUTTON_C, true, "Cont.");
            xEventGroupSetBits(dispod_event_group, DISPOD_BTN_C_CNT_BIT);
            cont = true;
        }

        if(retryWifi && retryBLE && (!cont)){
            dispod_screen_status_update_statustext(&dispod_screen_status, true, "Retry WiFi or BLE?");
        } else if(retryWifi && (!retryBLE) && cont){
            dispod_screen_status_update_statustext(&dispod_screen_status, true, "Retry WiFi or continue?");
        } else if(retryBLE && (!retryWifi) && (!cont)){
            dispod_screen_status_update_statustext(&dispod_screen_status, true, "Retry BLE?");
        } else if((!retryWifi) && (!retryBLE) && cont){
            dispod_screen_status_update_statustext(&dispod_screen_status, true, "Continue?");
        }
        xEventGroupSetBits(dispod_display_evg, DISPOD_DISPLAY_UPDATE_BIT);
        // Debug NeoPixels "green"
        // for(int i = 0; i < 10; i++)
        //     ESP_LOGI(TAG, "NeoPixels(%d) = %ul", i, pixels.getPixelColor(i));
        //
        }
        break;
    case DISPOD_RETRY_WIFI_EVT:
        ESP_LOGI(TAG, "DISPOD_RETRY_WIFI_EVT");
        break;
    case DISPOD_RETRY_BLE_EVT:
        ESP_LOGI(TAG, "DISPOD_RETRY_BLE_EVT");
        break;
    case DISPOD_GO_TO_RUNNING_SCREEN_EVT:
        ESP_LOGI(TAG, "DISPOD_GO_TO_RUNNING_SCREEN_EVT");
        xEventGroupSetBits(dispod_event_group, DISPOD_RUNNING_SCREEN_BIT);
        dispod_screen_change(&dispod_screen_status, SCREEN_RUNNING);
        dispod_screen_status_update_button(&dispod_screen_status, BUTTON_A, true, "Beep");
        dispod_screen_status_update_button(&dispod_screen_status, BUTTON_B, true, "Flash");
        dispod_screen_status_update_button(&dispod_screen_status, BUTTON_C, true, "Back");
        xEventGroupSetBits(dispod_display_evg, DISPOD_DISPLAY_UPDATE_BIT);
        break;
    //
    case DISPOD_BUTTON_TAP_EVT: {
        button_unit_t button_unit = *(button_unit_t*) event_data;
        ESP_LOGI(TAG, "DISPOD_BUTTON_TAP_EVT, button id %d", button_unit.btn_id);

        // come here from DISPOD_STARTUP_COMPLETE_EVT
        if((xEventGroupWaitBits(dispod_event_group, DISPOD_BTN_A_RETRY_WIFI_BIT | DISPOD_BTN_B_RETRY_BLE_BIT | DISPOD_BTN_C_CNT_BIT, pdFALSE, pdFALSE, 0)
                & (DISPOD_BTN_A_RETRY_WIFI_BIT | DISPOD_BTN_B_RETRY_BLE_BIT | DISPOD_BTN_C_CNT_BIT))){
            switch(button_unit.btn_id){
            case BUTTON_A:
                if((xEventGroupWaitBits(dispod_event_group, DISPOD_BTN_A_RETRY_WIFI_BIT, pdFALSE, pdFALSE, 0) & DISPOD_BTN_A_RETRY_WIFI_BIT)){
                    xEventGroupClearBits(dispod_event_group, DISPOD_BTN_A_RETRY_WIFI_BIT | DISPOD_BTN_B_RETRY_BLE_BIT | DISPOD_BTN_C_CNT_BIT);
                    xEventGroupSetBits(dispod_event_group, DISPOD_WIFI_RETRY_BIT);
                    dispod_screen_status_update_statustext(&dispod_screen_status, false, "");
                    dispod_screen_status_update_button(&dispod_screen_status, BUTTON_A, false, "");
                    dispod_screen_status_update_button(&dispod_screen_status, BUTTON_B, false, "");
                    dispod_screen_status_update_button(&dispod_screen_status, BUTTON_C, false, "");
                    ESP_ERROR_CHECK(esp_event_post_to(dispod_loop_handle, WORKFLOW_EVENTS, DISPOD_SPLASH_AND_NETWORK_INIT_DONE_EVT, NULL, 0, portMAX_DELAY));
                }
                break;
            case BUTTON_B:
                if((xEventGroupWaitBits(dispod_event_group, DISPOD_BTN_B_RETRY_BLE_BIT, pdFALSE, pdFALSE, 0) & DISPOD_BTN_B_RETRY_BLE_BIT)){
                    xEventGroupClearBits(dispod_event_group, DISPOD_BTN_A_RETRY_WIFI_BIT | DISPOD_BTN_B_RETRY_BLE_BIT | DISPOD_BTN_C_CNT_BIT);
                    xEventGroupSetBits(dispod_event_group, DISPOD_BLE_RETRY_BIT);
                    dispod_screen_status_update_statustext(&dispod_screen_status, false, "");
                    dispod_screen_status_update_button(&dispod_screen_status, BUTTON_A, false, "");
                    dispod_screen_status_update_button(&dispod_screen_status, BUTTON_B, false, "");
                    dispod_screen_status_update_button(&dispod_screen_status, BUTTON_C, false, "");
                    ESP_ERROR_CHECK(esp_event_post_to(dispod_loop_handle, WORKFLOW_EVENTS, DISPOD_SD_INIT_DONE_EVT, NULL, 0, portMAX_DELAY));
                }
                break;
            case BUTTON_C:
                if((xEventGroupWaitBits(dispod_event_group, DISPOD_BTN_C_CNT_BIT, pdFALSE, pdFALSE, 0) & DISPOD_BTN_C_CNT_BIT)){
                    xEventGroupClearBits(dispod_event_group, DISPOD_BTN_A_RETRY_WIFI_BIT | DISPOD_BTN_B_RETRY_BLE_BIT | DISPOD_BTN_C_CNT_BIT);
                    dispod_screen_status_update_statustext(&dispod_screen_status, false, "");
                    dispod_screen_status_update_button(&dispod_screen_status, BUTTON_A, false, "");
                    dispod_screen_status_update_button(&dispod_screen_status, BUTTON_B, false, "");
                    dispod_screen_status_update_button(&dispod_screen_status, BUTTON_C, false, "");
                    ESP_ERROR_CHECK(esp_event_post_to(dispod_loop_handle, WORKFLOW_EVENTS, DISPOD_GO_TO_RUNNING_SCREEN_EVT, NULL, 0, portMAX_DELAY));
                }
                break;
            default:
                ESP_LOGI(TAG, "unhandled button");
                break;
            }
        }
        // showing running screen
        if((xEventGroupWaitBits(dispod_event_group, DISPOD_RUNNING_SCREEN_BIT, pdFALSE, pdFALSE, 0) & DISPOD_RUNNING_SCREEN_BIT)){
            switch(button_unit.btn_id){
            case BUTTON_A:
                // Toggle Metronome/Sound
                if((xEventGroupWaitBits(dispod_event_group, DISPOD_METRO_SOUND_ACT_BIT, pdFALSE, pdFALSE, 0) & DISPOD_METRO_SOUND_ACT_BIT)){
                    xEventGroupClearBits(dispod_event_group, DISPOD_METRO_SOUND_ACT_BIT);
                } else {
                    xEventGroupSetBits(dispod_event_group, DISPOD_METRO_SOUND_ACT_BIT);
                }

                break;
            case BUTTON_B:
                // Toggle Metronome/Light
                if((xEventGroupWaitBits(dispod_event_group, DISPOD_METRO_LIGHT_ACT_BIT, pdFALSE, pdFALSE, 0) & DISPOD_METRO_LIGHT_ACT_BIT)){
                    xEventGroupClearBits(dispod_event_group, DISPOD_METRO_LIGHT_ACT_BIT);
#ifdef ADAFRUIT_NEOPIXEL
                    pixels.clear();
                    pixels.show();
#endif
#ifdef NEOPIXELBUS
                    pixels.ClearTo(NEOPIXEL_black);
                    pixels.Show();
#endif


                } else {
                    xEventGroupSetBits(dispod_event_group, DISPOD_METRO_LIGHT_ACT_BIT);
                }
                break;
            case BUTTON_C:
                xEventGroupClearBits(dispod_event_group, DISPOD_RUNNING_SCREEN_BIT);
                xEventGroupClearBits(dispod_event_group, DISPOD_METRO_SOUND_ACT_BIT);
                xEventGroupClearBits(dispod_event_group, DISPOD_METRO_LIGHT_ACT_BIT);
#ifdef ADAFRUIT_NEOPIXEL
                    pixels.clear();
                    pixels.show();
#endif
#ifdef NEOPIXELBUS
                    pixels.ClearTo(NEOPIXEL_black);
                    pixels.Show();
#endif
                dispod_screen_change(&dispod_screen_status, SCREEN_STATUS);
                dispod_screen_status_update_statustext(&dispod_screen_status, false, "");
                dispod_screen_status_update_button(&dispod_screen_status, BUTTON_A, false, "");
                dispod_screen_status_update_button(&dispod_screen_status, BUTTON_B, false, "");
                dispod_screen_status_update_button(&dispod_screen_status, BUTTON_C, false, "");
                xEventGroupSetBits(dispod_display_evg, DISPOD_DISPLAY_UPDATE_BIT);
                ESP_ERROR_CHECK(esp_event_post_to(dispod_loop_handle, WORKFLOW_EVENTS, DISPOD_STARTUP_COMPLETE_EVT, NULL, 0, portMAX_DELAY));
                break;
            default:
                ESP_LOGI(TAG, "unhandled button");
                break;
            }
        }
        }
        break;
    case DISPOD_BUTTON_2SEC_PRESS_EVT: {
        button_unit_t* button_unit = (button_unit_t*) event_data;
        ESP_LOGI(TAG, "DISPOD_BUTTON_2SEC_PRESS_EVT, button id %d", button_unit->btn_id);
        }
        break;
    case DISPOD_BUTTON_5SEC_PRESS_EVT: {
        button_unit_t* button_unit = (button_unit_t*) event_data;
        ESP_LOGI(TAG, "DISPOD_BUTTON_5SEC_PRESS_EVT, button id %d", button_unit->btn_id);
        }
        break;
    default:
        ESP_LOGI(TAG, "unhandled event base/id %s:%d", base, id);
        break;
    }
}

extern "C" void app_main()
{
    ESP_LOGI(TAG, "app_main() entered");

	// adjust logging
	esp_log_level_set("phy_init",       ESP_LOG_INFO);
	esp_log_level_set("nvs",            ESP_LOG_INFO);
	esp_log_level_set("tcpip_adapter",  ESP_LOG_INFO);
	esp_log_level_set("BTDM_INIT",      ESP_LOG_INFO);

    // initialize NVS
    initialize_nvs();

    // Initialize SPIFFS file system
    initialize_spiffs();

    // disPOD overall initialization
    ESP_LOGI(TAG, "initialize dispod");

    // event groups
    dispod_event_group  = xEventGroupCreate();
    dispod_display_evg  = xEventGroupCreate();
    dispod_sd_evg       = xEventGroupCreate();
    dispod_timer_evg    = xEventGroupCreate();

    // create running values queue to get BLE notification decoded and put into this queue
    running_values_queue = xQueueCreate( 10, sizeof( running_values_queue_element_t ) );

    esp_event_loop_args_t dispod_loop_args = {
        .queue_size = 5,
        .task_name = "loop_task",
        .task_priority = uxTaskPriorityGet(NULL),
        .task_stack_size = 4096,
        .task_core_id = tskNO_AFFINITY
    };

    // Create the dispod event loop
    ESP_ERROR_CHECK(esp_event_loop_create(&dispod_loop_args, &dispod_loop_handle));

    // Register the handler for task iteration event.
    ESP_ERROR_CHECK(esp_event_handler_register_with(dispod_loop_handle, ESP_EVENT_ANY_BASE, ESP_EVENT_ANY_ID, run_on_event, NULL));

    // run the display task with the same priority as the current process
    ESP_LOGI(TAG, "Starting dispod_screen_task()");
    xTaskCreate(dispod_screen_task, "dispod_screen_task", 4096, &dispod_screen_status, uxTaskPriorityGet(NULL), NULL);

    // run the updater task with the same priority as the current process
    ESP_LOGI(TAG, "Starting dispod_update_task()");
    xTaskCreate(dispod_update_task, "dispod_update_task", 4096, NULL, uxTaskPriorityGet(NULL), NULL);

    // run the archiver task with the same priority as the current process
    ESP_LOGI(TAG, "Starting dispod_archiver_task()");
    xTaskCreate(dispod_archiver_task, "dispod_archiver_task", 4096, NULL, uxTaskPriorityGet(NULL), NULL);

    // run the M5STack task
    ESP_LOGI(TAG, "Starting dispod_m5stack_task()");
    xTaskCreate(dispod_m5stack_task, "dispod_m5stack_task", 4096, NULL, uxTaskPriorityGet(NULL), NULL);

    // run the timer task and the timer (TODO)
    ESP_LOGI(TAG, "Starting dispod_timer_task()");
    xTaskCreate(dispod_timer_task, "dispod_timer_task", 4096, NULL, uxTaskPriorityGet(NULL), NULL);
    example_tg0_timer_init(TIMER_1, TEST_WITH_RELOAD, TIMER_INTERVAL0_SEC);

    // push a startup event in the loop
    ESP_ERROR_CHECK(esp_event_post_to(dispod_loop_handle, WORKFLOW_EVENTS, DISPOD_STARTUP_EVT, NULL, 0, portMAX_DELAY));
}
