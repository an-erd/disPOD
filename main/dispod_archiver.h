#ifndef __DISPOD_ARCHIVER_H__
#define __DISPOD_ARCHIVER_H__

#include "dispod_main.h"

// parameters for data buffers
typedef struct {
	struct tm timeinfo;	// empty: tm_year=0
    uint8_t cad;		// empty=0
    uint8_t str;		// empty=9, because 0-2 is used
    uint16_t GCT;		// empty=0
} buffer_element_t;

void dispod_archiver_initialize();
void dispod_archiver_add_RSCValues(uint8_t new_cad);
void dispod_archiver_add_customValues(uint16_t new_GCT, uint8_t new_str);
void dispod_archiver_add_time(tm timeinfo);
void dispod_archiver_set_new_file();    // set flag to open a new file and afterwards only append

void dispod_archiver_task(void *pvParameters);

#endif // __DISPOD_ARCHIVER_H__
