#ifndef _GPIO_BUTTONS_H
#define _GPIO_BUTTONS_H

#include "user_interface.h"

typedef void (*ButtonHandler)(uint8_t state, int button, int down);

void ICACHE_FLASH_ATTR SetupGPIO(ButtonHandler handler);
uint8_t ICACHE_FLASH_ATTR getLastGPIOState(void);

#endif



