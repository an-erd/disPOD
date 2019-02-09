#ifndef __DISPOD_LEDC_H__
#define __DISPOD_LEDC_H__

void dispod_init_beep(int gpio_num, uint32_t freq);
void dispod_beep(uint32_t volume);

#endif // __DISPOD_LEDC_H__
