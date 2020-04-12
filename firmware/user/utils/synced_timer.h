#ifndef _SYNCED_TIMER_H_
#define _SYNCED_TIMER_H_

#include "osapi.h"

typedef struct
{
    os_timer_t osTimer;
    uint8_t shouldRunCnt;
    os_timer_func_t* timerFunc;
    void* arg;
    bool isArmed;
    bool isRepeat;
} syncedTimer_t;

void syncedTimerDisarm(syncedTimer_t* timer);
void syncedTimersCheck(void);
void syncedTimerArm(syncedTimer_t* timer, uint32_t time, bool repeat_flag);
void syncedTimerSetFn(syncedTimer_t* newTimer, void (*timerFunc)(void*), void* arg);

#endif