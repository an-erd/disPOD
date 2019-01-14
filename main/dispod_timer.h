#ifndef __DISPOD_TIMER_H__
#define __DISPOD_TIMER_H__

#define	METRONOME_TIMER_US				333333
#define METRONOME_LIGHT_DURATION_US		 50000
#define METRONOME_SOUND_DURATION_US		 30000

// disPOD timer event group
#define DISPOD_TIMER_METRONOME_ON_BIT           (BIT0)
#define DISPOD_TIMER_METRONOME_OFF_LIGHT_BIT	(BIT1)
#define DISPOD_TIMER_METRONOME_OFF_SOUND_BIT	(BIT2)
extern EventGroupHandle_t dispod_timer_evg;

void dispod_timer_initialize();
void dispod_timer_start_metronome();
void dispod_timer_stop_metronome();

void dispod_timer_task(void *pvParameters);

#endif // __DISPOD_TIMER_H__
