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
    void (*timerFn)(void*); ///< This is a function which will be attached to the timer
    uint32_t period;        ///< This is the period, in ms, at which the function will be called
} timerWithPeriod;

extern timerWithPeriod danceTimers[];
uint8_t getNumDances(void);

void ICACHE_FLASH_ATTR setDanceBrightness(uint8_t brightness);
void ICACHE_FLASH_ATTR danceTimerMode1(void* arg);
void ICACHE_FLASH_ATTR danceTimerMode2(void* arg);
void ICACHE_FLASH_ATTR danceTimerMode3(void* arg);
void ICACHE_FLASH_ATTR danceTimerMode4(void* arg);
void ICACHE_FLASH_ATTR danceTimerMode5(void* arg);
void ICACHE_FLASH_ATTR danceTimerMode6(void* arg);
void ICACHE_FLASH_ATTR danceTimerMode7(void* arg);
void ICACHE_FLASH_ATTR danceTimerMode8(void* arg);
void ICACHE_FLASH_ATTR danceTimerMode9(void* arg);
void ICACHE_FLASH_ATTR danceTimerMode10(void* arg);
void ICACHE_FLASH_ATTR danceTimerMode11(void* arg);
void ICACHE_FLASH_ATTR danceTimerMode12(void* arg);
void ICACHE_FLASH_ATTR danceTimerMode13(void* arg);
void ICACHE_FLASH_ATTR danceTimerMode14(void* arg);
void ICACHE_FLASH_ATTR danceTimerMode15(void* arg);
void ICACHE_FLASH_ATTR danceTimerMode16(void* arg);
void ICACHE_FLASH_ATTR danceTimerMode17(void* arg);
void ICACHE_FLASH_ATTR danceTimerMode18(void* arg);

#endif /* MODE_DANCE_H_ */
