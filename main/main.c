// disPOD - connect to MilestonePod via BLE and read data and display on M5Stack-Fire

#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "esp_err.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "iot_button.h"

#include "dispod_main.h"
#include "dispod_wifi.h"
#include "dispod_gattc.h"
#include "dispod_tft.h"
#include "dispod_sntp.h"
#include "dispod_ledc.h"
#include "dispod_runvalues.h"
#include "dispod_update.h"
#include "dispod_archiver.h"

static const char* TAG = "DISPOD";

// Event group
//
EventGroupHandle_t dispod_event_group;

// Event loop
ESP_EVENT_DEFINE_BASE(ACTIVITY_EVENTS);
ESP_EVENT_DEFINE_BASE(WORKFLOW_EVENTS);
esp_event_loop_handle_t dispod_loop_handle;

// Storing values from BLE running device
runningValuesStruct_t running_values;

// runvalues queue
QueueHandle_t running_values_queue;

// storing screen information
dispod_screen_status_t dispod_screen_info;

// time to wait in
const TickType_t xTicksToWait = 100 / portTICK_PERIOD_MS;

// temp return value from xEventGroupWaitBits, ... functions
EventBits_t uxBits;

// temp to don't have it twice
const char* otaErrorNames[] = {
	"Error: Auth Failed",		// OTA_AUTH_ERROR
	"Error: Begin Failed",		// OTA_BEGIN_ERROR
	"Error: Connect Failed",	// OTA_CONNECT_ERROR
	"Error: Receive Failed",	// OTA_RECEIVE_ERROR
	"Error: End Failed",		// OTA_END_ERROR
};

