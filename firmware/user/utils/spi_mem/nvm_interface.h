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

// For raycasterScore_t
#include "mode_raycaster.h"

// For mtHighScores_t
#include "mode_mtype.h"

#define NUM_DEMON_MEMORIALS 8

typedef struct __attribute__((aligned(4)))
{
    char name[32];
    int32_t actionsTaken;
}
demonMemorial_t;


#define NUM_FLIGHTSIM_TOP_SCORES 4
#define FLIGHT_HIGH_SCORE_NAME_LEN 4

typedef struct __attribute__((aligned(4)))
{
    //One set for any% one set for 100%
    char displayName[NUM_FLIGHTSIM_TOP_SCORES * 2][FLIGHT_HIGH_SCORE_NAME_LEN];
    uint32_t timeCentiseconds[NUM_FLIGHTSIM_TOP_SCORES * 2];
    uint8_t flightInvertY;
}
flightSimSaveData_t;

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

void ICACHE_FLASH_ATTR setMTScores(mtHighScores_t* highScores);
void ICACHE_FLASH_ATTR getMTScores(mtHighScores_t* highScores);

demonMemorial_t* ICACHE_FLASH_ATTR getDemonMemorials(void);
void ICACHE_FLASH_ATTR addDemonMemorial(char* name, int32_t actionsTaken);

flightSimSaveData_t* ICACHE_FLASH_ATTR getFlightSaveData(void);
void ICACHE_FLASH_ATTR setFlightSaveData( flightSimSaveData_t* t );

void ICACHE_FLASH_ATTR setGitHash(char* hash);
void ICACHE_FLASH_ATTR getGitHash(char* hash);

void ICACHE_FLASH_ATTR setSelfTestPass(bool pass);
bool ICACHE_FLASH_ATTR getSelfTestPass(void);

raycasterScores_t* ICACHE_FLASH_ATTR getRaycasterScores(void);
void ICACHE_FLASH_ATTR addRaycasterScore(raycasterDifficulty_t difficulty, raycasterMap_t mapIdx, uint16_t kills, uint32_t tElapsed);

#define SSID_NAME_LEN 64

void ICACHE_FLASH_ATTR setSsidPw(char* ssid, char* pw);
void ICACHE_FLASH_ATTR getSsidPw(char* ssid, char* pw);

#endif /* USER_CUSTOM_COMMANDS_H_ */
