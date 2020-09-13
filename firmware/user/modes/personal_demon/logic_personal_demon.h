#ifndef _LOGIC_PERSONAL_DEMON_H_
#define _LOGIC_PERSONAL_DEMON_H_

#include "linked_list.h"

/*******************************************************************************
 * Defines
 ******************************************************************************/

#define STOMACH_SIZE 5 // Max number of foods being digested

/*******************************************************************************
 * Enums
 ******************************************************************************/

typedef enum
{
    AGE_CHILD,
    AGE_TEEN,
    AGE_ADULT
} age_t;

typedef enum
{
    ACT_FEED,
    ACT_PLAY,
    ACT_DISCIPLINE,
    ACT_MEDICINE,
    ACT_FLUSH,
    ACT_QUIT,
    ACT_NUM_ACTIONS
} action_t;

typedef enum
{
    EVT_NONE,
    // Queued events (only one happens once per action)
    EVT_GOT_SICK_RANDOMLY,
    EVT_GOT_SICK_POOP,
    EVT_GOT_SICK_OBESE,
    EVT_GOT_SICK_MALNOURISHED,
    EVT_POOPED,
    EVT_LOST_DISCIPLINE,
    // Immediate eating events
    EVT_EAT,
    EVT_OVEREAT,
    EVT_NO_EAT_SICK,
    EVT_NO_EAT_DISCIPLINE,
    EVT_NO_EAT_FULL,
    // Immediate playing events
    EVT_PLAY,
    EVT_NO_PLAY_DISCIPLINE,
    // Immediate scolding events
    EVT_SCOLD,
    EVT_NO_SCOLD_SICK,
    // Immediate medicine events
    EVT_MEDICINE_NOT_SICK,
    EVT_MEDICINE_CURE,
    EVT_MEDICINE_FAIL,
    // Immediate flush events
    EVT_FLUSH_POOP,
    EVT_FLUSH_NOTHING,
    // Immediate general events
    EVT_LOST_HEALTH_SICK,
    EVT_LOST_HEALTH_OBESITY,
    EVT_LOST_HEALTH_MALNOURISHMENT,
    EVT_TEENAGER,
    EVT_ADULT,
    EVT_BORN,
    EVT_DEAD,
    // Always last
    EVT_NUM_EVENTS
} event_t;

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
    event_t evQueue[32];
    uint8_t evQueueIdx;
} demon_t;

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

void resetDemon(demon_t* pd);
bool takeAction(demon_t* pd, action_t act);

bool ICACHE_FLASH_ATTR isDemonObese(demon_t* pd);
bool ICACHE_FLASH_ATTR isDemonThin(demon_t* pd);

#endif
