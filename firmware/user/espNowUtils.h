/*
 * espNowUtils.h
 *
 *  Created on: Oct 29, 2018
 *      Author: adam
 */

#ifndef USER_ESPNOWUTILS_H_
#define USER_ESPNOWUTILS_H_

void ICACHE_FLASH_ATTR espNowInit(void);
void ICACHE_FLASH_ATTR espNowRecvCb(uint8_t* mac_addr, uint8_t* data, uint8_t len);
void ICACHE_FLASH_ATTR espNowSendCb(uint8_t* mac_addr, uint8_t status);
void ICACHE_FLASH_ATTR espNowDeinit(void);

#endif /* USER_ESPNOWUTILS_H_ */
