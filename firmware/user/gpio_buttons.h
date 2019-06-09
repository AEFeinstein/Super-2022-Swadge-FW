#ifndef _GPIO_BUTTONS_H
#define _GPIO_BUTTONS_H

#include "user_interface.h"
#include "user_main.h"

typedef enum {
	UP    = 0x01,
	DOWN  = 0x02,
	LEFT  = 0x04,
	RIGHT = 0x08,
} button_mask;

void ICACHE_FLASH_ATTR SetupGPIO(void (*handler)(uint8_t state, int button, int down), bool enableMic);
uint8_t ICACHE_FLASH_ATTR getLastGPIOState(void);
unsigned char GetButtons();
void setOledResetOn(bool on);

#endif



