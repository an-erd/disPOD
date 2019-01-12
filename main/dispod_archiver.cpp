#include <string.h>
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
// #include "tftspi.h"
// #include "tft.h"
#include "sdmmc_cmd.h"
#include "dispod_archiver.h"
#include "dispod_main.h"

static const char* TAG = "DISPOD_ARCHIVER";

// data buffers
static buffer_element_t buffers[CONFIG_SDCARD_NUM_BUFFERS][CONFIG_SDCARD_BUFFER_SIZE];
static uint8_t  current_buffer;                             // buffer to write next elements to
static uint32_t used_in_buffer[CONFIG_SDCARD_NUM_BUFFERS];  // position in resp. buffer
static uint8_t  next_buffer_to_write;                       // next buffer to write to

EventGroupHandle_t dispod_sd_evg;

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


static int read_sd_card_file_refnr()
{
    // file handle and buffer to read
    FILE*   f;
    char    line[64];
    int     file_nr = -1;

    ESP_LOGI(TAG, "read_sd_card_file_refnr()");
    struct stat st;
    if (stat("/sdcard/refnr.txt", &st) == 0) {
        f = fopen("/sdcard/refnr.txt", "r");
        if (f == NULL) {
            ESP_LOGE(TAG, "Failed to open file for reading");
            return -1;
        }
    } else {
        f = fopen("/sdcard/refnr.txt", "w");
        if (f == NULL) {
            ESP_LOGE(TAG, "Failed to open file for writing");
            return -1;
        }
        fprintf(f, "0\n");
        fclose(f);
        ESP_LOGI(TAG, "File written");

        f = fopen("/sdcard/refnr.txt", "r");
        if (f == NULL) {
            ESP_LOGE(TAG, "Failed to open file for reading");
            return -1;
        }
    }

    fgets(line, sizeof(line), f);
    fclose(f);

    // strip newline
    char* pos = strchr(line, '\n');
    if (pos) {
        *pos = '\0';
    }
    ESP_LOGI(TAG, "read_sd_card_file_refnr: Read from file: '%s'", line);
    sscanf(line, "%d", (int*)&file_nr);

    return file_nr;
}

int write_out_any_buffers()
{

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


    return 1;
}

void dispod_archiver_set_next_element()
{
    int complete = xEventGroupWaitBits(dispod_sd_evg, DISPOD_SD_WRITE_COMPLETED_BUFFER_EVT, pdFALSE, pdFALSE, 0) & DISPOD_SD_WRITE_COMPLETED_BUFFER_EVT;
    ESP_LOGD(TAG, "dispod_archiver_set_next_element >: current_buffer %u, used in current buffer %u, size %u, complete %u",
        current_buffer, used_in_buffer[current_buffer], CONFIG_SDCARD_BUFFER_SIZE, complete);

    used_in_buffer[current_buffer]++;
    if(used_in_buffer[current_buffer] == CONFIG_SDCARD_BUFFER_SIZE){
        current_buffer = (current_buffer + 1) % CONFIG_SDCARD_NUM_BUFFERS;
        used_in_buffer[current_buffer]= 0;
        xEventGroupSetBits(dispod_sd_evg, DISPOD_SD_WRITE_COMPLETED_BUFFER_EVT);
    }

    complete = xEventGroupWaitBits(dispod_sd_evg, DISPOD_SD_WRITE_COMPLETED_BUFFER_EVT, pdFALSE, pdFALSE, 0) & DISPOD_SD_WRITE_COMPLETED_BUFFER_EVT;
    ESP_LOGD(TAG, "dispod_archiver_set_next_element <: current_buffer %u, used in current buffer %u, size %u, complete %u",
        current_buffer, used_in_buffer[current_buffer], CONFIG_SDCARD_BUFFER_SIZE, complete);
}

void dispod_archiver_set_to_next_buffer()
{
    ESP_LOGD(TAG, "dispod_archiver_set_to_next_buffer >: current_buffer %u, used in current buffer %u ", current_buffer, used_in_buffer[current_buffer]);
    if( used_in_buffer[current_buffer] != 0) {
        // set to next (free) buffer and write (all and maybe incomplete) buffer but current
        current_buffer = (current_buffer + 1) % CONFIG_SDCARD_NUM_BUFFERS;
        xEventGroupSetBits(dispod_sd_evg, DISPOD_SD_WRITE_COMPLETED_BUFFER_EVT);
    } else {
        // do nothing because current buffer is empty
    }
    ESP_LOGD(TAG, "dispod_archiver_set_to_next_buffer <: current_buffer %u, used in current buffer %u ", current_buffer, used_in_buffer[current_buffer]);
}

void dispod_archiver_add_RSCValues(uint8_t new_cad)
{
    buffers[current_buffer][used_in_buffer[current_buffer]].cad = new_cad;
    ESP_LOGD(TAG, "dispod_archiver_add_RSCValues: current_buffer %u, used in current buffer %u ", current_buffer, used_in_buffer[current_buffer]);
    dispod_archiver_set_next_element();
}

void dispod_archiver_add_customValues(uint16_t new_GCT, uint8_t new_str)
{
    buffers[current_buffer][used_in_buffer[current_buffer]].GCT = new_GCT;
    buffers[current_buffer][used_in_buffer[current_buffer]].str = new_str;
    ESP_LOGD(TAG, "dispod_archiver_add_customValues: current_buffer %u, used in current buffer %u ", current_buffer, used_in_buffer[current_buffer]);
    dispod_archiver_set_next_element();
}

