/*
 * mode_ddr.h
 *
 *  Created on: May 13, 2020
 *      Author: rick
 */

#ifndef MODES_MODE_DDR_H_
#define MODES_MODE_DDR_H_
#define DDR_HIGHSCORE_LEN 8

#include "user_main.h"
extern swadgeMode ddrMode;

typedef enum
{
    DDR_VERY_EASY,
    DDR_EASY,
    DDR_MEDIUM,
    DDR_HARD
} ddrDifficultyType;

typedef enum
{
    DDR_PASS,
    DDR_MANY_HIT,
    DDR_MANY_PERFECT,
    DDR_ALL_HIT,
    DDR_ALL_PERFECT,
    DDR_FAIL
} ddrWinType;

typedef struct __attribute__((aligned(4)))
{
    ddrWinType winType;
    uint32_t score;
}
ddrWinResult_t;


typedef struct __attribute__((aligned(4)))
{
    ddrWinResult_t veryEasyWins[DDR_HIGHSCORE_LEN];
    ddrWinResult_t easyWins[DDR_HIGHSCORE_LEN];
    ddrWinResult_t mediumWins[DDR_HIGHSCORE_LEN];
    ddrWinResult_t hardWins[DDR_HIGHSCORE_LEN];
}
ddrHighScores_t;

#endif /* MODES_MODE_DDR_H_ */