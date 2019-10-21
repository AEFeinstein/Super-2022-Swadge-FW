/*
 * custom_commands.h
 *
 *  Created on: Oct 13, 2018
 *      Author: adam
 */

#ifndef USER_CUSTOM_COMMANDS_H_
#define USER_CUSTOM_COMMANDS_H_

#define NUM_TT_HIGH_SCORES 3 //Track this many highest scores.
#define NUM_MZ_LEVELS 8 //Track best times for each level

void ICACHE_FLASH_ATTR LoadSettings( void );

uint32_t* ICACHE_FLASH_ATTR ttGetHighScores(void);
void ICACHE_FLASH_ATTR ttSetHighScores(uint32_t* newHighScores);
uint32_t ICACHE_FLASH_ATTR ttGetLastScore(void);
void ICACHE_FLASH_ATTR ttSetLastScore(uint32_t newLastScore);

uint32_t* ICACHE_FLASH_ATTR mzGetBestTimes(void);
void ICACHE_FLASH_ATTR mzSetBestTimes(uint32_t* newHighScores);
uint32_t ICACHE_FLASH_ATTR mzGetLastScore(void);
void ICACHE_FLASH_ATTR mzSetLastScore(uint32_t newLastScore);

void ICACHE_FLASH_ATTR setJoustElo(uint32_t);
uint32_t ICACHE_FLASH_ATTR getJoustElo(void);

void ICACHE_FLASH_ATTR setSnakeHighScore(uint8_t difficulty, uint32_t score);
uint32_t* ICACHE_FLASH_ATTR getSnakeHighScores(void);

#endif /* USER_CUSTOM_COMMANDS_H_ */
