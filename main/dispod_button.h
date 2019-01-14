#ifndef __DISPOD_BUTTON_H__
#define __DISPOD_BUTTON_H__

// typedef enum {
//     BTN_A = 0,
//     BTN_B,
//     BTN_C,
//     NUM_BUTTONS
// } button_id_t;

typedef int button_id_t;
typedef struct {
    button_id_t     btn_id;
} button_unit_t;

void dispod_m5_buttons_test();

#endif // __DISPOD_BUTTON_H__
