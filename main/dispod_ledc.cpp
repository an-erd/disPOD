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

#include "dispod_main.h"
#include "dispod_ledc.h"

static const char* TAG = "DISPOD_LEDC";

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
#define beep_freq 1000

#define GPIO_OUTPUT_SPEED LEDC_HIGH_SPEED_MODE

static uint16_t volume_map[16] = {   0,
                                     1,   3,  10,  30,  60,
                                    90, 120, 150, 180, 230,
                                   260, 300, 350, 450, 600 };

static ledc_timer_config_t timer_conf;
static ledc_channel_config_t ledc_conf;

void dispod_init_beep(int gpio_num, uint32_t freq)
{
    ESP_LOGD(TAG, "dispod_init_beep()");
    memset(&timer_conf, 0, sizeof(ledc_timer_config_t));
	timer_conf.speed_mode       = GPIO_OUTPUT_SPEED;
	timer_conf.duty_resolution  = LEDC_TIMER_10_BIT;
	timer_conf.timer_num        = LEDC_TIMER_0;
	timer_conf.freq_hz          = freq;
	ledc_timer_config(&timer_conf);

    memset(&ledc_conf, 0, sizeof(ledc_channel_config_t));
	ledc_conf.gpio_num          = gpio_num;
	ledc_conf.speed_mode        = GPIO_OUTPUT_SPEED;
	ledc_conf.channel           = LEDC_CHANNEL_0;
	ledc_conf.intr_type         = LEDC_INTR_DISABLE;
	ledc_conf.timer_sel         = LEDC_TIMER_0;
	ledc_conf.duty              = 0x0;  // 50%=0x3FFF, 100%=0x7FFF for 15 Bit
	                                    // 50%=0x01FF, 100%=0x03FF for 10 Bit
	ledc_channel_config(&ledc_conf);
}

void dispod_beep(uint32_t volume)
{
    // uint32_t new_duty = map(volume, 0, 10, 0, 0x01FF);
    ESP_LOGD(TAG, "volume %u, mapped %u)", volume, volume_map[volume]);

    ledc_set_duty(GPIO_OUTPUT_SPEED, LEDC_CHANNEL_0, volume_map[volume]);
    ledc_update_duty(GPIO_OUTPUT_SPEED, LEDC_CHANNEL_0);
}
