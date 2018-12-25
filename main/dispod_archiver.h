#ifndef __DISPOD_ARCHIVER_H__
#define __DISPOD_ARCHIVER_H__

#include "dispod_main.h"

// parameters for data buffers
typedef struct {
    uint8_t cad;
    uint8_t str;
    uint16_t GCT;
} buffer_element_t;

extern buffer_element_t buffers[CONFIG_SDCARD_NUM_BUFFERS][CONFIG_SDCARD_BUFFER_SIZE];

void dispod_archiver_task(void *pvParameters);

#endif // __DISPOD_ARCHIVER_H__
