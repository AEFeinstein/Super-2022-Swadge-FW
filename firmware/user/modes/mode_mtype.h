/*
 * mode_mtype.h
 *
 *  Created on: Aug 11, 2020
 *      Author: Jonathan Moriarty
 */

#ifndef MODES_MODE_MTYPE_H_
#define MODES_MODE_MTYPE_H_

#define MT_NUM_HIGHSCORES 8

typedef struct __attribute__((aligned(4)))
{
    uint32_t timeSurvived;
    uint32_t score;
}
mtScore_t;


typedef struct __attribute__((aligned(4)))
{
    mtScore_t easyScores[MT_NUM_HIGHSCORES];
    mtScore_t mediumScores[MT_NUM_HIGHSCORES];
    mtScore_t hardScores[MT_NUM_HIGHSCORES];
    mtScore_t veryhardScores[MT_NUM_HIGHSCORES];
}
mtHighScores_t;

extern swadgeMode mTypeMode;

#endif /* MODES_MODE_MTYPE_H_ */