static void run_on_event(void* handler_arg, esp_event_base_t base, int32_t id, void* event_data)
{
    switch(id){
    case DISPOD_STARTUP_EVT:
        ESP_LOGI(TAG, "DISPOD_STARTUP_EVT");
        dispod_runvalues_initialize(&running_values);
        dispod_archiver_initialize();
        xEventGroupSetBits(dispod_event_group, DISPOD_WIFI_ACTIVATED_BIT);
        xEventGroupSetBits(dispod_event_group, DISPOD_NTP_ACTIVATED_BIT);
        xEventGroupSetBits(dispod_event_group, DISPOD_BLE_ACTIVATED_BIT);
        xEventGroupSetBits(dispod_event_group, DISPOD_SD_ACTIVATED_BIT);
        ESP_ERROR_CHECK(esp_event_post_to(dispod_loop_handle, WORKFLOW_EVENTS, DISPOD_BASIC_INIT_DONE_EVT, NULL, 0, portMAX_DELAY));
        break;
    case DISPOD_BASIC_INIT_DONE_EVT:
        ESP_LOGI(TAG, "DISPOD_BASIC_INIT_DONE_EVT");
        dispod_display_initialize();
        dispod_screen_data_initialize(&dispod_screen_info);
        xEventGroupSetBits(dispod_event_group, DISPOD_SCREEN_STATUS_BIT);
        xEventGroupSetBits(dispod_event_group, DISPOD_SCREEN_COMPLETE_BIT);
        // ESP_ERROR_CHECK(esp_event_post_to(dispod_loop_handle, ACTIVITY_EVENTS, DISPOD_DISPLAY_UPDATE_EVT, NULL, 0, portMAX_DELAY));
        xEventGroupSetBits(dispod_display_evg, DISPOD_DISPLAY_UPDATE_BIT | DISPOD_DISPLAY_COMPLETE_UPDATE_BIT);
        ESP_ERROR_CHECK(esp_event_post_to(dispod_loop_handle, WORKFLOW_EVENTS, DISPOD_DISPLAY_INIT_DONE_EVT, NULL, 0, portMAX_DELAY));
        break;
    case DISPOD_DISPLAY_INIT_DONE_EVT:
        ESP_LOGI(TAG, "DISPOD_DISPLAY_INIT_DONE_EVT");
        uxBits = xEventGroupWaitBits(dispod_event_group, DISPOD_WIFI_ACTIVATED_BIT, pdFALSE, pdFALSE, 0);
        if(uxBits & DISPOD_WIFI_ACTIVATED_BIT){
            // WiFi config activated -> connect to WiFi
            ESP_LOGI(TAG, "connect to WiFi");
            dispod_wifi_network_up();
            ESP_ERROR_CHECK(esp_event_post_to(dispod_loop_handle, WORKFLOW_EVENTS, DISPOD_WIFI_INIT_DONE_EVT, NULL, 0, portMAX_DELAY));
        } else {
            // no WiFi and thus NTP configured, jump to BLE connect
            ESP_LOGI(TAG, "no WiFi configured, connect to BLE (w/o WiFi&NTP)");
            dispod_ble_initialize();
            ESP_ERROR_CHECK(esp_event_post_to(dispod_loop_handle, WORKFLOW_EVENTS, DISPOD_BLE_DEVICE_DONE_EVT, NULL, 0, portMAX_DELAY));
        }
        break;
    case DISPOD_WIFI_INIT_DONE_EVT:
        ESP_LOGI(TAG, "DISPOD_WIFI_INIT_DONE_EVT");
        uxBits = xEventGroupWaitBits(dispod_event_group, DISPOD_WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, 0);
        if(uxBits & DISPOD_WIFI_CONNECTED_BIT){
            ESP_LOGI(TAG, "WiFi connected, update NTP");
            dispod_sntp_check_time();
            ESP_ERROR_CHECK(esp_event_post_to(dispod_loop_handle, WORKFLOW_EVENTS, DISPOD_NTP_INIT_DONE_EVT, NULL, 0, portMAX_DELAY));
        } else {
            // WiFi configured, but not connected, jump to BLE connect
            ESP_LOGI(TAG, "no WiFi connection thus no NTP, connect to BLE (w/o WiFi&NTP)");
            dispod_ble_initialize();
            ESP_ERROR_CHECK(esp_event_post_to(dispod_loop_handle, WORKFLOW_EVENTS, DISPOD_BLE_DEVICE_DONE_EVT, NULL, 0, portMAX_DELAY));
        }
        break;
    case DISPOD_NTP_INIT_DONE_EVT:
            ESP_LOGI(TAG, "DISPOD_NTP_INIT_DONE_EVT");
            dispod_ble_initialize();
            ESP_ERROR_CHECK(esp_event_post_to(dispod_loop_handle, WORKFLOW_EVENTS, DISPOD_BLE_DEVICE_DONE_EVT, NULL, 0, portMAX_DELAY));
        break;
    case DISPOD_BLE_DEVICE_DONE_EVT:
        // at this point we've either
        // - no Wifi configured: no WiFi, no NTP, maybe BLE
        // - WiFi configured: maybe WiFi -> maybe updated NTP, maybe BLE

        break;
    case DISPOD_LEAVE_SCREEN_EVT:
        break;
    case DISPOD_ENTER_SCREEN_EVT:
        break;
    case DISPOD_GO_SHUTDOWN_EVT:
        break;
    case DISPOD_GO_SLEEP_EVT:
        break;
    // ACTIVITY_EVENTS
    case DISPOD_WIFI_SCANNING_EVT:
        dispod_screen_info.wifi_status = WIFI_SCANNING;
        // ESP_ERROR_CHECK(esp_event_post_to(dispod_loop_handle, ACTIVITY_EVENTS, DISPOD_DISPLAY_UPDATE_EVT, NULL, 0, portMAX_DELAY));
        xEventGroupSetBits(dispod_display_evg, DISPOD_DISPLAY_UPDATE_BIT);

        break;
    case DISPOD_WIFI_CONNECTING_BIT:
        dispod_screen_info.wifi_status = WIFI_CONNECTING;
        // ESP_ERROR_CHECK(esp_event_post_to(dispod_loop_handle, ACTIVITY_EVENTS, DISPOD_DISPLAY_UPDATE_EVT, NULL, 0, portMAX_DELAY));
        xEventGroupSetBits(dispod_display_evg, DISPOD_DISPLAY_UPDATE_BIT);
        break;
    case DISPOD_WIFI_CONNECTED_BIT:
        dispod_screen_info.wifi_status = WIFI_CONNECTED;

        // ESP_ERROR_CHECK(esp_event_post_to(dispod_loop_handle, ACTIVITY_EVENTS, DISPOD_DISPLAY_UPDATE_EVT, NULL, 0, portMAX_DELAY));
        xEventGroupSetBits(dispod_display_evg, DISPOD_DISPLAY_UPDATE_BIT);
        break;

    case DISPOD_DISPLAY_UPDATE_EVT:
        ESP_LOGI(TAG, "DISPOD_DISPLAY_UPDATE_EVT, MUST NOT RUN!");

        // uxBits = xEventGroupWaitBits(dispod_event_group, DISPOD_SCREEN_COMPLETE_BIT, pdTRUE, pdFALSE, 0);
        // ESP_LOGI(TAG, "DISPOD_DISPLAY_UPDATE_EVT, complete %u", (uxBits & DISPOD_SCREEN_COMPLETE_BIT)?1:0);
        // dispod_screen_status_update_display(&dispod_screen_info, (uxBits & DISPOD_SCREEN_COMPLETE_BIT));
        break;
    default:
        ESP_LOGI(TAG, "unhandled event base/id %s:%d", base, id);
        break;
    }
    // Two types of data can be passed in to the event handler: the handler specific data and the event-specific data.
    //
    // The handler specific data (handler_arg) is a pointer to the original data, therefore, the user should ensure that
    // the memory location it points to is still valid when the handler executes.
    //
    // The event-specific data (event_data) is a pointer to a deep copy of the original data, and is managed automatically.
    // int iteration = *((int*) event_data);

    // char* loop;

    // if (handler_args  == loop_with_task) {
    //     loop = "loop_with_task";
    // } else {
    //     loop = "loop_without_task";
    // }
}


