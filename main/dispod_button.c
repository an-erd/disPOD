#include "esp_log.h"

#include "dispod_main.h"
#include "iot_button.h"

static const char* TAG  = "DISPOD_BUTTON";

button_unit_t btnA_unit;
button_unit_t btnB_unit;
button_unit_t btnC_unit;

// Button callback functions
void button_tap_cb(void* arg)
{
    button_unit_t* button_unit = (button_unit_t*) arg;

    ESP_LOGI(TAG, "push cb (id %d)", button_unit->btn_id);
    ESP_ERROR_CHECK(esp_event_post_to(dispod_loop_handle, ACTIVITY_EVENTS, DISPOD_BUTTON_TAP_EVT, button_unit, sizeof(button_unit_t), portMAX_DELAY));
}

void button_press_2s_cb(void* arg)
{
    button_unit_t* button_unit = (button_unit_t*) arg;

    ESP_LOGI(TAG, "press 2s (id %d)", button_unit->btn_id);
    ESP_ERROR_CHECK(esp_event_post_to(dispod_loop_handle, ACTIVITY_EVENTS, DISPOD_BUTTON_2SEC_PRESS_EVT, button_unit, sizeof(button_unit_t), portMAX_DELAY));
}

void button_press_5s_cb(void* arg)
{
    button_unit_t* button_unit = (button_unit_t*) arg;

    ESP_LOGI(TAG, "press 5s (id %d)", button_unit->btn_id);
    ESP_ERROR_CHECK(esp_event_post_to(dispod_loop_handle, ACTIVITY_EVENTS, DISPOD_BUTTON_5SEC_PRESS_EVT, button_unit, sizeof(button_unit_t), portMAX_DELAY));
}

void dispod_button_initialize()
{
    ESP_LOGI(TAG, "dispod_button_initialize()");
    ESP_LOGI(TAG, "before btn init, heap: %d", esp_get_free_heap_size());

    btnA_unit.btn_handle = iot_button_create(BUTTON_A_PIN, BUTTON_ACTIVE_LEVEL);
    btnA_unit.btn_id     = BUTTON_A;
    btnB_unit.btn_handle = iot_button_create(BUTTON_B_PIN, BUTTON_ACTIVE_LEVEL);
    btnB_unit.btn_id     = BUTTON_B;
    btnC_unit.btn_handle = iot_button_create(BUTTON_C_PIN, BUTTON_ACTIVE_LEVEL);
    btnC_unit.btn_id     = BUTTON_C;

    iot_button_set_evt_cb(btnA_unit.btn_handle, BUTTON_CB_TAP, button_tap_cb, &btnA_unit);
    iot_button_set_evt_cb(btnB_unit.btn_handle, BUTTON_CB_TAP, button_tap_cb, &btnB_unit);
    iot_button_set_evt_cb(btnC_unit.btn_handle, BUTTON_CB_TAP, button_tap_cb, &btnC_unit);

    iot_button_add_custom_cb(btnA_unit.btn_handle, 2, button_press_2s_cb, &btnA_unit);
    iot_button_add_custom_cb(btnA_unit.btn_handle, 5, button_press_5s_cb, &btnA_unit);
    iot_button_add_custom_cb(btnB_unit.btn_handle, 2, button_press_2s_cb, &btnB_unit);
    iot_button_add_custom_cb(btnB_unit.btn_handle, 5, button_press_5s_cb, &btnB_unit);
    iot_button_add_custom_cb(btnC_unit.btn_handle, 2, button_press_2s_cb, &btnC_unit);
    iot_button_add_custom_cb(btnC_unit.btn_handle, 5, button_press_5s_cb, &btnC_unit);

    // iot_button_set_evt_cb(btnB_handle, BUTTON_CB_PUSH, button_tap_cb, "PUSH");
    // iot_button_set_evt_cb(btnB_handle, BUTTON_CB_RELEASE, button_tap_cb, "RELEASE");
    // iot_button_set_evt_cb(btnB_handle, BUTTON_CB_TAP, button_tap_cb, "TAP");

    ESP_LOGI(TAG, "after btn init, heap: %d\n", esp_get_free_heap_size());
}