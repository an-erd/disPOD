#ifndef __DISPOD_GATTC_H__
#define __DISPOD_GATTC_H__


// BLE config params
#define DISPOD_BLE_PREDEFINED_BLE       CONFIG_USE_PREDEFINED_BLE
#define DISPOD_BLE_SCAN_PREFIX_DEVICE   CONFIG_BLE_SCAN_PREFIX_DEVICE
#define DISPOD_BLE_DEVICE_COUNT         COINFIG_BLE_DEVICE_COUNT
#define DISPOD_BLE_DEVICE_1_ADDR        CONFIG_BLE_DEVICE_1_ADDR
#define DISPOD_BLE_DEVICE_1_NAME        CONFIG_BLE_DEVICE_1_NAME
#define DISPOD_BLE_DEVICE_2_ADDR        CONFIG_BLE_DEVICE_2_ADDR
#define DISPOD_BLE_DEVICE_2_NAME        CONFIG_BLE_DEVICE_2_NAME
#define DISPOD_BLE_DEVICE_3_ADDR        CONFIG_BLE_DEVICE_3_ADDR
#define DISPOD_BLE_DEVICE_3_NAME        CONFIG_BLE_DEVICE_3_NAME
#define DISPOD_BLE_DEVICE_4_ADDR        CONFIG_BLE_DEVICE_4_ADDR
#define DISPOD_BLE_DEVICE_4_NAME        CONFIG_BLE_DEVICE_4_NAME
#define DISPOD_BLE_DEVICE_5_ADDR        CONFIG_BLE_DEVICE_5_ADDR
#define DISPOD_BLE_DEVICE_5_NAME        CONFIG_BLE_DEVICE_5_NAME

void dispod_ble_initialize();

#endif // __DISPOD_GATTC_H__
