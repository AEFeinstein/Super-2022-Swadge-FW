/*
 * nvm_interface.h
 *
 *  Created on: Oct 13, 2018
 *      Author: adam
 */

#ifndef USER_CUSTOM_COMMANDS_H_
#define USER_CUSTOM_COMMANDS_H_

// For demon_t
#include "personal_demon/logic_personal_demon.h"

// For ddrHighScores_t
#include "mode_ddr.h"

#define NUM_DEMON_MEMORIALS 8

typedef struct
{
    char name[32];
    int32_t actionsTaken;
} demonMemorial_t;

void ICACHE_FLASH_ATTR LoadSettings( void );

#if defined(FEATURE_BZR)
    void ICACHE_FLASH_ATTR setMuteOverride(bool opt);
    void ICACHE_FLASH_ATTR setIsMutedOption(bool mute);
    bool ICACHE_FLASH_ATTR getIsMutedOption(void);
#endif

uint8_t ICACHE_FLASH_ATTR getMenuPos(void);
void ICACHE_FLASH_ATTR setMenuPos(uint8_t pos);

void ICACHE_FLASH_ATTR setSavedDemon(demon_t* demon);
void ICACHE_FLASH_ATTR getSavedDemon(demon_t* demon);

void ICACHE_FLASH_ATTR setDDRScores(ddrHighScores_t* highScores);
void ICACHE_FLASH_ATTR getDDRScores(ddrHighScores_t* highScores);

demonMemorial_t* ICACHE_FLASH_ATTR getDemonMemorials(void);
void ICACHE_FLASH_ATTR addDemonMemorial(char* name, int32_t actionsTaken);

void ICACHE_FLASH_ATTR setGitHash(char* hash);
void ICACHE_FLASH_ATTR getGitHash(char* hash);

void ICACHE_FLASH_ATTR setSelfTestPass(bool pass);
bool ICACHE_FLASH_ATTR getSelfTestPass(void);

#endif /* USER_CUSTOM_COMMANDS_H_ */
