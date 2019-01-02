#ifndef __DISPOD_ARCHIVER_H__
#define __DISPOD_ARCHIVER_H__

#include "dispod_main.h"

// parameters for data buffers
typedef struct {
    uint8_t cad;
    uint8_t str;
    uint16_t GCT;
} buffer_element_t;

void dispod_archiver_initialize();
void dispod_archiver_add_RSCValues(uint8_t new_cad);
void dispod_archiver_add_customValues(uint16_t new_GCT, uint8_t new_str);

void dispod_archiver_task(void *pvParameters);

#endif // __DISPOD_ARCHIVER_H__
