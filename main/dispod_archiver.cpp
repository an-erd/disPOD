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

// data buffers (should be configured to CONFIG_SDCARD_NUM_BUFFERS=4 and CONFIG_SDCARD_BUFFER_SIZE=131072)
static buffer_element_t buffers[CONFIG_SDCARD_NUM_BUFFERS][CONFIG_SDCARD_BUFFER_SIZE] EXT_RAM_ATTR;
static uint8_t  current_buffer;                             // buffer to write next elements to
static uint32_t used_in_buffer[CONFIG_SDCARD_NUM_BUFFERS];  // position in resp. buffer
static uint8_t  next_buffer_to_write;                       // next buffer to write to
static int s_ref_file_nr = -2;

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

//TF card test begin
void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
    ESP_LOGI(TAG, "listDir() > , Listing directory: %s\n", dirname);

    File root = fs.open(dirname);
    if(!root){
        ESP_LOGI(TAG, "listDir(), Failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        ESP_LOGI(TAG, "listDir(), Not a directory");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            ESP_LOGI(TAG, "listDir(), DIR: %s", file.name());
            if(levels){
                listDir(fs, file.name(), levels -1);
            }
        } else {
            ESP_LOGI(TAG, "listDir(), FILE: %s, SIZE: %u", file.name(), file.size());
        }
        file = root.openNextFile();
    }
    ESP_LOGI(TAG, "listDir() <");
}

void readFile(fs::FS &fs, const char * path) {
    ESP_LOGI(TAG, "readFile() >, Reading file: %s", path);

    File file = fs.open(path);
    if(!file){
        ESP_LOGI(TAG, "readFile(), Failed to open file for reading");
        return;
    }

    ESP_LOGI(TAG, "readFile(), Read from file: ");
    while(file.available()){
        int ch = file.read();
        ESP_LOGI(TAG, "readFile(): %c", ch);
    }
    ESP_LOGI(TAG, "readFile() <");
}

void writeFile(fs::FS &fs, const char * path, const char * message){
    ESP_LOGI(TAG, "writeFile() >, Writing file: %s", path);

    File file = fs.open(path, FILE_WRITE);
    if(!file){
        ESP_LOGI(TAG, "writeFile(), Failed to open file for writing");
        return;
    }
    if(file.print(message)){
        ESP_LOGI(TAG, "writeFile() <, file written");
    } else {
        ESP_LOGI(TAG, "writeFile() <, write failed");
    }
}
//TF card test end


static int read_sd_card_file_refnr()
{
    // file handle and buffer to read
    FILE*   f;
    char    line[64];
    int     file_nr = -1;

    ESP_LOGI(TAG, "read_sd_card_file_refnr() >");
    struct stat st;
    if (stat("/sd/refnr.txt", &st) == 0) {
        f = fopen("/sd/refnr.txt", "r");
        if (f == NULL) {
            ESP_LOGE(TAG, "Failed to open file for reading");
            return -1;
        }
    // else {
    //     f = fopen("/refnr.txt", "w");
    //     if (f == NULL) {
    //         ESP_LOGE(TAG, "Failed to open file for writing");
    //         return -1;
    //     }
    //     fprintf(f, "0\n");
    //     fclose(f);
    //     ESP_LOGI(TAG, "File written");

    //     f = fopen("/refnr.txt", "r");
    //     if (f == NULL) {
    //         ESP_LOGE(TAG, "Failed to open file for reading");
    //         return -1;
    //     }
    // }

    fgets(line, sizeof(line), f);
    fclose(f);
    }

    // strip newline
    char* pos = strchr(line, '\n');
    if (pos) {
        *pos = '\0';
    }
    ESP_LOGI(TAG, "read_sd_card_file_refnr: Read from file: '%s'", line);
    sscanf(line, "%d", (int*)&file_nr);

    s_ref_file_nr = file_nr;

    ESP_LOGI(TAG, "read_sd_card_file_refnr() <, file_nr = %d", file_nr);

    return file_nr;
}

