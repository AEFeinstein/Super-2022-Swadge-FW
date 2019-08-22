#ifndef _GPIO_BUTTONS_H
#define _GPIO_BUTTONS_H

void ICACHE_FLASH_ATTR SetupGPIO(bool enableMic);
uint8_t ICACHE_FLASH_ATTR getLastGPIOState(void);
unsigned char ICACHE_FLASH_ATTR GetButtons(void);
void ICACHE_FLASH_ATTR setOledResetOn(bool on);
void ICACHE_FLASH_ATTR setBuzzerOn(bool on);
bool ICACHE_FLASH_ATTR getBuzzerState(void);
void ICACHE_FLASH_ATTR setGpiosForBoot(void);

#endif
