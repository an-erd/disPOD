#include <stdbool.h>
#include <string.h>
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_common_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_common_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"

#include "dispod_config.h"
#include "dispod_gattc.h"
#include "dispod_runvalues.h"

static const char* GATTC_TAG = "DISPOD_GATTC";

// Beginning of GATTC handling
#define REMOTE_NOTIFY_CHAR_CUSTOM_UUID  0xff00
#define NOTIFY_HANDLE_RSC               19
#define NOTIFY_HANDLE_CUSTOM            29
#define REMOTE_NOTIFY_NUM_UUIDS         2
#define PROFILE_NUM                     1
#define PROFILE_A_APP_ID                0
#define INVALID_HANDLE                  0

static const char remote_device_name[] = DISPOD_BLE_SCAN_PREFIX_DEVICE; // "MilestonePod";
static bool connect    = false;
static bool get_server = false;
static esp_gattc_char_elem_t *char_elem_result   = NULL;
static esp_gattc_descr_elem_t *descr_elem_result = NULL;

/* declare static functions */
static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
static void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
static void gattc_profile_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);


static esp_bt_uuid_t remote_filter_service_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 = ESP_GATT_UUID_RUNNING_SPEED_CADENCE_SVC,},
};

static esp_bt_uuid_t remote_filter_char_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 = ESP_GATT_UUID_RSC_MEASUREMENT,},
};

static esp_bt_uuid_t remote_filter_char_custom_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 = REMOTE_NOTIFY_CHAR_CUSTOM_UUID,},
};

static esp_bt_uuid_t* dispod_notify_char_uuids[REMOTE_NOTIFY_NUM_UUIDS] = {
    &remote_filter_char_uuid,
    &remote_filter_char_custom_uuid,
};

static esp_bt_uuid_t notify_descr_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG,},
};

static esp_ble_scan_params_t ble_scan_params = {
    .scan_type              = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval          = 0x800, // 0x800, // 0x50,
    .scan_window            = 0x600, // 0x600,// 0x30,
    .scan_duplicate         = BLE_SCAN_DUPLICATE_ENABLE		// TODO: check, was BLE_SCAN_DUPLICATE_DISABLE
};

struct gattc_profile_inst {
    esp_gattc_cb_t gattc_cb;
    uint16_t gattc_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_start_handle;
    uint16_t service_end_handle;
    uint16_t char_handle[REMOTE_NOTIFY_NUM_UUIDS];
    esp_bd_addr_t remote_bda;
};

/* One gatt-based profile one app_id and one gattc_if, this array will store the gattc_if returned by ESP_GATTS_REG_EVT */
static struct gattc_profile_inst gl_profile_tab[PROFILE_NUM] = {
    [PROFILE_A_APP_ID] = {
        .gattc_cb = gattc_profile_event_handler,
        .gattc_if = ESP_GATT_IF_NONE,       /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
    },
};

