#ifndef __DISPOD_TIMER_H__
#define __DISPOD_TIMER_H__

// disPOD timer event group
#define DISPOD_TIMER_METRONOME_BIT           (BIT0)
extern EventGroupHandle_t dispod_timer_evg;

void dispod_timer_task(void *pvParameters);

#endif // __DISPOD_TIMER_H__
