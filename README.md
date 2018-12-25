
### disPOD

# disPOD modules

disPOD consists of several modules:

## main.c

* `void app_main()` does adjustment of log level, initialization of NVS Flash, SPIFFS file system,  disPOD overall initialization (event groups: `dispod_event_groups`, `dispod_display_evg`, queue: `running_values_queue`) and runs the different tasks (`dispod_collector_task`, `dispod_update_task`, `dispod_screen_task`, `dispod_archiver_task`)
* `run_on_event` is the overall program logic
* At the end of `app_main()` the event to startup disPOD (`DISPOD_STARTUP_EVT`) is put in the loop.

## `dispod_gattc.h` and `dispod_gattc.c`

* handles all the BLE process from scanning for devices, connecting, register notification and receiving/forwarding notifications.

## `dispoD_wifi.h` and `dispod_wifi.c`

* by calling `dispod_wifi_network_up()`the connection to WiFi is made, using the predefined SSID/PWD combinations. Also, an event handler is registered for the internal WiFi connection process and update of the disPOD WiFi status.

## `dispod_sntp.h` and `dispod_sntp.c`

* When WiFi is available, a call to `dispod_sntp_check_time()` tries to update the time. The time is used for time stamps during logging the running data, and correct time stamps of the SD card files.

## `dispod_tft.h` and `dispod_tft.c`

* HW initialization is done in `void dispod_display_initialize()`
* functions to udpate the data to display on the TFT are provided
* functions to actually draw the screens are provided

## `dispod_runvalues.h` and `dispod_runvalues.c`

* structures to hold the data are given
* functions to calculate the mean to smoothen the received values from the BLE device are provided

## `dispod_archiver.h` and `dispod_archiver.c`

* data structures and functions to store the running data in an array an to bulk write to SD card as soon as at least one buffer is complete.

## `dispod_ledc.h` and `dispod_ledc.c`

* functions to output a beep are provided (will be used for the metronome) are provided.

# Used components

## Copied from `espressif/esp-iot-solution`, `#49f305f`

* `components/button`
* `components/general/debugs` (unused)
* `components/general/ota` (unused)
* `components/general/param` (unused)
* `components/wifi_conn` (unused)

## Copied from `ESP32_TFT_library`, `#aa21772`

* `components/spidriver`
* `components/tft`
* `tools/`
* `main/tft_demo.c` used as a basis for my own programming

# Data Flow

## Tasks and Modules

### `dispod_collector_task`:

* From `dispod_gattc.c` for each BLE notification received the function `gattc_profile_event_handler()` will be called for an `ESP_GATTC_NOTIFY_EVT` event
* the payload is deserialized
* the element containing the data (RSC or custom profile) will be put in the `running_values_queue` using the `typedef struct running_values_queue_element_t`.

### `dispod_update_task`

* task is blocked until the queue `running_values_queue` contains at least one element.
* the head element will be removed from the queue
* this item will be stored in the archivers buffer (w/o any queue)
* and also the values will be used to smoothen the values to display and hand over to the `dispod_screen_task`, a call to TODO (w/o any queue)

### `dispod_screen_task`

* blocked until `DISPOD_DISPLAY_UPDATE_BIT` in `dispod_display_evg` set
* update the screen or change the screen (e.g. from status to running values )

### `dispod_archiver_task`

* if a buffer is completed, write to disk
* if `DISPOD_SD_WRITE_COMPLETED_BUFFER_EVT` in the event group `EventGroupHandle_t dispod_sd_evg` is set, write all completed buffers to SD card
* if `DISPOD_SD_WRITE_CURRENT_BUFFER_EVT` in the event group `EventGroupHandle_t dispod_sd_evg` is set, write the current (partial)  buffer to SD card