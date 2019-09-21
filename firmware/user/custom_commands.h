/*
 * custom_commands.h
 *
 *  Created on: Oct 13, 2018
 *      Author: adam
 */

#ifndef USER_CUSTOM_COMMANDS_H_
#define USER_CUSTOM_COMMANDS_H_

#define NUM_TT_HIGH_SCORES 3 //Track this many highest scores.
#define NUM_MZ_HIGH_SCORES 3 //Track this many highest scores.

void ICACHE_FLASH_ATTR LoadSettings( void );
uint8_t ICACHE_FLASH_ATTR getRefGameWins(void);
void ICACHE_FLASH_ATTR incrementRefGameWins(void);
void ICACHE_FLASH_ATTR setGameWinsToMax(void);
uint32_t * ICACHE_FLASH_ATTR ttGetHighScores(void);
void ICACHE_FLASH_ATTR ttSetHighScores(uint32_t * newHighScores);
uint32_t ICACHE_FLASH_ATTR ttGetLastScore(void);
void ICACHE_FLASH_ATTR ttSetLastScore(uint32_t newLastScore);
uint32_t * ICACHE_FLASH_ATTR mzGetHighScores(void);
void ICACHE_FLASH_ATTR mzSetHighScores(uint32_t * newHighScores);
uint32_t ICACHE_FLASH_ATTR mzGetLastScore(void);
void ICACHE_FLASH_ATTR mzSetLastScore(uint32_t newLastScore);
#endif /* USER_CUSTOM_COMMANDS_H_ */
