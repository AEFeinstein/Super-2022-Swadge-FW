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
uint32_t ICACHE_FLASH_ATTR ttHighScoreGet(void);
void ICACHE_FLASH_ATTR ttHighScoreSet(uint32_t newHighScore);
uint32_t ICACHE_FLASH_ATTR ttLastScoreGet(void);
void ICACHE_FLASH_ATTR ttLastScoreSet(uint32_t newLastScore);

#endif /* USER_CUSTOM_COMMANDS_H_ */
