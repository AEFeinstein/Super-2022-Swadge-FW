#ifndef _GPIO_BUTTONS_H
#define _GPIO_BUTTONS_H

#include "user_interface.h"

//Change this per controller.
#define TOTAL_BUTTONS_THIS_CONTROLLER 6


//For original controller:
//0x04 = 0b00000100 = Left

void ICACHE_FLASH_ATTR SetupGPIO();


//Don't call, use LastGPIOState
unsigned char ICACHE_FLASH_ATTR GetButtons();

//You write.
void ICACHE_FLASH_ATTR HandleButtonEvent( uint8_t state, int button, int down );

extern volatile uint8_t LastGPIOState; //From last "GetButtons()" command.  Will not be updated until after interrupt and all HandleButtonEvent messages have been called.

#endif