static int write_sd_card_file_refnr(int new_refnr)
{
    // file handle and buffer to read
    FILE*   f;
    char    line[64];
    struct stat st;

    ESP_LOGI(TAG, "write_sd_card_file_refnr() >, new_refnr = %d", new_refnr);
    if (stat("/sd/refnr.txt", &st) == 0) {
        ESP_LOGI(TAG, "write_sd_card_file_refnr(), stat = 0");
        f = fopen("/sd/refnr.txt", "w");
        if (f == NULL) {
            ESP_LOGE(TAG, "write_sd_card_file_refnr() <, Failed to open file for writing");
            return -1;
        }
        fprintf(f, "%d\n", new_refnr);
        fclose(f);
        ESP_LOGI(TAG, "write_sd_card_file_refnr() <, File written");
    }

    return new_refnr;
}

int write_out_any_buffers()
{
    ESP_LOGI(TAG, "write_out_any_buffers(): >");

    // file handle and buffer to read
    FILE*   f;
    char    line[64];
    int     file_nr;

    // Read the last file number from file. If it does not exist, create a new and start with "0".
    // If it exists, read the value and increase by 1.
    file_nr = read_sd_card_file_refnr();
    if(file_nr < 0)
        ESP_LOGE(TAG, "write_out_any_buffers(): Ref. file could not be read or created - ABORT");
    file_nr++;
    write_sd_card_file_refnr(file_nr);

    // generate new file name
    sprintf(line, CONFIG_SDCARD_FILE_NAME, file_nr);
    // sprintf(line, "/sd/out_1.txt");
    ESP_LOGI(TAG, "write_out_any_buffers(): generated file name '%s'", line);

    f = fopen(line, "a");
    if(f == NULL) {
        f = fopen(line, "w");
        if (f == NULL) {
            ESP_LOGE(TAG, "write_out_any_buffers(): Failed to open file for writing");
            return 0;
        }
        ESP_LOGI(TAG, "write_out_any_buffers(): Created new file for write");
    } else {
        ESP_LOGI(TAG, "write_out_any_buffers(): Opening file for append");
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

    ESP_LOGI(TAG, "write_out_any_buffers: <");

    return 1;
}

void dispod_archiver_set_next_element()
{
    int complete = xEventGroupWaitBits(dispod_sd_evg, DISPOD_SD_WRITE_COMPLETED_BUFFER_EVT, pdFALSE, pdFALSE, 0) & DISPOD_SD_WRITE_COMPLETED_BUFFER_EVT;
    // ESP_LOGD(TAG, "dispod_archiver_set_next_element >: current_buffer %u, used in current buffer %u, size %u, complete %u",
    //     current_buffer, used_in_buffer[current_buffer], CONFIG_SDCARD_BUFFER_SIZE, complete);

    used_in_buffer[current_buffer]++;
    if(used_in_buffer[current_buffer] == CONFIG_SDCARD_BUFFER_SIZE){
        current_buffer = (current_buffer + 1) % CONFIG_SDCARD_NUM_BUFFERS;
        used_in_buffer[current_buffer] = 0;
        xEventGroupSetBits(dispod_sd_evg, DISPOD_SD_WRITE_COMPLETED_BUFFER_EVT);
    }

    complete = xEventGroupWaitBits(dispod_sd_evg, DISPOD_SD_WRITE_COMPLETED_BUFFER_EVT, pdFALSE, pdFALSE, 0) & DISPOD_SD_WRITE_COMPLETED_BUFFER_EVT;
    // ESP_LOGD(TAG, "dispod_archiver_set_next_element <: current_buffer %u, used in current buffer %u, size %u, complete %u",
    //     current_buffer, used_in_buffer[current_buffer], CONFIG_SDCARD_BUFFER_SIZE, complete);
}

void dispod_archiver_set_to_next_buffer()
{
    // ESP_LOGD(TAG, "dispod_archiver_set_to_next_buffer >: current_buffer %u, used in current buffer %u ", current_buffer, used_in_buffer[current_buffer]);
    if( used_in_buffer[current_buffer] != 0) {
        // set to next (free) buffer and write (all and maybe incomplete) buffer but current
        current_buffer = (current_buffer + 1) % CONFIG_SDCARD_NUM_BUFFERS;
        xEventGroupSetBits(dispod_sd_evg, DISPOD_SD_WRITE_COMPLETED_BUFFER_EVT);
    } else {
        // do nothing because current buffer is empty
    }
    // ESP_LOGD(TAG, "dispod_archiver_set_to_next_buffer <: current_buffer %u, used in current buffer %u ", current_buffer, used_in_buffer[current_buffer]);
}

void dispod_archiver_add_RSCValues(uint8_t new_cad)
{
    buffers[current_buffer][used_in_buffer[current_buffer]].cad = new_cad;
    // ESP_LOGD(TAG, "dispod_archiver_add_RSCValues: current_buffer %u, used in current buffer %u ", current_buffer, used_in_buffer[current_buffer]);
    dispod_archiver_set_next_element();
}

void dispod_archiver_add_customValues(uint16_t new_GCT, uint8_t new_str)
{
    buffers[current_buffer][used_in_buffer[current_buffer]].GCT = new_GCT;
    buffers[current_buffer][used_in_buffer[current_buffer]].str = new_str;
    // ESP_LOGD(TAG, "dispod_archiver_add_customValues: current_buffer %u, used in current buffer %u ", current_buffer, used_in_buffer[current_buffer]);
    dispod_archiver_set_next_element();
}

void dispod_archiver_task(void *pvParameters)
{
    ESP_LOGI(TAG, "dispod_archiver_task: started");
    EventBits_t uxBits;

    for (;;)
    {
        uxBits = xEventGroupWaitBits(dispod_sd_evg,
                DISPOD_SD_WRITE_COMPLETED_BUFFER_EVT | DISPOD_SD_PROBE_EVT | DISPOD_SD_GENERATE_TESTDATA_EVT,
                pdTRUE, pdFALSE, portMAX_DELAY);

        if(uxBits & DISPOD_SD_PROBE_EVT){
            xEventGroupClearBits(dispod_sd_evg, DISPOD_SD_PROBE_EVT);

            ESP_LOGI(TAG, "DISPOD_SD_PROBE_EVT: DISPOD_SD_PROBE_EVT");
            ESP_LOGI(TAG, "SD card info %d, card size %llu, total bytes %llu used bytes %llu, used %3.1f perc.",
                SD.cardType(), SD.cardSize(), SD.totalBytes(), SD.usedBytes(), (SD.usedBytes() / (float) SD.totalBytes()));
            // TF card test
            listDir(SD, "/", 0);
            // writeFile(SD, "/hello.txt", "Hello world");
            // readFile(SD, "/hello.txt");
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
        }

        if((uxBits & DISPOD_SD_WRITE_COMPLETED_BUFFER_EVT) == DISPOD_SD_WRITE_COMPLETED_BUFFER_EVT){
            ESP_LOGI(TAG, "dispod_archiver_task: DISPOD_SD_WRITE_COMPLETED_BUFFER_EVT");
            if( xEventGroupWaitBits(dispod_event_group, DISPOD_SD_AVAILABLE_BIT, pdFALSE, pdFALSE, portMAX_DELAY) & DISPOD_SD_AVAILABLE_BIT){
                write_out_any_buffers();
            } else {
                ESP_LOGE(TAG, "dispod_archiver_task: write out completed buffers but no SD mounted");
            }
        }

        if((uxBits & DISPOD_SD_GENERATE_TESTDATA_EVT) == DISPOD_SD_GENERATE_TESTDATA_EVT){
            ESP_LOGI(TAG, "dispod_archiver_task: DISPOD_SD_GENERATE_TESTDATA_EVT start");
            for(int i=0; i < CONFIG_SDCARD_BUFFER_SIZE; i++){
                for(int j=0; j < CONFIG_SDCARD_BUFFER_SIZE; j++){
                    if(j%2){
                        dispod_archiver_add_RSCValues(180);
                    } else {
                        dispod_archiver_add_customValues(352,1);
                    }
                    if((i==(CONFIG_SDCARD_BUFFER_SIZE-1)) && j == 130000)
                        break;
                }
            }
            ESP_LOGI(TAG, "dispod_archiver_task: DISPOD_SD_GENERATE_TESTDATA_EVT done");
        }
    }
}