void app_main()
{
    esp_err_t ret;

    ESP_LOGI(TAG, "app_main() entered");

	// adjust logging, TODO still necessary?
	esp_log_level_set("phy_init", ESP_LOG_INFO);

    // Initialize NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    // Initialize SPIFFS file system
#ifdef CONFIG_DISPOD_USE_SPIFFS
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
#endif // CONFIG_DISPOD_USE_SPIFFS

    // disPOD overall initialization
    ESP_LOGI(TAG, "initialize dispod");
    dispod_event_group = xEventGroupCreate();
    dispod_display_evg = xEventGroupCreate();   // TODO just a test because later we got an assert() ?!

    // create running values queue to get BLE notification decoded and put into this queue
    ESP_LOGI(TAG, "sizeof(running_values_queue_element_t) = %u", sizeof(running_values_queue_element_t));
    running_values_queue = xQueueCreate( 10, sizeof( running_values_queue_element_t ) );

    ESP_LOGI(TAG, "sizeof(buffer_element_t) = %u", sizeof(buffer_element_t));

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
    // TODO check last argument...
    ESP_ERROR_CHECK(esp_event_handler_register_with(dispod_loop_handle, ESP_EVENT_ANY_BASE, ESP_EVENT_ANY_ID, run_on_event, NULL));

    // run the display task with the same priority as the current process
    ESP_LOGI(TAG, "Starting dispod_screen_task()");
    xTaskCreate(dispod_screen_task, "dispod_screen_task", 4096, &dispod_screen_info, uxTaskPriorityGet(NULL), NULL);

    // run the updater task with the same priority as the current process
    ESP_LOGI(TAG, "Starting dispod_update_task()");
    xTaskCreate(dispod_update_task, "dispod_update_task", 4096, NULL, uxTaskPriorityGet(NULL), NULL);

    // run the archiver task with the same priority as the current process
    ESP_LOGI(TAG, "Starting dispod_archiver_task()");
    xTaskCreate(dispod_archiver_task, "dispod_archiver_task", 4096, NULL, uxTaskPriorityGet(NULL), NULL);

    // push a startup event in the loop
    ESP_ERROR_CHECK(esp_event_post_to(dispod_loop_handle, WORKFLOW_EVENTS, DISPOD_STARTUP_EVT, NULL, 0, portMAX_DELAY));
}