static void gattc_profile_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
    esp_ble_gattc_cb_param_t *p_data = (esp_ble_gattc_cb_param_t *)param;

    switch (event) {
    case ESP_GATTC_REG_EVT:
        ESP_LOGI(GATTC_TAG, "REG_EVT");
        esp_err_t scan_ret = esp_ble_gap_set_scan_params(&ble_scan_params);
        if (scan_ret){
            ESP_LOGE(GATTC_TAG, "set scan params error, error code = %x", scan_ret);
        }
        break;
    case ESP_GATTC_CONNECT_EVT:
        ESP_LOGI(GATTC_TAG, "ESP_GATTC_CONNECT_EVT conn_id %d, if %d", p_data->connect.conn_id, gattc_if);
        gl_profile_tab[PROFILE_A_APP_ID].conn_id = p_data->connect.conn_id;
        memcpy(gl_profile_tab[PROFILE_A_APP_ID].remote_bda, p_data->connect.remote_bda, sizeof(esp_bd_addr_t));
        ESP_LOGI(GATTC_TAG, "REMOTE BDA:");
        esp_log_buffer_hex(GATTC_TAG, gl_profile_tab[PROFILE_A_APP_ID].remote_bda, sizeof(esp_bd_addr_t));
        esp_err_t mtu_ret = esp_ble_gattc_send_mtu_req (gattc_if, p_data->connect.conn_id);
        if (mtu_ret){
            ESP_LOGE(GATTC_TAG, "config MTU error, error code = %x", mtu_ret);
        }
        break;
    case ESP_GATTC_OPEN_EVT:
		ESP_LOGI(GATTC_TAG, "ESP_GATTC_OPEN_EVT");
        if (param->open.status != ESP_GATT_OK){
            ESP_LOGE(GATTC_TAG, "open failed, status %d", p_data->open.status);
            break;
        }
        ESP_LOGI(GATTC_TAG, "open success");
        break;
    case ESP_GATTC_CFG_MTU_EVT:
        if (param->cfg_mtu.status != ESP_GATT_OK){
            ESP_LOGE(GATTC_TAG,"config mtu failed, error status = %x", param->cfg_mtu.status);
        }
        ESP_LOGI(GATTC_TAG, "ESP_GATTC_CFG_MTU_EVT, Status %d, MTU %d, conn_id %d", param->cfg_mtu.status, param->cfg_mtu.mtu, param->cfg_mtu.conn_id);
        esp_ble_gattc_search_service(gattc_if, param->cfg_mtu.conn_id, &remote_filter_service_uuid);
        break;
    case ESP_GATTC_SEARCH_RES_EVT:
		ESP_LOGI(GATTC_TAG, "ESP_GATTC_SEARCH_RES_EVT");
        ESP_LOGI(GATTC_TAG, "SEARCH RES: conn_id = %x is primary service %d", p_data->search_res.conn_id, p_data->search_res.is_primary);
        ESP_LOGI(GATTC_TAG, "start handle %d end handle %d current handle value %d", p_data->search_res.start_handle, p_data->search_res.end_handle, p_data->search_res.srvc_id.inst_id);
        if (p_data->search_res.srvc_id.uuid.len == ESP_UUID_LEN_16
                && p_data->search_res.srvc_id.uuid.uuid.uuid16 == ESP_GATT_UUID_RUNNING_SPEED_CADENCE_SVC) {
            ESP_LOGI(GATTC_TAG, "service found");
            get_server = true;
            gl_profile_tab[PROFILE_A_APP_ID].service_start_handle = p_data->search_res.start_handle;
            gl_profile_tab[PROFILE_A_APP_ID].service_end_handle = p_data->search_res.end_handle;
            ESP_LOGI(GATTC_TAG, "UUID16: %x", p_data->search_res.srvc_id.uuid.uuid.uuid16);
        }
        break;
    case ESP_GATTC_SEARCH_CMPL_EVT:
        ESP_LOGI(GATTC_TAG, "ESP_GATTC_SEARCH_CMPL_EVT");
        if (p_data->search_cmpl.status != ESP_GATT_OK){
            ESP_LOGE(GATTC_TAG, "search service failed, error status = %x", p_data->search_cmpl.status);
            break;
        }
        if(p_data->search_cmpl.searched_service_source == ESP_GATT_SERVICE_FROM_REMOTE_DEVICE) {
            ESP_LOGI(GATTC_TAG, "Get service information from remote device");
        } else if (p_data->search_cmpl.searched_service_source == ESP_GATT_SERVICE_FROM_NVS_FLASH) {
            ESP_LOGI(GATTC_TAG, "Get service information from flash");
        } else {
            ESP_LOGI(GATTC_TAG, "unknown service source");
        }
        if (get_server){
            uint16_t count = 0;
            esp_gatt_status_t status = esp_ble_gattc_get_attr_count( gattc_if,
                                                                     p_data->search_cmpl.conn_id,
                                                                     ESP_GATT_DB_CHARACTERISTIC,
                                                                     gl_profile_tab[PROFILE_A_APP_ID].service_start_handle,
                                                                     gl_profile_tab[PROFILE_A_APP_ID].service_end_handle,
                                                                     INVALID_HANDLE,
                                                                     &count);
            if (status != ESP_GATT_OK){
                ESP_LOGE(GATTC_TAG, "esp_ble_gattc_get_attr_count error");
            }

            if (count > 0){
                char_elem_result = (esp_gattc_char_elem_t *)malloc(sizeof(esp_gattc_char_elem_t) * count);
                if (!char_elem_result){
                    ESP_LOGE(GATTC_TAG, "gattc no mem");
                }else{
                    for (int idx = 0; idx < REMOTE_NOTIFY_NUM_UUIDS; idx++) {
                        // register for notify for all services
                        status = esp_ble_gattc_get_char_by_uuid( gattc_if,
                                                                 p_data->search_cmpl.conn_id,
                                                                 gl_profile_tab[PROFILE_A_APP_ID].service_start_handle,
                                                                 gl_profile_tab[PROFILE_A_APP_ID].service_end_handle,
                                                                 *dispod_notify_char_uuids[idx],     // TODO
                                                                 char_elem_result,
                                                                 &count);
                        if (status != ESP_GATT_OK){
                            ESP_LOGE(GATTC_TAG, "esp_ble_gattc_get_char_by_uuid error");
                        }

                        // the services we're looking (0x2a53 and 0xff00) only have one char, so use first 'char_elem_result'
                        if (count > 0 && (char_elem_result[0].properties & ESP_GATT_CHAR_PROP_BIT_NOTIFY)){
			    			ESP_LOGI(GATTC_TAG, "esp_ble_gattc_write_char_descr() if %d, handle %d",
				    							 gattc_if,
					    						 char_elem_result[0].char_handle);
						    ESP_LOGI(GATTC_TAG, "REMOTE BDA:");
           					esp_log_buffer_hex(GATTC_TAG, gl_profile_tab[PROFILE_A_APP_ID].remote_bda, sizeof(esp_bd_addr_t));

                            gl_profile_tab[PROFILE_A_APP_ID].char_handle[idx] = char_elem_result[0].char_handle;
                            esp_ble_gattc_register_for_notify (gattc_if, gl_profile_tab[PROFILE_A_APP_ID].remote_bda, char_elem_result[0].char_handle);
                        }
                    }
                }
                /* free char_elem_result */
                free(char_elem_result);

            }else{
                ESP_LOGE(GATTC_TAG, "no char found");
            }
        }
         break;
    case ESP_GATTC_REG_FOR_NOTIFY_EVT:
        ESP_LOGI(GATTC_TAG, "ESP_GATTC_REG_FOR_NOTIFY_EVT");
        if (p_data->reg_for_notify.status != ESP_GATT_OK){
            ESP_LOGE(GATTC_TAG, "REG FOR NOTIFY failed: error status = %d", p_data->reg_for_notify.status);
        }
        else{
            uint16_t count = 0;
            static uint16_t notify_en = 0x1;
            esp_gatt_status_t ret_status = esp_ble_gattc_get_attr_count( gattc_if,
                                                                         gl_profile_tab[PROFILE_A_APP_ID].conn_id,
                                                                         ESP_GATT_DB_DESCRIPTOR,
                                                                         gl_profile_tab[PROFILE_A_APP_ID].service_start_handle,
                                                                         gl_profile_tab[PROFILE_A_APP_ID].service_end_handle,
                                                                         p_data->reg_for_notify.handle,
                                                                         &count);
            if (ret_status != ESP_GATT_OK){
                ESP_LOGE(GATTC_TAG, "esp_ble_gattc_get_attr_count error");
            }
            if (count > 0){
                descr_elem_result = malloc(sizeof(esp_gattc_descr_elem_t) * count);
                if (!descr_elem_result){
                    ESP_LOGE(GATTC_TAG, "malloc error, gattc no mem");
                }else{
                    ret_status = esp_ble_gattc_get_descr_by_char_handle( gattc_if,
                                                                         gl_profile_tab[PROFILE_A_APP_ID].conn_id,
                                                                         p_data->reg_for_notify.handle,
                                                                         notify_descr_uuid,
                                                                         descr_elem_result,
                                                                         &count);
                    if (ret_status != ESP_GATT_OK){
                        ESP_LOGE(GATTC_TAG, "esp_ble_gattc_get_descr_by_char_handle error");
                    }
                    if (count > 0 && descr_elem_result[0].uuid.len == ESP_UUID_LEN_16 && descr_elem_result[0].uuid.uuid.uuid16 == ESP_GATT_UUID_CHAR_CLIENT_CONFIG){
				        ESP_LOGI(GATTC_TAG, "esp_ble_gattc_write_char_descr() if %d, conn_id %d, handle %d, size %u, value %u, type_rsp %u, auth req %u",
																	 gattc_if,
                                                                     gl_profile_tab[PROFILE_A_APP_ID].conn_id,
                                                                     descr_elem_result[0].handle,
                                                                     sizeof(notify_en),
                                                                     notify_en, //  (uint8_t*) &notify_en,
                                                                     ESP_GATT_WRITE_TYPE_RSP,
                                                                     ESP_GATT_AUTH_REQ_NONE);

						ret_status = esp_ble_gattc_write_char_descr( gattc_if,
                                                                     gl_profile_tab[PROFILE_A_APP_ID].conn_id,
                                                                     descr_elem_result[0].handle,
                                                                     sizeof(notify_en),
                                                                     (uint8_t*) &notify_en,
                                                                     ESP_GATT_WRITE_TYPE_RSP,
                                                                     ESP_GATT_AUTH_REQ_NONE);
                    }

                    if (ret_status != ESP_GATT_OK){
                        ESP_LOGE(GATTC_TAG, "esp_ble_gattc_write_char_descr error");
                    }

                    /* free descr_elem_result */
                    free(descr_elem_result);
                }
            }
            else{
                ESP_LOGE(GATTC_TAG, "decsr not found");
            }
        }
        break;
    case ESP_GATTC_NOTIFY_EVT:
        ESP_LOGI(GATTC_TAG, "ESP_GATTC_NOTIFY_EVT");
        if (p_data->notify.is_notify){
            ESP_LOGI(GATTC_TAG, "ESP_GATTC_NOTIFY_EVT, receive notify value:");
        }else{
            ESP_LOGI(GATTC_TAG, "ESP_GATTC_NOTIFY_EVT, receive indicate value:");
        }
        ESP_LOGI(GATTC_TAG, "handle: %u", p_data->notify.handle);
        esp_log_buffer_hex(GATTC_TAG, p_data->notify.value, p_data->notify.value_len);

        switch(p_data->notify.handle){
        case NOTIFY_HANDLE_RSC:{
            // Flags, uint8, pData[0] -> will be used directly

	        // Instantaneous Speed, uint16, pData[1..2], unit: km/h
	        float instSpeedF = (float)((p_data->notify.value[2] << 8) | (p_data->notify.value[1])) * 3.6 / 256.0;

	        // Instantaneous Cadence, uint8, pData[3], unit: RPM = 1/min
	        uint8_t instantaneousCadence = p_data->notify.value[3];

	        // Instantaneous Stride Length, uint16, pData[4..5], unit: cm
	        uint16_t instantaneousStrideLength = (p_data->notify.value[5] << 8) | (p_data->notify.value[4]);

	        // Total Distance, uint32, pData[6..9], unit: m
	        uint32_t totalDistance = ((p_data->notify.value[9] << 24) | (p_data->notify.value[8] << 16)
                                        | (p_data->notify.value[7] << 8) | (p_data->notify.value[6])) / 10;

	        ESP_LOGI(GATTC_TAG, "0x2a53: F %2u S %2.1f C %3u Str %3u Dis %5u", p_data->notify.value[0], instSpeedF, instantaneousCadence, instantaneousStrideLength, totalDistance);

            dispod_runvalues_update_RSCValues(&running_values, instantaneousCadence);
            }
            break;
        case NOTIFY_HANDLE_CUSTOM:{
	        // Flags, uint8, pData[0] -> will be used directly

	        // Stance time, 2 Bytes
	        uint16_t stanceTime = (p_data->notify.value[2] << 8) | (p_data->notify.value[1]);

	        // Foot - 1 Byte:
	        // 0-1 bits for footstike (heel/mid/toe),
	        // 2-3 bits for leg swing (low/mid/high)
	        // 4-5 bits for Rate of impact (low/mid/high)
        	uint8_t footStrike = (BIT1|BIT0) &  p_data->notify.value[3];
	        uint8_t footSwing =  (BIT1|BIT0) & (p_data->notify.value[3] >> 2);
        	uint8_t footImpact = (BIT1|BIT0) & (p_data->notify.value[3] >> 4);

            ESP_LOGI(GATTC_TAG, "0xFF00: F %2u Str %1u Swi %1u Imp %1u Sta%4u", p_data->notify.value[0], footStrike, footSwing, footImpact, stanceTime);

            dispod_runvalues_update_customValues(&running_values, stanceTime, footStrike);
            }
            break;
        default:
            break;
        }
    case ESP_GATTC_WRITE_DESCR_EVT:
        ESP_LOGI(GATTC_TAG, "ESP_GATTC_WRITE_DESCR_EVT");
        if (p_data->write.status != ESP_GATT_OK){
            ESP_LOGE(GATTC_TAG, "write descr failed, error status = %x", p_data->write.status);
            break;
        }
        ESP_LOGI(GATTC_TAG, "write descr success ");
        break;
    case ESP_GATTC_SRVC_CHG_EVT:
        ESP_LOGI(GATTC_TAG, "ESP_GATTC_SRVC_CHG_EVT");
        esp_bd_addr_t bda;
        memcpy(bda, p_data->srvc_chg.remote_bda, sizeof(esp_bd_addr_t));
        ESP_LOGI(GATTC_TAG, "ESP_GATTC_SRVC_CHG_EVT, bd_addr:");
        esp_log_buffer_hex(GATTC_TAG, bda, sizeof(esp_bd_addr_t));
        break;
    case ESP_GATTC_WRITE_CHAR_EVT:
        ESP_LOGI(GATTC_TAG, "ESP_GATTC_WRITE_CHAR_EVT");
        if (p_data->write.status != ESP_GATT_OK){
            ESP_LOGE(GATTC_TAG, "write char failed, error status = %x", p_data->write.status);
            break;
        }
        ESP_LOGI(GATTC_TAG, "write char success ");
        break;
    case ESP_GATTC_DISCONNECT_EVT:
        connect = false;
        get_server = false;
        ESP_LOGI(GATTC_TAG, "ESP_GATTC_DISCONNECT_EVT, reason = %d", p_data->disconnect.reason);
        break;
    default:
        break;
    }
}

