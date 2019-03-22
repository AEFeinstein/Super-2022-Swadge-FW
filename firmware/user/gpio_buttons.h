#ifndef _GPIO_BUTTONS_H
#define _GPIO_BUTTONS_H

#include "user_interface.h"
#include "user_main.h"

void ICACHE_FLASH_ATTR SetupGPIO(void (*handler)(uint8_t state, int button, int down), bool enableMic);
uint8_t ICACHE_FLASH_ATTR getLastGPIOState(void);
unsigned char GetButtons();
void setOledResetOn(bool on);

#endif



