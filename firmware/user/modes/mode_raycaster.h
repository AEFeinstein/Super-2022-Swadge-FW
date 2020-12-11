#ifndef _MODE_RAYCASTER_H_
#define _MODE_RAYCASTER_H_

typedef enum
{
    RC_EASY,
    RC_MED,
    RC_HARD,
    RC_NUM_DIFFICULTIES
} raycasterDifficulty_t;

typedef struct __attribute__((aligned(4)))
{
    uint16_t kills;
    uint32_t tElapsedUs;
}
raycasterScore_t;

#define RC_NUM_SCORES 4

typedef struct __attribute__((aligned(4)))
{
    raycasterScore_t scores[RC_NUM_DIFFICULTIES][RC_NUM_SCORES];
}
raycasterScores_t;

extern swadgeMode raycasterMode;

#endif