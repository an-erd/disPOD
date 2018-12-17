#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tftspi.h"
#include "tft.h"
#include "driver/gpio.h"
#include "dispod_tft.h"

static const char* TAG = "DISPOD_TFT";

void dispod_display_initialize()
{
    esp_err_t ret;

    // set display configuration, SPI devices
	tft_disp_type = DISP_TYPE_ILI9341;
	_width = DEFAULT_TFT_DISPLAY_WIDTH;     // smaller dimension
	_height = DEFAULT_TFT_DISPLAY_HEIGHT;   // larger dimension
	max_rdclock = 8000000;
    TFT_PinsInit();
    spi_lobo_device_handle_t spi;
    spi_lobo_bus_config_t buscfg={
        .miso_io_num=PIN_NUM_MISO,				// set SPI MISO pin
        .mosi_io_num=PIN_NUM_MOSI,				// set SPI MOSI pin
        .sclk_io_num=PIN_NUM_CLK,				// set SPI CLK pin
        .quadwp_io_num=-1,
        .quadhd_io_num=-1,
		.max_transfer_sz = 6*1024,
    };
    spi_lobo_device_interface_config_t devcfg={
        .clock_speed_hz=8000000,                // Initial clock out at 8 MHz
        .mode=0,                                // SPI mode 0
        .spics_io_num=-1,                       // we will use external CS pin
		.spics_ext_io_num=PIN_NUM_CS,           // external CS pin
		.flags=LB_SPI_DEVICE_HALFDUPLEX,        // ALWAYS SET  to HALF DUPLEX MODE!! for display spi
    };

    vTaskDelay(500 / portTICK_RATE_MS);
    ESP_LOGI(TAG, "Display config, Pins used: miso=%d, mosi=%d, sck=%d, cs=%d", PIN_NUM_MISO, PIN_NUM_MOSI, PIN_NUM_CLK, PIN_NUM_CS);

    // Initialize SPI bus and attach the LCD to the SPI bus
	ret = spi_lobo_bus_add_device(SPI_BUS, &buscfg, &devcfg, &spi);
    assert(ret == ESP_OK);
    ESP_LOGI(TAG, "SPI: display device added to spi bus (%d)", SPI_BUS);
	disp_spi = spi;

	// ==== Test select/deselect ====
	ret = spi_lobo_device_select(spi, 1);
    assert(ret==ESP_OK);
	ret = spi_lobo_device_deselect(spi);
    assert(ret==ESP_OK);

    ESP_LOGI(TAG, "SPI: attached display device, speed=%u", spi_lobo_get_speed(spi));
	ESP_LOGI(TAG, "SPI: bus uses native pins: %s", spi_lobo_uses_native_pins(spi) ? "true" : "false");

	// Initialize the Display ====

	ESP_LOGI(TAG, "SPI: display init... ");
	TFT_display_init();
	ESP_LOGI(TAG, "OK");

	// Detect maximum read speed
	max_rdclock = find_rd_speed();
	ESP_LOGI(TAG, "SPI: Max rd speed = %u", max_rdclock);

    // Set SPI clock used for display operations
	spi_lobo_set_speed(spi, DEFAULT_SPI_CLOCK);
	ESP_LOGI(TAG,"SPI: Changed speed to %u", spi_lobo_get_speed(spi));

    // set orientation, etc.
	font_rotate = 0;
	text_wrap = 0;
	font_transparent = 0;
	font_forceFixed = 0;
	gray_scale = 0;
    TFT_setGammaCurve(DEFAULT_GAMMA_CURVE);
	TFT_setRotation(PORTRAIT);
	TFT_setFont(DEFAULT_FONT, NULL);
	TFT_resetclipwin();
}
