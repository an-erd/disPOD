#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"

#include "dispod_archiver.h"
#include "dispod_config.h"

static const char* TAG = "DISPOD_ARCHIVER";

#define sdPIN_NUM_MISO 19
#define sdPIN_NUM_MOSI 23
#define sdPIN_NUM_CLK  18
#define sdPIN_NUM_CS   4

// data buffers
static buffer_element_t buffers[CONFIG_SDCARD_NUM_BUFFERS][CONFIG_SDCARD_BUFFER_SIZE];
static uint8_t current_buffer          = 0;
static uint8_t next_buffer_to_write    = 0;
static uint8_t seconds_to_write        = 0;
static uint32_t used_in_current_buffer = 0;


int write_out_any_buffers()
{
    ESP_LOGI(TAG, "Initializing SD card");
    ESP_LOGI(TAG, "Using SDMMC peripheral");
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
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
        // .allocation_unit_size = 16 * 1024        // TODO check option
    };

        // Use settings defined above to initialize SD card and mount FAT filesystem.
    // Note: esp_vfs_fat_sdmmc_mount is an all-in-one convenience function.
    // Please check its source code and implement error recovery when developing
    // production applications.
    sdmmc_card_t* card;
    esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                "If you want the card to be formatted, set format_if_mount_failed = true.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        return 0;
    }

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);

    // file handle and buffer to read
    FILE*   f;
    char    line[64];
    uint8_t file_nr;

    // Read the last file number from file. If it does not exist, create a new and start with "0".
    // If it exists, read the value and increase by 1.
    ESP_LOGI(TAG_SD, "Opening refernce numer file");
    struct stat st;
    if (stat("/sdcard/refnr.txt", &st) == 0) {
        f = fopen("/sdcard/refnr.txt", "r");
        if (f == NULL) {
            ESP_LOGE(TAG, "Failed to open file for reading");
            return;
        }
    } else {
        f = fopen("/sdcard/refnr.txt", "w");
        if (f == NULL) {
            ESP_LOGE(TAG, "Failed to open file for writing");
            return;
        }
        fprintf(f, "0\n");
        fclose(f);
        ESP_LOGI(TAG, "File written");

        f = fopen("/sdcard/refnr.txt", "r");
        if (f == NULL) {
            ESP_LOGE(TAG_SD, "Failed to open file for reading");
            return;
        }
    }

    fgets(line, sizeof(line), f);
    fclose(f);
    // strip newline
    char* pos = strchr(line, '\n');
    if (pos) {
        *pos = '\0';
    }
    ESP_LOGI(TAG, "Read from file: '%s'", line);
    sscanf(line, "%d", &file_nr);

    file_nr++;
    sprintf(line, CONFIG_SDCARD_FILE_NAME, file_nr);

    f = fopen(line, "a");
    if(f == NULL) {
        f = fopen(line, "w");
        if (f == NULL) {
            ESP_LOGE(TAG, "Failed to open file for writing");
            return 0;
        }
        ESP_LOGI(TAG, "Created new file");
    } else {
        ESP_LOGI(TAG, "Opening file");
    }

    // Attempt to write out any full buffers
    while(next_buffer_to_write != current_buffer) {
        if(fwrite(buffers[next_buffer_to_write], sizeof(buffer_element_t), CONFIG_SDCARD_BUFFER_SIZE, f) != CONFIG_SDCARD_BUFFER_SIZE) {
        ESP_LOGI(TAG, "Write fwrite(buffers[%u], ...) error", next_buffer_to_write);
        break;
        }
        next_buffer_to_write++;
        if(next_buffer_to_write == N_BUFFERS) {
            next_buffer_to_write = 0;
        }
    }

    ESP_LOGI(TAG, "Closing file");
    fclose(f);

    // All done, unmount partition and disable SDMMC or SPI peripheral
    esp_vfs_fat_sdmmc_unmount();

    return 1;
}

void dispod_archiver_task(void *pvParameters)
{
    ESP_LOGI(TAG, "dispod_archiver_task: started");
    EventBits_t uxBits;
    bool complete;
    // dispod_screen_status_t* params = (dispod_screen_status_t*)pvParameters;

    for (;;)
    {
        // while(!(xEventGroupWaitBits(dispod_display_evg, DISPOD_DISPLAY_UPDATE_BIT,
        //         pdTRUE, pdFALSE, portMAX_DELAY) & DISPOD_DISPLAY_UPDATE_BIT));
        // uxBits = xEventGroupWaitBits(dispod_display_evg, DISPOD_DISPLAY_COMPLETE_UPDATE_BIT,
        //         pdTRUE, pdFALSE, 0);
        // ESP_LOGI(TAG, "uxBits = xEventGroupWaitBits(dispod_display_evg, DISPOD_DISPLAY_COMPLETE_UPDATE_BIT = %u", uxBits);
        complete = (bool) (uxBits & DISPOD_DISPLAY_COMPLETE_UPDATE_BIT);

    }
}