static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    uint8_t *adv_name = NULL;
    uint8_t adv_name_len = 0;
    switch (event) {
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: {
		ESP_LOGI(GATTC_TAG, "ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT");
        //the unit of the duration is second
        uint32_t duration = 30;
        esp_ble_gap_start_scanning(duration);
        break;
    }
    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
		ESP_LOGI(GATTC_TAG, "ESP_GAP_BLE_SCAN_START_COMPLETE_EVT");

        //scan start complete event to indicate scan start successfully or failed
        if (param->scan_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(GATTC_TAG, "scan start failed, error status = %x", param->scan_start_cmpl.status);
            break;
        }
        ESP_LOGI(GATTC_TAG, "scan start success");

        break;
    case ESP_GAP_BLE_SCAN_RESULT_EVT: {
	    ESP_LOGI(GATTC_TAG, "ESP_GAP_BLE_SCAN_RESULT_EVT");

        esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
        switch (scan_result->scan_rst.search_evt) {
        case ESP_GAP_SEARCH_INQ_RES_EVT:
            esp_log_buffer_hex(GATTC_TAG, scan_result->scan_rst.bda, 6);
            ESP_LOGI(GATTC_TAG, "searched Adv Data Len %d, Scan Response Len %d", scan_result->scan_rst.adv_data_len, scan_result->scan_rst.scan_rsp_len);
            adv_name = esp_ble_resolve_adv_data(scan_result->scan_rst.ble_adv,
                                                ESP_BLE_AD_TYPE_NAME_CMPL, &adv_name_len);
            ESP_LOGI(GATTC_TAG, "searched Device Name Len %d", adv_name_len);
            esp_log_buffer_char(GATTC_TAG, adv_name, adv_name_len);
            ESP_LOGI(GATTC_TAG, "\n");
            if (adv_name != NULL) {
                // TODO: better control of device we're searching for
                if (strlen(remote_device_name) <= adv_name_len && strncmp((char *)adv_name, remote_device_name, strlen(remote_device_name)) == 0) {
                    ESP_LOGI(GATTC_TAG, "searched device %s\n", remote_device_name);
                    if (connect == false) {
                        connect = true;
                        ESP_LOGI(GATTC_TAG, "connect to the remote device.");
                        esp_ble_gap_stop_scanning();
                        esp_ble_gattc_open(gl_profile_tab[PROFILE_A_APP_ID].gattc_if, scan_result->scan_rst.bda, scan_result->scan_rst.ble_addr_type, true);
                    }
                }
            }
            break;
        case ESP_GAP_SEARCH_INQ_CMPL_EVT:
		    ESP_LOGI(GATTC_TAG, "ESP_GAP_SEARCH_INQ_CMPL_EVT");
            break;
        default:
            break;
        }
        break;
    }

    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
		ESP_LOGI(GATTC_TAG, "ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT");

        if (param->scan_stop_cmpl.status != ESP_BT_STATUS_SUCCESS){
            ESP_LOGE(GATTC_TAG, "scan stop failed, error status = %x", param->scan_stop_cmpl.status);
            break;
        }
        ESP_LOGI(GATTC_TAG, "stop scan successfully");
        break;

    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
		ESP_LOGI(GATTC_TAG, "ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT");

        if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS){
            ESP_LOGE(GATTC_TAG, "adv stop failed, error status = %x", param->adv_stop_cmpl.status);
            break;
        }
        ESP_LOGI(GATTC_TAG, "stop adv successfully");
        break;
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
		ESP_LOGI(GATTC_TAG, "ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT");
        ESP_LOGI(GATTC_TAG, "update connection params status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d",
                  param->update_conn_params.status,
                  param->update_conn_params.min_int,
                  param->update_conn_params.max_int,
                  param->update_conn_params.conn_int,
                  param->update_conn_params.latency,
                  param->update_conn_params.timeout);
        break;
    default:
        break;
    }
}

