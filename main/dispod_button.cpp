#include "esp_log.h"

#include "dispod_main.h"

static const char* TAG  = "DISPOD_BUTTON";


void dispod_m5_buttons_test() {
    button_unit_t button_unit = {0};

    if(M5.BtnA.wasPressed()) {
        button_unit.btn_id = BTN_A;
        ESP_LOGD(TAG, "dispod_m5_buttons_test: pressed %u", button_unit.btn_id);
    }
    if(M5.BtnB.wasPressed()) {
        button_unit.btn_id = BTN_B;
        ESP_LOGD(TAG, "dispod_m5_buttons_test: pressed %u", button_unit.btn_id);
    }
    if(M5.BtnC.wasPressed()) {
        button_unit.btn_id = BTN_C;
        ESP_LOGD(TAG, "dispod_m5_buttons_test: pressed %u", button_unit.btn_id);
    }

    if(M5.BtnA.wasReleased()) {
        button_unit.btn_id = BTN_A;
        ESP_LOGD(TAG, "dispod_m5_buttons_test: released %u", button_unit.btn_id);
        ESP_ERROR_CHECK(esp_event_post_to(dispod_loop_handle, ACTIVITY_EVENTS, DISPOD_BUTTON_TAP_EVT, &button_unit, sizeof(button_unit_t), portMAX_DELAY));
    }
    if(M5.BtnB.wasReleased()) {
        button_unit.btn_id = BTN_B;
        ESP_LOGD(TAG, "dispod_m5_buttons_test: released %u", button_unit.btn_id);
        ESP_ERROR_CHECK(esp_event_post_to(dispod_loop_handle, ACTIVITY_EVENTS, DISPOD_BUTTON_TAP_EVT, &button_unit, sizeof(button_unit_t), portMAX_DELAY));
    }
    if(M5.BtnC.wasReleased()) {
        button_unit.btn_id = BTN_C;
        ESP_LOGD(TAG, "dispod_m5_buttons_test: released %u", button_unit.btn_id);
        ESP_ERROR_CHECK(esp_event_post_to(dispod_loop_handle, ACTIVITY_EVENTS, DISPOD_BUTTON_TAP_EVT, &button_unit, sizeof(button_unit_t), portMAX_DELAY));
    }

    if(M5.BtnA.wasReleasefor(2000)) {
        button_unit.btn_id = BTN_A;
        ESP_LOGD(TAG, "dispod_m5_buttons_test: was pressed for (2000) %u", button_unit.btn_id);
        ESP_ERROR_CHECK(esp_event_post_to(dispod_loop_handle, ACTIVITY_EVENTS, DISPOD_BUTTON_2SEC_RELEASE_EVT, &button_unit, sizeof(button_unit_t), portMAX_DELAY));
    }
    if(M5.BtnB.wasReleasefor(2000)) {
        button_unit.btn_id = BTN_B;
        ESP_LOGD(TAG, "dispod_m5_buttons_test: was pressed for (2000) %u", button_unit.btn_id);
        ESP_ERROR_CHECK(esp_event_post_to(dispod_loop_handle, ACTIVITY_EVENTS, DISPOD_BUTTON_2SEC_RELEASE_EVT, &button_unit, sizeof(button_unit_t), portMAX_DELAY));
    }
    if(M5.BtnC.wasReleasefor(2000)) {
        button_unit.btn_id = BTN_C;
        ESP_LOGD(TAG, "dispod_m5_buttons_test: was pressed for (2000) %u", button_unit.btn_id);
        ESP_ERROR_CHECK(esp_event_post_to(dispod_loop_handle, ACTIVITY_EVENTS, DISPOD_BUTTON_2SEC_RELEASE_EVT, &button_unit, sizeof(button_unit_t), portMAX_DELAY));
    }
}
