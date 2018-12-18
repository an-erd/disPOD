#ifndef __DISPOD_RUNVALUES_H__
#define __DISPOD_RUNVALUES_H__

#include "esp_event.h"

#define DISPOD_RUNVALUES_BUFFERLEN      5
#define RUNNING_VALUES_UPDATE_RCS		BIT0
#define RUNNING_VALUES_UPDATE_CUSTOM	BIT1

typedef struct {
	uint16_t	values[DISPOD_RUNVALUES_BUFFERLEN];
	uint8_t		num_items;
	uint8_t		idx_item;
	uint16_t	sum_items;
} runValuesElementStruct_t;

typedef struct {
	uint16_t	cad;		// one leg
	uint16_t	GCT;		// avg value
	uint16_t	str;		// div by 10 to display
} runValuesRSC_t;

typedef struct {
	runValuesElementStruct_t	val_cad;		// Cadence
	runValuesElementStruct_t	val_GCT;		// Ground Contact Time
	runValuesElementStruct_t	val_str;		// Strikt (heel=0, mid=1, toe=2)
	runValuesRSC_t		        values_to_display;

	bool update_display_available;

	// update in callback functions, copy to values[] if
	runValuesRSC_t tmp_values;
	bool update_avail_RSC;
	bool update_avail_custom;
} runningValuesStruct_t;

void dispod_runvalues_initialize(runningValuesStruct_t *values);
void dispod_runvalues_calculate_display_values(runningValuesStruct_t *values);
bool dispod_runvalues_get_update_display_available(runningValuesStruct_t *values);
void dispod_runvalues_update_display_values(runningValuesStruct_t *values,
    runValuesRSC_t *new_values);
void dispod_runvalues_update_RSCValues(runningValuesStruct_t *values,
    uint8_t new_cad);
void dispod_runvalues_update_customValues(runningValuesStruct_t *values,
    uint16_t new_GCT, uint8_t new_str);

#endif // __DISPOD_RUNVALUES_H__