static void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
	ESP_LOGI(GATTC_TAG, "esp_gattc_cb()");

    /* If event is register event, store the gattc_if for each profile */
    if (event == ESP_GATTC_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK) {
            gl_profile_tab[param->reg.app_id].gattc_if = gattc_if;
        } else {
            ESP_LOGI(GATTC_TAG, "reg app failed, app_id %04x, status %d",
                    param->reg.app_id,
                    param->reg.status);
            return;
        }
    }

    /* If the gattc_if equal to profile A, call profile A cb handler,
     * so here call each profile's callback */
    do {
        int idx;
        for (idx = 0; idx < PROFILE_NUM; idx++) {
            if (gattc_if == ESP_GATT_IF_NONE || /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
                    gattc_if == gl_profile_tab[idx].gattc_if) {
                if (gl_profile_tab[idx].gattc_cb) {
                    gl_profile_tab[idx].gattc_cb(event, gattc_if, param);
                }
            }
        }
    } while (0);
}

void dispod_ble_initialize()
{
    esp_err_t ret;

	ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(GATTC_TAG, "%s initialize controller failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(GATTC_TAG, "%s enable controller failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(GATTC_TAG, "%s init bluetooth failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(GATTC_TAG, "%s enable bluetooth failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    //register the callback function to the gap module
    ret = esp_ble_gap_register_callback(esp_gap_cb);
    if (ret){
        ESP_LOGE(GATTC_TAG, "%s gap register failed, error code = %x\n", __func__, ret);
        return;
    }

    //register the callback function to the gattc module
    ret = esp_ble_gattc_register_callback(esp_gattc_cb);
    if(ret){
        ESP_LOGE(GATTC_TAG, "%s gattc register failed, error code = %x\n", __func__, ret);
        return;
    }

    ret = esp_ble_gattc_app_register(PROFILE_A_APP_ID);
    if (ret){
        ESP_LOGE(GATTC_TAG, "%s gattc app register failed, error code = %x\n", __func__, ret);
    }
    esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(500);
    if (local_mtu_ret){
        ESP_LOGE(GATTC_TAG, "set local  MTU failed, error code = %x", local_mtu_ret);
    }

}