void dispod_archiver_task(void *pvParameters)
{
    ESP_LOGI(TAG, "dispod_archiver_task: started");
    EventBits_t uxBits;

    for (;;)
    {
        uxBits = xEventGroupWaitBits(dispod_sd_evg,
                DISPOD_SD_WRITE_COMPLETED_BUFFER_EVT | DISPOD_SD_MOUNT_EVT | DISPOD_SD_UNMOUNT_EVT | DISPOD_SD_PROBE_EVT,
                pdTRUE, pdFALSE, portMAX_DELAY);

        if(uxBits & DISPOD_SD_PROBE_EVT){
            xEventGroupClearBits(dispod_sd_evg, DISPOD_SD_PROBE_EVT);

            ESP_LOGI(TAG, "DISPOD_SD_PROBE_EVT: DISPOD_SD_PROBE_EVT");
            ESP_LOGI(TAG, "SD card info %d, card size %llu, total bytes %llu used bytes %llu, used %3.1f perc.",
                SD.cardType(), SD.cardSize(), SD.totalBytes(), SD.usedBytes(), (SD.usedBytes() / (float) SD.totalBytes()));
            if(SD.cardType() != CARD_NONE){
                xEventGroupSetBits(dispod_event_group, DISPOD_SD_AVAILABLE_BIT);
                dispod_screen_status_update_sd(&dispod_screen_status, SD_AVAILABLE);
                xEventGroupSetBits(dispod_display_evg, DISPOD_DISPLAY_UPDATE_BIT);
                ESP_ERROR_CHECK(esp_event_post_to(dispod_loop_handle, WORKFLOW_EVENTS, DISPOD_SD_INIT_DONE_EVT, NULL, 0, portMAX_DELAY));
            } else {
                xEventGroupClearBits(dispod_event_group, DISPOD_SD_AVAILABLE_BIT);
                dispod_screen_status_update_sd(&dispod_screen_status, SD_NOT_AVAILABLE);
                xEventGroupSetBits(dispod_display_evg, DISPOD_DISPLAY_UPDATE_BIT);
                ESP_ERROR_CHECK(esp_event_post_to(dispod_loop_handle, WORKFLOW_EVENTS, DISPOD_SD_INIT_DONE_EVT, NULL, 0, portMAX_DELAY));
            }
            // mount_sd_card();
            // ESP_LOGI(TAG, "DISPOD_SD_PROBE_EVT: read_sd_card_file_refnr()");
            // read_sd_card_file_refnr();
            // ESP_LOGI(TAG, "DISPOD_SD_PROBE_EVT: unmount_sd_card()");
            // unmount_sd_card();
            // ESP_LOGI(TAG, "DISPOD_SD_PROBE_EVT: disp_select()");
            // TODO check real status of mount/read/unmount probe!

        }

        // if((uxBits & DISPOD_SD_MOUNT_EVT) == DISPOD_SD_MOUNT_EVT){
        //     ESP_LOGI(TAG, "dispod_archiver_task: mount sd card");
        //     if( mount_sd_card() ){
        //         dispod_screen_status_update_sd(&dispod_screen_status, SD_AVAILABLE);
        //         xEventGroupSetBits(dispod_event_group, DISPOD_SD_AVAILABLE_BIT);
        //         xEventGroupSetBits(dispod_display_evg, DISPOD_DISPLAY_UPDATE_BIT);
        //     } else {
        //         dispod_screen_status_update_sd(&dispod_screen_status, SD_NOT_AVAILABLE);
        //         xEventGroupClearBits(dispod_event_group, DISPOD_SD_AVAILABLE_BIT);
        //         xEventGroupSetBits(dispod_display_evg, DISPOD_DISPLAY_UPDATE_BIT);
        //     }
        //     ESP_ERROR_CHECK(esp_event_post_to(dispod_loop_handle, WORKFLOW_EVENTS, DISPOD_SD_INIT_DONE_EVT, NULL, 0, portMAX_DELAY));
        // }

        // if((uxBits & DISPOD_SD_UNMOUNT_EVT) == DISPOD_SD_UNMOUNT_EVT){
        //     ESP_LOGI(TAG, "dispod_archiver_task: unmount sd card");
        //     unmount_sd_card();

        //     dispod_screen_status_update_sd(&dispod_screen_status, SD_NOT_AVAILABLE);
        //     xEventGroupClearBits(dispod_event_group, DISPOD_SD_AVAILABLE_BIT);
        //     xEventGroupSetBits(dispod_display_evg, DISPOD_DISPLAY_UPDATE_BIT);
        // }

        // if((uxBits & DISPOD_SD_WRITE_COMPLETED_BUFFER_EVT) == DISPOD_SD_WRITE_COMPLETED_BUFFER_EVT){
        //     ESP_LOGI(TAG, "dispod_archiver_task: write out completed buffers");
        //     if( xEventGroupWaitBits(dispod_event_group, DISPOD_SD_AVAILABLE_BIT, pdFALSE, pdFALSE, portMAX_DELAY) & DISPOD_SD_AVAILABLE_BIT){
        //         write_out_any_buffers();
        //     } else {
        //         ESP_LOGE(TAG, "dispod_archiver_task: write out completed buffers but no SD mounted");
        //     }
        // }
    }
}