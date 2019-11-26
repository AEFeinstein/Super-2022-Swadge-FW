/*
 * mode_dance.h
 *
 *  Created on: Nov 10, 2018
 *      Author: adam
 */

#ifndef MODE_DANCE_H_
#define MODE_DANCE_H_


typedef struct
{
    // os_timer_t timer;       ///< This is a system timer
    void (*timerFn)(void*);  ///< This is a function which will be attached to the timer
    uint32_t period;        ///< This is the period, in ms, at which the function will be called
} timerWithPeriod;

extern timerWithPeriod danceTimers[];
uint8_t getNumDances(void);

void ICACHE_FLASH_ATTR setDanceBrightness(uint8_t brightness);

#endif /* MODE_DANCE_H_ */
