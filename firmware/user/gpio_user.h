#ifndef _GPIO_BUTTONS_H
#define _GPIO_BUTTONS_H

void ICACHE_FLASH_ATTR SetupGPIO(bool enableMic);
uint8_t ICACHE_FLASH_ATTR getLastGPIOState(void);
unsigned char GetButtons();
void setOledResetOn(bool on);
void ICACHE_FLASH_ATTR setGpiosForBoot(void);

#endif
