#include "esp_log.h"
#include <stdlib.h>
#include "dispod_runvalues.h"
#include "dispod_main.h"

static const char* TAG = "DISPOD_RUNVALUES";
bool update_display_RSC    = false;
bool update_display_Custom = false;

void dispod_runvalues_initialize(runningValuesStruct_t *values)
{
	values->val_cad.num_items = 0;
	values->val_cad.idx_item = 0;
	values->val_cad.sum_items = 0;

	values->val_GCT.num_items = 0;
	values->val_GCT.idx_item = 0;
	values->val_GCT.sum_items = 0;

	values->val_str.num_items = 0;
	values->val_str.idx_item = 0;
	values->val_str.sum_items = 0;

	for (int i = 0; i < DISPOD_RUNVALUES_BUFFERLEN; i++) {
		values->val_cad.values[i] = 0;
		values->val_GCT.values[i] = 0;
		values->val_str.values[i] = 0;
	}

	values->values_to_display.cad = 0;
	values->values_to_display.GCT = 0;
	values->values_to_display.str = 0;

	values->update_display_available = false;
};

bool dispod_runvalues_get_update_display_available(runningValuesStruct_t *values)
{
    ESP_LOGI(TAG, "dispod_runvalues_get_update_display_available(), update_display_available %u", values->update_display_available);
    if(values->update_display_available) {
        update_display_RSC = false;
        update_display_Custom = false;
        values->update_display_available = false;

    	ESP_LOGI(TAG, "dispod_runvalues_get_update_display_available(), cad [%3u %3u %3u %3u %3u], num %3u, sum %4u, val2dis %3u",
	    	values->val_cad.values[0], values->val_cad.values[1], values->val_cad.values[2], values->val_cad.values[3], values->val_cad.values[4],
		    values->val_cad.num_items, values->val_cad.sum_items, values->values_to_display.cad);
    	ESP_LOGI(TAG, "dispod_runvalues_get_update_display_available(), GCT [%3u %3u %3u %3u %3u], num %3u, sum %4u, val2dis %3u",
	    	values->val_GCT.values[0], values->val_GCT.values[1], values->val_GCT.values[2], values->val_GCT.values[3], values->val_GCT.values[4],
		    values->val_GCT.num_items, values->val_GCT.sum_items, values->values_to_display.GCT);
    	ESP_LOGI(TAG, "dispod_runvalues_get_update_display_available(), str [%3u %3u %3u %3u %3u], num %3u, sum %4u, val2dis %3u",
	    	values->val_str.values[0], values->val_str.values[1], values->val_str.values[2], values->val_str.values[3], values->val_str.values[4],
		    values->val_str.num_items, values->val_str.sum_items, values->values_to_display.str);
        return true;
    } else {
    	return false;
    }
}

void dispod_runvalues_update_RSCValues(runningValuesStruct_t *values, uint8_t new_cad)
{
	// update cadence,
    if(new_cad){    // TODO what to do if cad = 0?
	    values->tmp_values.cad = new_cad;
		values->val_cad.sum_items += values->tmp_values.cad - values->val_cad.values[values->val_cad.idx_item];
		values->val_cad.values[values->val_cad.idx_item] = values->tmp_values.cad;
		values->val_cad.idx_item = (values->val_cad.idx_item + 1) % DISPOD_RUNVALUES_BUFFERLEN;
		values->val_cad.num_items = min(DISPOD_RUNVALUES_BUFFERLEN, values->val_cad.num_items + 1);
	    if(values->val_cad.num_items){
		    values->values_to_display.cad = (values->val_cad.sum_items / values->val_cad.num_items) * 2;
			update_display_RSC = true;
        }
    }

    if(update_display_RSC && update_display_Custom)
        values->update_display_available = true;

    ESP_LOGI(TAG, "dispod_runvalues_update_RSCValues(), cad %u, val2dis %u, update_display_RSC %u, update_display_Custom %u",
        values->tmp_values.cad, values->values_to_display.cad,
        update_display_RSC, update_display_Custom);
}

void dispod_runvalues_update_customValues(runningValuesStruct_t *values, uint16_t new_GCT, uint8_t new_str)
{
	// update GCT and strike
	if (new_GCT) {  // TODO what to do if cad = 0?
	    values->tmp_values.GCT = new_GCT;
	    values->tmp_values.str = new_str;
		values->val_GCT.sum_items += values->tmp_values.GCT - values->val_GCT.values[values->val_GCT.idx_item];
		values->val_GCT.values[values->val_GCT.idx_item] = values->tmp_values.GCT;
		values->val_GCT.idx_item = (values->val_GCT.idx_item + 1) % DISPOD_RUNVALUES_BUFFERLEN;
		values->val_GCT.num_items = min(DISPOD_RUNVALUES_BUFFERLEN, values->val_GCT.num_items + 1);

		values->val_str.sum_items += values->tmp_values.str - values->val_str.values[values->val_str.idx_item];
		values->val_str.values[values->val_str.idx_item] = values->tmp_values.str;
		values->val_str.idx_item = (values->val_str.idx_item + 1) % DISPOD_RUNVALUES_BUFFERLEN;
		values->val_str.num_items = min(DISPOD_RUNVALUES_BUFFERLEN, values->val_str.num_items + 1);

        if(values->val_GCT.num_items){   // one check (e.g. values->val_GCT.num_items) is sufficient
        	values->values_to_display.GCT = values->val_GCT.sum_items / values->val_GCT.num_items;
        	values->values_to_display.str = (values->val_str.sum_items * 10) / values->val_str.num_items;		// div by 10 during display procedure
            update_display_Custom = true;
        }
    }
    ESP_LOGI(TAG, "dispod_runvalues_update_customValues(), GCT %u, val2dis %u, str %u, val2dis %u, update_display_RSC %u, update_display_Custom %u",
        values->tmp_values.GCT, values->values_to_display.GCT,
        values->tmp_values.str, values->values_to_display.str,
        update_display_RSC, update_display_Custom);
}
