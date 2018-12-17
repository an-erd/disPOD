// disPOD - connect to BLE to read MilestonePod data and display on M5Stack-Fire

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_spiffs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "lwip/sys.h"
#include "lwip/err.h"
#include "lwip/err.h"
#include "lwip/arch.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "tftspi.h"
#include "tft.h"

#include "iot_button.h"

#include "dispod_config.h"
#include "dispod_wifi.h"
#include "dispod_gattc.h"
#include "dispod_tft.h"
#include "dispod_sntp.h"
#include "dispod_ledc.h"

// load fonts from ...?
// static const char* file_fonts[3] = {"/spiffs/fonts/DotMatrix_M.fon", "/spiffs/fonts/Ubuntu.fon", "/spiffs/fonts/Grotesk24x48.fon"};

static const char* TAG = "DISPOD";
static const char* TAG_BTN = "DISPOD_BUTTON";

EventGroupHandle_t dispod_event_group;

// Button callback functions
void button_tap_cb(void* arg)
{
    char* pstr = (char*) arg;
    ESP_EARLY_LOGI(TAG_BTN, "tap cb (%s), heap: %d\n", pstr, esp_get_free_heap_size());
}

void button_press_2s_cb(void* arg)
{
    ESP_EARLY_LOGI(TAG_BTN, "press 2s, heap: %d\n", esp_get_free_heap_size());
}

void button_press_5s_cb(void* arg)
{
    ESP_EARLY_LOGI(TAG_BTN, "press 5s, heap: %d\n", esp_get_free_heap_size());
}

void button_test()
{
    ESP_EARLY_LOGI(TAG_BTN, "before btn init, heap: %d\n", esp_get_free_heap_size());

    button_handle_t btn_handle = iot_button_create(BUTTON_A_PIN, BUTTON_ACTIVE_LEVEL);

    iot_button_set_evt_cb(btn_handle, BUTTON_CB_PUSH, button_tap_cb, "PUSH");
    iot_button_set_evt_cb(btn_handle, BUTTON_CB_RELEASE, button_tap_cb, "RELEASE");
    iot_button_set_evt_cb(btn_handle, BUTTON_CB_TAP, button_tap_cb, "TAP");
    // iot_button_set_serial_cb(btn_handle, 2, 1000/portTICK_RATE_MS, button_tap_cb, "SERIAL");

    iot_button_add_custom_cb(btn_handle, 2, button_press_2s_cb, NULL);
    iot_button_add_custom_cb(btn_handle, 5, button_press_5s_cb, NULL);
    ESP_EARLY_LOGI(TAG_BTN, "after btn init, heap: %d\n", esp_get_free_heap_size());

    vTaskDelay(10000 / portTICK_PERIOD_MS);
    // printf("free btn: heap:%d\n", esp_get_free_heap_size());
    // iot_button_delete(btn_handle);
    // printf("after free btn: heap:%d\n", esp_get_free_heap_size());
}





// ================== TEST SD CARD ==========================================

#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"

// This example can use SDMMC and SPI peripherals to communicate with SD card.
// SPI mode IS USED

// When testing SD and SPI modes, keep in mind that once the card has been
// initialized in SPI mode, it can not be reinitialized in SD mode without
// toggling power to the card.

// Pin mapping when using SPI mode.
// With this mapping, SD card can be used both in SPI and 1-line SD mode.
// Note that a pull-up on CS line is required in SD mode.
#define sdPIN_NUM_MISO 19
#define sdPIN_NUM_MOSI 23
#define sdPIN_NUM_CLK  18
#define sdPIN_NUM_CS   4

static const char* TAG_SD = "SDCard test";

