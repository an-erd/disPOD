#ifndef __DISPOD_IDLE_TIMER_H__
#define __DISPOD_IDLE_TIMER_H__

void dispod_idle_timer_set(uint32_t duration);
void dispod_idle_timer_stop();
bool dispod_idle_timer_expired();
void dispod_touch_timer();

#endif // __DISPOD_IDLE_TIMER_H__
