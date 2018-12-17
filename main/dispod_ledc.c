#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "esp_system.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "sdkconfig.h"

#include "dispod_config.h"
#include "dispod_ledc.h"


#define c 261
#define d 294
#define e 329
#define f 349
#define g 391
#define gS 415
#define a 440
#define aS 455
#define b 466
#define cH 523
#define cSH 554
#define dH 587
#define dSH 622
#define eH 659
#define fH 698
#define fSH 740
#define gH 784
#define gSH 830
#define aH 880

#define GPIO_OUTPUT_SPEED LEDC_HIGH_SPEED_MODE

uint32_t map(uint32_t x, uint32_t in_min, uint32_t in_max, uint32_t out_min, uint32_t out_max)
{
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}


void sound(int gpio_num, uint32_t freq, uint32_t duration, uint32_t volume)
{
	ledc_timer_config_t timer_conf;
    memset(&timer_conf, 0, sizeof(ledc_timer_config_t));

	timer_conf.speed_mode       = GPIO_OUTPUT_SPEED;
	timer_conf.duty_resolution  = LEDC_TIMER_10_BIT;
	timer_conf.timer_num        = LEDC_TIMER_0;
	timer_conf.freq_hz          = freq;
	ledc_timer_config(&timer_conf);

	ledc_channel_config_t ledc_conf;
    memset(&ledc_conf, 0, sizeof(ledc_channel_config_t));

	ledc_conf.gpio_num          = gpio_num;
	ledc_conf.speed_mode        = GPIO_OUTPUT_SPEED;
	ledc_conf.channel           = LEDC_CHANNEL_0;
	ledc_conf.intr_type         = LEDC_INTR_DISABLE;
	ledc_conf.timer_sel         = LEDC_TIMER_0;
	ledc_conf.duty              = 0x0;  // 50%=0x3FFF, 100%=0x7FFF for 15 Bit
	                                    // 50%=0x01FF, 100%=0x03FF for 10 Bit
	ledc_channel_config(&ledc_conf);

    // uint32_t new_duty = map(volume, 0, 100, 0, 0x03FF);

	// start
    ledc_set_duty(GPIO_OUTPUT_SPEED, LEDC_CHANNEL_0, volume); // 0x7F -> 12% duty - play here for your speaker or buzzer
    ledc_update_duty(GPIO_OUTPUT_SPEED, LEDC_CHANNEL_0);

	vTaskDelay(duration/portTICK_PERIOD_MS);

    // stop
    ledc_set_duty(GPIO_OUTPUT_SPEED, LEDC_CHANNEL_0, 0);
    ledc_update_duty(GPIO_OUTPUT_SPEED, LEDC_CHANNEL_0);
}
