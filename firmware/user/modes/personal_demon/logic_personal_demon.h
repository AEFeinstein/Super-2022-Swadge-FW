#ifndef _LOGIC_PERSONAL_DEMON_H_
#define _LOGIC_PERSONAL_DEMON_H_

/*******************************************************************************
 * Defines
 ******************************************************************************/

#define STOMACH_SIZE 5 // Max number of foods being digested

/*******************************************************************************
 * Enums
 ******************************************************************************/

typedef enum
{
    ACT_FEED,
    ACT_PLAY,
    ACT_DISCIPLINE,
    ACT_MEDICINE,
    ACT_SCOOP,
    ACT_QUIT,
    ACT_NUM_ACTIONS
} action_t;

typedef enum
{
    AGE_CHILD,
    AGE_TEEN,
    AGE_ADULT
} age_t;

/*******************************************************************************
 * Structs
 ******************************************************************************/

typedef struct
{
    int32_t hunger; ///< 0 hunger is perfect, positive means too hungry, negative means too full
    int32_t happy;
    int32_t discipline;
    int32_t health;
    int32_t poopCount;
    int32_t actionsTaken;
    bool isSick;
    int32_t stomach[STOMACH_SIZE];
    char name[32];
    age_t age;
    list_t evQueue;
} demon_t;

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

void resetDemon(demon_t* pd);
void takeAction(demon_t* pd, action_t act);

#endif
