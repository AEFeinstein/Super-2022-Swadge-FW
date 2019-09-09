/*
 * custom_commands.h
 *
 *  Created on: Oct 13, 2018
 *      Author: adam
 */

#ifndef USER_CUSTOM_COMMANDS_H_
#define USER_CUSTOM_COMMANDS_H_

void ICACHE_FLASH_ATTR LoadSettings( void );
uint8_t ICACHE_FLASH_ATTR getRefGameWins(void);
void ICACHE_FLASH_ATTR incrementRefGameWins(void);
void ICACHE_FLASH_ATTR setGameWinsToMax(void);
void ICACHE_FLASH_ATTR setJoustElo(uint32_t);
uint32_t ICACHE_FLASH_ATTR getJoustElo(void);

#endif /* USER_CUSTOM_COMMANDS_H_ */
