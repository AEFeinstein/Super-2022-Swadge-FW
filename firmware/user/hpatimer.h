//Copyright 2015 <>< Charles Lohr Under the MIT/x11 License, NewBSD License or
// ColorChord License.  You Choose.

//This is a tool to make the ESP8266 run its ADC and pipe the samples into
//the sounddata fifo.

#ifndef _HPATIMER_H
#define _HPATIMER_H

#include "buzzer.h"

void ICACHE_FLASH_ATTR StartHPATimer(void);
void ContinueHPATimer(void);
void PauseHPATimer(void);
bool ICACHE_FLASH_ATTR isHpaRunning(void);

void ICACHE_FLASH_ATTR initBuzzer(void);
void ICACHE_FLASH_ATTR setBuzzerNote(notePeriod_t note);
void ICACHE_FLASH_ATTR stopBuzzerSong(void);
void ICACHE_FLASH_ATTR startBuzzerSong(const song_t* song);

#endif

