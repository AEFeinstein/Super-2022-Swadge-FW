//Copyright 2015 <>< Charles Lohr Under the MIT/x11 License, NewBSD License or
// ColorChord License.  You Choose.

//This is a tool to make the ESP8266 run its ADC and pipe the samples into
//the sounddata fifo.

#ifndef _HPATIMER_H
#define _HPATIMER_H

#include <c_types.h>
#include "ccconfig.h" //For DFREQ

//Using a system timer on the ESP to poll the ADC in at a regular interval...

//BUFFSIZE must be a power-of-two
#define HPABUFFSIZE 512

uint8_t ICACHE_FLASH_ATTR getSample(void);
bool ICACHE_FLASH_ATTR sampleAvailable(void);

void ICACHE_FLASH_ATTR StartHPATimer(void);
void ICACHE_FLASH_ATTR ContinueHPATimer(void);
void ICACHE_FLASH_ATTR PauseHPATimer(void);


#endif

