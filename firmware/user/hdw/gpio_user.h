#ifndef _GPIO_BUTTONS_H
#define _GPIO_BUTTONS_H

#include "user_config.h"

void ICACHE_FLASH_ATTR SetupGPIO(void);
uint8_t ICACHE_FLASH_ATTR getLastGPIOState(void);
unsigned char ICACHE_FLASH_ATTR GetButtons(void);
#if defined(FEATURE_OLED)
    void ICACHE_FLASH_ATTR setOledResetOn(bool on);
#endif
#if defined(FEATURE_BZR)
    void ICACHE_FLASH_ATTR setBuzzerGpio(bool on);
    bool ICACHE_FLASH_ATTR getBuzzerGpio(void);
#endif
void ICACHE_FLASH_ATTR setGpiosForBoot(void);

#endif
