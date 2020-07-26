#ifndef _SYNCED_TIMER_H_
#define _SYNCED_TIMER_H_

#include "osapi.h"

#define timer_t                       os_timer_t
#define timerSetFn(timer, fn, arg)    os_timer_setfn(timer, fn, arg)
#define timerArm(timer, time, repeat) os_timer_arm(timer, time, repeat)
#define timerDisarm(timer)            os_timer_disarm(timer)
#define timersCheck()                 
#define timerFlush()

// #define timer_t                       syncedTimer_t
// #define timerSetFn(timer, fn, arg)    syncedTimerSetFn(timer, fn, arg)
// #define timerArm(timer, time, repeat) syncedTimerArm(timer, time, repeat)
// #define timerDisarm(timer)            syncedTimerDisarm(timer)
// #define timersCheck()                 syncedTimersCheck()
// #define timerFlush()                  syncedTimerFlush()

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
void syncedTimerFlush(void);

#endif