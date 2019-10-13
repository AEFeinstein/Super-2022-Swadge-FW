/*
 * custom_commands.h
 *
 *  Created on: Oct 13, 2018
 *      Author: adam
 */

#ifndef USER_CUSTOM_COMMANDS_H_
#define USER_CUSTOM_COMMANDS_H_

void ICACHE_FLASH_ATTR LoadSettings( void );
void ICACHE_FLASH_ATTR setJoustElo(uint32_t);
uint32_t ICACHE_FLASH_ATTR getJoustElo(void);
void ICACHE_FLASH_ATTR setSnakeHighScore(uint8_t difficulty, uint32_t score);
uint32_t* ICACHE_FLASH_ATTR getSnakeHighScores(void);

#endif /* USER_CUSTOM_COMMANDS_H_ */
