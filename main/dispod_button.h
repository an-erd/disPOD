#ifndef __DISPOD_BUTTON_H__
#define __DISPOD_BUTTON_H__

#include "iot_button.h"

#define BUTTON_A_PIN 39
#define BUTTON_B_PIN 38
#define BUTTON_C_PIN 37
#define BUTTON_ACTIVE_LEVEL 0

typedef enum {
    BUTTON_A = 0,
    BUTTON_B,
    BUTTON_C,
    NUM_BUTTONS
} button_id_t;

typedef struct {
    button_handle_t btn_handle;
    button_id_t     btn_id;
} button_unit_t;

void dispod_button_initialize();

#endif // __DISPOD_BUTTON_H__
