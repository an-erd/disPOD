#include <string.h>
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"
#include "dispod_archiver.h"
#include "dispod_main.h"

static const char* TAG = "DISPOD_ARCHIVER";

// data buffers
static buffer_element_t buffers[CONFIG_SDCARD_NUM_BUFFERS][CONFIG_SDCARD_BUFFER_SIZE];
static uint8_t  current_buffer;                             // buffer to write next elements to
static uint32_t used_in_buffer[CONFIG_SDCARD_NUM_BUFFERS];  // position in resp. buffer
static uint8_t  next_buffer_to_write;                       // next buffer to write to

void dispod_archiver_set_to_next_buffer();

static void clean_buffer(uint8_t num_buffer)
{
    memset(buffers[num_buffer], 0, sizeof(buffers[num_buffer])+1);
    used_in_buffer[num_buffer] = 0;
}

void dispod_archiver_initialize()
{
    for (int i = 0; i < CONFIG_SDCARD_NUM_BUFFERS; i++){
        clean_buffer(i);
    }
    current_buffer = 0;
    next_buffer_to_write = 0;
}

int write_out_any_buffers()
{
    ESP_LOGI(TAG, "Initializing SD card");
    ESP_LOGI(TAG, "Using SDMMC peripheral");
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    sdspi_slot_config_t slot_config = SDSPI_SLOT_CONFIG_DEFAULT();
    slot_config.gpio_miso = CONFIG_SDCARD_PIN_MISO;
    slot_config.gpio_mosi = CONFIG_SDCARD_PIN_MOSI;
    slot_config.gpio_sck  = CONFIG_SDCARD_PIN_CLK;
    slot_config.gpio_cs   = CONFIG_SDCARD_PIN_CS;
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
    ESP_LOGI(TAG, "Opening refernce numer file");
    struct stat st;
    if (stat("/sdcard/refnr.txt", &st) == 0) {
        f = fopen("/sdcard/refnr.txt", "r");
        if (f == NULL) {
            ESP_LOGE(TAG, "Failed to open file for reading");
            return 0;
        }
    } else {
        f = fopen("/sdcard/refnr.txt", "w");
        if (f == NULL) {
            ESP_LOGE(TAG, "Failed to open file for writing");
            return 0;
        }
        fprintf(f, "0\n");
        fclose(f);
        ESP_LOGI(TAG, "File written");

        f = fopen("/sdcard/refnr.txt", "r");
        if (f == NULL) {
            ESP_LOGE(TAG, "Failed to open file for reading");
            return 0;
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
    sscanf(line, "%u", (unsigned int*)&file_nr);

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

    // Attempt to write out any full or completed buffers
    while( next_buffer_to_write != current_buffer ) {
        if(fwrite(buffers[next_buffer_to_write], sizeof(buffer_element_t), used_in_buffer[next_buffer_to_write], f) != used_in_buffer[next_buffer_to_write]) {
        ESP_LOGI(TAG, "Write fwrite(buffers[%u], ...) error", next_buffer_to_write);
        break;
        }
        clean_buffer(next_buffer_to_write);
        // dispod_archiver_set_to_next_buffer();
        next_buffer_to_write = (next_buffer_to_write + 1) % CONFIG_SDCARD_NUM_BUFFERS;
    }

    ESP_LOGI(TAG, "Closing file");
    fclose(f);

    // All done, unmount partition and disable SDMMC or SPI peripheral
    esp_vfs_fat_sdmmc_unmount();

    return 1;
}

void dispod_archiver_set_next_element()
{
    used_in_buffer[current_buffer]++;
    if(used_in_buffer[current_buffer] == CONFIG_SDCARD_BUFFER_SIZE){
        current_buffer = (current_buffer + 1) % CONFIG_SDCARD_NUM_BUFFERS;
        used_in_buffer[current_buffer]= 0;
        xEventGroupSetBits(dispod_sd_evg, DISPOD_SD_WRITE_COMPLETED_BUFFER_EVT);
    }
}

void dispod_archiver_set_to_next_buffer()
{
    if( used_in_buffer[current_buffer] != 0) {
        // set to next (free) buffer and write (all and maybe incomplete) buffer but current
        current_buffer = (current_buffer + 1) % CONFIG_SDCARD_NUM_BUFFERS;
        xEventGroupSetBits(dispod_sd_evg, DISPOD_SD_WRITE_COMPLETED_BUFFER_EVT);
    } else {
        // do nothing because current buffer is empty
    }
}

void dispod_archiver_add_RSCValues(uint8_t new_cad)
{
    buffers[current_buffer][used_in_buffer[current_buffer]].cad = new_cad;
    dispod_archiver_set_next_element();
}

void dispod_archiver_add_customValues(uint16_t new_GCT, uint8_t new_str)
{
    buffers[current_buffer][used_in_buffer[current_buffer]].GCT = new_GCT;
    buffers[current_buffer][used_in_buffer[current_buffer]].str = new_str;
    dispod_archiver_set_next_element();
}

void dispod_archiver_task(void *pvParameters)
{
    ESP_LOGI(TAG, "dispod_archiver_task: started");
    EventBits_t uxBits;

    for (;;)
    {
        uxBits = xEventGroupWaitBits(dispod_sd_evg, DISPOD_SD_WRITE_COMPLETED_BUFFER_EVT, pdTRUE, pdFALSE, portMAX_DELAY);
        if(uxBits & DISPOD_SD_WRITE_COMPLETED_BUFFER_EVT){
            write_out_any_buffers();
        }
    }
}