void test_sd_card(void)
{
    printf("\n=======================================================\n");
    printf("===== Test using SD Card in SPI mode              =====\n");
    printf("===== SD Card uses the same gpio's as TFT display =====\n");
    printf("=======================================================\n\n");
    ESP_LOGI(TAG_SD, "Initializing SD card");
    ESP_LOGI(TAG_SD, "Using SPI peripheral");

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    sdspi_slot_config_t slot_config = SDSPI_SLOT_CONFIG_DEFAULT();
    slot_config.gpio_miso = sdPIN_NUM_MISO;
    slot_config.gpio_mosi = sdPIN_NUM_MOSI;
    slot_config.gpio_sck  = sdPIN_NUM_CLK;
    slot_config.gpio_cs   = sdPIN_NUM_CS;
    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.

    // Options for mounting the filesystem.
    // If format_if_mount_failed is set to true, SD card will be partitioned and
    // formatted in case when mounting fails.
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5
    };

    // Use settings defined above to initialize SD card and mount FAT filesystem.
    // Note: esp_vfs_fat_sdmmc_mount is an all-in-one convenience function.
    // Please check its source code and implement error recovery when developing
    // production applications.
    sdmmc_card_t* card;
    esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG_SD, "Failed to mount filesystem. "
                "If you want the card to be formatted, set format_if_mount_failed = true.");
        } else {
            ESP_LOGE(TAG_SD, "Failed to initialize the card (%d). "
                "Make sure SD card lines have pull-up resistors in place.", ret);
        }
        return;
    }

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);

    // Use POSIX and C standard library functions to work with files.
    // First create a file.
    ESP_LOGI(TAG_SD, "Opening file");
    FILE* f = fopen("/sdcard/hello.txt", "w");
    if (f == NULL) {
        ESP_LOGE(TAG_SD, "Failed to open file for writing");
        return;
    }
    fprintf(f, "Hello %s!\n", card->cid.name);
    fclose(f);
    ESP_LOGI(TAG_SD, "File written");

    // Check if destination file exists before renaming
    struct stat st;
    if (stat("/sdcard/foo.txt", &st) == 0) {
        // Delete it if it exists
        unlink("/sdcard/foo.txt");
    }

    // Rename original file
    ESP_LOGI(TAG_SD, "Renaming file");
    if (rename("/sdcard/hello.txt", "/sdcard/foo.txt") != 0) {
        ESP_LOGE(TAG_SD, "Rename failed");
        return;
    }

    // Open renamed file for reading
    ESP_LOGI(TAG_SD, "Reading file");
    f = fopen("/sdcard/foo.txt", "r");
    if (f == NULL) {
        ESP_LOGE(TAG_SD, "Failed to open file for reading");
        return;
    }
    char line[64];
    fgets(line, sizeof(line), f);
    fclose(f);
    // strip newline
    char* pos = strchr(line, '\n');
    if (pos) {
        *pos = '\0';
    }
    ESP_LOGI(TAG_SD, "Read from file: '%s'", line);

    // All done, unmount partition and disable SDMMC or SPI peripheral
    esp_vfs_fat_sdmmc_unmount();
    ESP_LOGI(TAG_SD, "Card unmounted");

    printf("===== SD Card test end ================================\n\n");
}

// ================== TEST SD CARD ==========================================







void app_main()
{
    esp_err_t ret;

    ESP_LOGI(TAG, "app_main() entered");

	// adjust logging
	esp_log_level_set("phy_init", ESP_LOG_INFO);

    // disPOD overall initialization
    ESP_LOGI(TAG, "initialize_dispod");
    dispod_event_group = xEventGroupCreate();
    // TODO further initialization

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

    ESP_LOGI("INFO", "Reading file");
    FILE* f = fopen("/spiffs/tiger.bmp", "r");
    if (f == NULL) {
        ESP_LOGE("INFO", "Failed to open file for reading");
        return;
    }


    // All done, unmount partition and disable SPIFFS
    // esp_vfs_spiffs_unregister(NULL);
    // ESP_LOGI(TAG, "SPIFFS unmounted");
#endif // CONFIG_DISPOD_USE_SPIFFS

    dispod_display_initialize();
    dispod_wifi_network_up();
    dispod_sntp_check_time();
    dispod_ble_initialize();

    button_test();
    test_sd_card();

    // check sound
    // for (int i = 0; i < 15; i++) {
    //     ESP_LOGI(TAG, "play note, volume %u", 1+3*i);
    //     for (int j = 0; j < 1; j++){
    //         sound(SPEAKER_PIN, 660, 50, (uint32_t) 1+3*i);
    //         vTaskDelay(150/portTICK_PERIOD_MS);
    //     }
    // }
}
