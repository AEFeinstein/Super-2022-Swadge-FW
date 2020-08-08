/*******************************************************************************
 * Includes
 ******************************************************************************/

#include <osapi.h>
#include <stdint.h>

#include "linked_list.h"
#include "logic_personal_demon.h"
#include "demon-namegen.h"

/*******************************************************************************
 * Defines
 ******************************************************************************/

#define PRINT_F(...) os_printf(__VA_ARGS__) // do{if(!autoMode){printf(__VA_ARGS__);}}while(false)

#define INC_BOUND(base, inc, lbound, ubound) \
    do{                                      \
        if (base + inc > ubound) {           \
            base = ubound;                   \
        } else if (base + inc < lbound) {    \
            base = lbound;                   \
        } else {                             \
            base += inc;                     \
        }                                    \
    } while(false)

// Every action modifies hunger somehow
#define HUNGER_LOST_PER_FEEDING    5 ///< Hunger is lost when feeding
#define HUNGER_GAINED_PER_PLAY     3 ///< Hunger is gained when playing
#define HUNGER_GAINED_PER_SCOLD    1 ///< Hunger is gained when being scolded
#define HUNGER_GAINED_PER_MEDICINE 1 ///< Hunger is gained when taking medicine
#define HUNGER_GAINED_PER_FLUSH    1 ///< Hunger is gained when flushing

#define OBESE_THRESHOLD        -6 ///< too fat (i.e. not hungry)
#define MALNOURISHED_THRESHOLD  6 ///< too skinny (i.e. hungry)

#define HAPPINESS_GAINED_PER_GAME                4 ///< Playing games increases happiness
#define HAPPINESS_GAINED_PER_FEEDING_WHEN_HUNGRY 1 ///< Eating when hungry increases happiness
#define HAPPINESS_LOST_PER_FEEDING_WHEN_FULL     3 ///< Eating when full decreases happiness
#define HAPPINESS_LOST_PER_MEDICINE              4 ///< Taking medicine makes decreases happiness
#define HAPPINESS_LOST_PER_STANDING_POOP         5 ///< Being around poop decreases happiness
#define HAPPINESS_LOST_PER_SCOLDING              6 ///< Scolding decreases happiness

// TODO once a demon gets unruly, its hard to get it back on track, cascading effect. unruly->refuse stuff->unhappy->unruly
#define DISCIPLINE_GAINED_PER_SCOLDING 4 ///< Scolding increases discipline
#define DISCIPLINE_LOST_RANDOMLY       2 ///< Discipline is randomly lost

#define STARTING_HEALTH          20 ///< Health is started with, cannot be increased
#define HEALTH_LOST_PER_SICKNESS  1 ///< Health is lost every turn while sick
#define HEALTH_LOST_PER_OBE_MAL   2 ///< Health is lost every turn while obese or malnourished

#define ACTIONS_UNTIL_TEEN  33
#define ACTIONS_UNTIL_ADULT 66

/*******************************************************************************
 * Enums
 ******************************************************************************/

typedef enum
{
    EVT_NONE,
    EVT_GOT_SICK_RANDOMLY,
    EVT_GOT_SICK_POOP,
    EVT_GOT_SICK_OBESE,
    EVT_GOT_SICK_MALNOURISHED,
    EVT_POOPED,
    EVT_LOST_DISCIPLINE,
    EVT_NUM_EVENTS,
} event_t;

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

bool eatFood(demon_t* pd);
void feedDemon(demon_t* pd);
void playWithDemon(demon_t* pd);
void disciplineDemon(demon_t* pd);
bool disciplineCheck(demon_t* pd);
void medicineDemon(demon_t* pd);
void scoopPoop(demon_t* pd);
void updateStatus(demon_t* pd);

event_t dequeueEvt(demon_t* pd);
void enqueueEvt(demon_t* pd, event_t evt);

/*******************************************************************************
 * Functions
 ******************************************************************************/

/**
 * Feed a demon
 * Feeding makes the demon happier if it is hungry
 *
 * @param pd The demon
 */
void ICACHE_FLASH_ATTR feedDemon(demon_t* pd)
{
    // Count feeding as an action
    INC_BOUND(pd->actionsTaken, 1, 0, INT16_MAX);

    // If the demon is sick, there's a 50% chance it refuses to eat
    if (pd->isSick && os_random() % 2)
    {
        PRINT_F("%s was too sick to eat\n", pd->name);
        // Get a bit hungrier
        INC_BOUND(pd->hunger, HUNGER_GAINED_PER_MEDICINE,  INT32_MIN, INT32_MAX);
    }
    // If the demon is unruly, it may refuse to eat
    else if (disciplineCheck(pd))
    {
        if(os_random() % 2 == 0)
        {
            PRINT_F("%s was too unruly eat\n", pd->name);
            // Get a bit hungrier
            INC_BOUND(pd->hunger, HUNGER_GAINED_PER_MEDICINE,  INT32_MIN, INT32_MAX);
        }
        else
        {
            // Eat as much as possible
            for(int i = 0; i < 3; i++)
            {
                eatFood(pd);
            }
            PRINT_F("%s ate the food, then stole more and overate\n", pd->name);
        }
    }
    // Normal feeding
    else
    {
        // Normal feeding is successful
        if(eatFood(pd))
        {
            PRINT_F("%s ate the food\n", pd->name);
        }
        else
        {
            PRINT_F("%s was too full to eat\n", pd->name);
        }
    }
}

/**
 * @brief Eat a food
 *
 * @param pd The demon to feed
 * @return true if the food was eaten, false if the demon was full
 */
bool ICACHE_FLASH_ATTR eatFood(demon_t* pd)
{
    // Make sure there's room in the stomach first
    for (int i = 0; i < STOMACH_SIZE; i++)
    {
        if (pd->stomach[i] == 0)
        {
            // If the demon eats when hungry, it gets happy, otherwise it gets sad
            if (pd->hunger > 0)
            {
                INC_BOUND(pd->happy, HAPPINESS_GAINED_PER_FEEDING_WHEN_HUNGRY,  INT32_MIN, INT32_MAX);
            }
            else
            {
                INC_BOUND(pd->happy, -HAPPINESS_LOST_PER_FEEDING_WHEN_FULL,  INT32_MIN, INT32_MAX);
            }

            // Give the food between 4 and 7 cycles to digest
            pd->stomach[i] = 3 + (os_random() % 4);

            // Feeding always makes the demon less hungry
            INC_BOUND(pd->hunger, -HUNGER_LOST_PER_FEEDING,  INT32_MIN, INT32_MAX);
            return true;
        }
    }
    return false;
}

/**
 * Play with the demon
 *
 * @param pd The demon
 */
void ICACHE_FLASH_ATTR playWithDemon(demon_t* pd)
{
    // Count playing as an action
    INC_BOUND(pd->actionsTaken, 1, 0, INT16_MAX);

    if (disciplineCheck(pd))
    {
        PRINT_F("%s was too unruly to play\n", pd->name);
    }
    else
    {
        // Playing makes the demon happy
        switch(pd->age)
        {
            default:
            case AGE_CHILD:
            case AGE_TEEN:
            {
                INC_BOUND(pd->happy, HAPPINESS_GAINED_PER_GAME,  INT32_MIN, INT32_MAX);
                break;
            }
            case AGE_ADULT:
            {
                // Adults don't get as happy per play as kids
                INC_BOUND(pd->happy, HAPPINESS_GAINED_PER_GAME / 2,  INT32_MIN, INT32_MAX);
                break;
            }
        }

        PRINT_F("You played with %s\n", pd->name);
    }

    // Playing makes the demon hungry
    INC_BOUND(pd->hunger, HUNGER_GAINED_PER_PLAY,  INT32_MIN, INT32_MAX);
}

/**
 * Scold the demon
 *
 * @param pd The demon
 */
void ICACHE_FLASH_ATTR disciplineDemon(demon_t* pd)
{
    // Count discipline as an action
    INC_BOUND(pd->actionsTaken, 1, 0, INT16_MAX);

    // Discipline always reduces happiness
    INC_BOUND(pd->happy, -HAPPINESS_LOST_PER_SCOLDING,  INT32_MIN, INT32_MAX);

    // Discipline only increases if the demon is not sick
    if (false == pd->isSick)
    {
        INC_BOUND(pd->discipline, DISCIPLINE_GAINED_PER_SCOLDING,  INT32_MIN, INT32_MAX);
        PRINT_F("You scolded %s\n", pd->name);
    }
    else
    {
        PRINT_F("You scolded %s, but it was sick\n", pd->name);
    }

    // Disciplining makes the demon hungry
    INC_BOUND(pd->hunger, HUNGER_GAINED_PER_SCOLD,  INT32_MIN, INT32_MAX);
}

/**
 * @brief
 *
 * @return true if the demon is being unruly (won't take action)
 */
bool ICACHE_FLASH_ATTR disciplineCheck(demon_t* pd)
{
    if (pd->discipline < 0)
    {
        switch (pd->discipline)
        {
            case -1:
            {
                return (os_random() % 8) < 4;
            }
            case -2:
            {
                return (os_random() % 8) < 5;
            }
            case -3:
            {
                return (os_random() % 8) < 6;
            }
            default:
            {
                return (os_random() % 8) < 7;
            }
        }
    }
    else if(AGE_TEEN == pd->age)
    {
        return (os_random() % 8) < 2;
    }
    else if(AGE_ADULT == pd->age)
    {
        return (os_random() % 8) < 1;
    }
    else
    {
        return false;
    }
}

/**
 * Give the demon medicine, works 6/8 times
 *
 * @param pd The demon
 */
void ICACHE_FLASH_ATTR medicineDemon(demon_t* pd)
{
    // Giving medicine counts as an action
    INC_BOUND(pd->actionsTaken, 1, 0, INT16_MAX);

    // 6/8 chance the demon is healed
    if (os_random() % 8 < 6)
    {
        PRINT_F("You gave %s medicine, and it was cured\n", pd->name);
        pd->isSick = false;
    }
    else
    {
        PRINT_F("You gave %s medicine, but it didn't work\n", pd->name);
    }

    // Giving medicine to the demon makes the demon hungry
    INC_BOUND(pd->happy, -HAPPINESS_LOST_PER_MEDICINE,  INT32_MIN, INT32_MAX);

    // Giving medicine to the demon makes the demon hungry
    INC_BOUND(pd->hunger, HUNGER_GAINED_PER_MEDICINE,  INT32_MIN, INT32_MAX);
}

/**
 * Flush one poop
 *
 * @param pd The demon
 */
void ICACHE_FLASH_ATTR scoopPoop(demon_t* pd)
{
    // Flushing counts as an action
    INC_BOUND(pd->actionsTaken, 1, 0, INT16_MAX);

    // Clear a poop
    if (pd->poopCount > 0)
    {
        PRINT_F("You flushed a poop\n");
        INC_BOUND(pd->poopCount, -1,  INT32_MIN, INT32_MAX);
    }
    else
    {
        PRINT_F("You flushed nothing\n");
    }

    // Flushing makes the demon hungry
    INC_BOUND(pd->hunger, HUNGER_GAINED_PER_FLUSH,  INT32_MIN, INT32_MAX);
}

/**
 * This is called after every action.
 * If there is poop, check if the demon becomes sick
 * If the demon is malnourished or obese, check if the demon becomes sick
 * If the demon is malnourished or obese, decrease health
 * If the demon is sick, decrease health (separately from obese / malnourised)
 * If food has been digested, make a poop
 * If health reaches zero, the demon is dead
 *
 * @param pd The demon
 */
void ICACHE_FLASH_ATTR updateStatus(demon_t* pd)
{
    /***************************************************************************
     * Sick Status
     **************************************************************************/

    // If the demon is sick, decrease health
    if (pd->isSick)
    {
        INC_BOUND(pd->health, -HEALTH_LOST_PER_SICKNESS,  INT32_MIN, INT32_MAX);
        PRINT_F("%s lost health to sickness\n", pd->name);
    }

    // The demon randomly gets sick
    if (os_random() % 12 == 0)
    {
        enqueueEvt(pd, EVT_GOT_SICK_RANDOMLY);
    }

    /***************************************************************************
     * Poop Status
     **************************************************************************/

    // Check if demon should poop
    for (int i = 0; i < STOMACH_SIZE; i++)
    {
        if (pd->stomach[i] > 0)
        {
            pd->stomach[i]--;
            // If the food was digested
            if (0 == pd->stomach[i])
            {
                enqueueEvt(pd, EVT_POOPED);
            }
        }
    }

    // Check if poop makes demon sick
    // 1 poop  -> 25% chance
    // 2 poop  -> 50% chance
    // 3 poop  -> 75% chance
    // 4+ poop -> 100% chance
    if ((int32_t)(os_random() % 4) > (3 - pd->poopCount))
    {
        enqueueEvt(pd, EVT_GOT_SICK_POOP);
    }

    // Being around poop makes the demon sad
    if (pd->poopCount > 0)
    {
        INC_BOUND(pd->happy, -HAPPINESS_LOST_PER_STANDING_POOP, INT32_MIN, INT32_MAX);
    }

    /***************************************************************************
     * Hunger Status
     **************************************************************************/

    // If the demon is too full (obese))
    if (pd->hunger < OBESE_THRESHOLD)
    {
        // 5/8 chance the demon becomes sick
        if ((os_random() % 8) >= 5)
        {
            enqueueEvt(pd, EVT_GOT_SICK_OBESE);
        }

        // decrease the health
        INC_BOUND(pd->health, -HEALTH_LOST_PER_OBE_MAL,  INT32_MIN, INT32_MAX);

        PRINT_F("%s lost health to obesity\n", pd->name);
    }
    else if (pd->hunger > MALNOURISHED_THRESHOLD)
    {
        // 5/8 chance the demon becomes sick
        if ((os_random() % 8) >= 5)
        {
            enqueueEvt(pd, EVT_GOT_SICK_MALNOURISHED);
        }

        // decrease the health
        INC_BOUND(pd->health, -HEALTH_LOST_PER_OBE_MAL,  INT32_MIN, INT32_MAX);
        PRINT_F("%s lost health to malnourishment\n", pd->name);
    }

    /***************************************************************************
     * Discipline Status
     **************************************************************************/

    // If unhappy, the demon might get a little less disciplined
    // pos -> 12.5%
    //  0  -> 25%
    // -1  -> 50%
    // -2  -> 75%
    // -3  -> 100%
    if (pd->happy > 0 && os_random() % 16 < 1)
    {
        enqueueEvt(pd, EVT_LOST_DISCIPLINE);
    }
    else if (pd->happy <= 0 && (int32_t)(os_random() % 4) < (1 - pd->happy))
    {
        enqueueEvt(pd, EVT_LOST_DISCIPLINE);
    }

    /***************************************************************************
     * Age status
     **************************************************************************/

    if(pd->age == AGE_CHILD && pd->actionsTaken >= ACTIONS_UNTIL_TEEN)
    {
        PRINT_F("%s is now a teenager. Watch out.\n", pd->name);
        pd->age = AGE_TEEN;
    }
    else if(pd->age == AGE_TEEN && pd->actionsTaken >= ACTIONS_UNTIL_ADULT)
    {
        PRINT_F("%s is now an adult. Boring.\n", pd->name);
        pd->age = AGE_ADULT;
    }

    /***************************************************************************
     * Process one event per call
     **************************************************************************/

    switch(dequeueEvt(pd))
    {
        default:
        case EVT_NONE:
        case EVT_NUM_EVENTS:
        {
            // Nothing
            break;
        }
        case EVT_GOT_SICK_RANDOMLY:
        {
            if(false == pd->isSick)
            {
                pd->isSick = true;
                PRINT_F("%s randomly got sick\n", pd->name);
            }
            break;
        }
        case EVT_GOT_SICK_POOP:
        {
            if(false == pd->isSick)
            {
                pd->isSick = true;
                PRINT_F("Poop made %s sick\n", pd->name);
            }
            break;
        }
        case EVT_GOT_SICK_OBESE:
        {
            if(false == pd->isSick)
            {
                pd->isSick = true;
                PRINT_F("Obesity made %s sick\n", pd->name);
            }
            break;
        }
        case EVT_GOT_SICK_MALNOURISHED:
        {
            if(false == pd->isSick)
            {
                pd->isSick = true;
                PRINT_F("Malnourishment made %s sick\n", pd->name);
            }
            break;
        }
        case EVT_POOPED:
        {
            // Make a poop
            pd->poopCount++;
            PRINT_F("%s pooped\n", pd->name);
            break;
        }
        case EVT_LOST_DISCIPLINE:
        {
            switch(pd->age)
            {
                default:
                case AGE_CHILD:
                {
                    // Kids don't lose discipline, they're good kids!
                    break;
                }
                case AGE_TEEN:
                {
                    // Rebellious teenage years lose triple discipline
                    PRINT_F("%s became less disciplined\n", pd->name);
                    INC_BOUND(pd->discipline, 3 * -DISCIPLINE_LOST_RANDOMLY,  INT32_MIN, INT32_MAX);
                    break;
                }
                case AGE_ADULT:
                {
                    // Adults calm down a bit
                    PRINT_F("%s became less disciplined\n", pd->name);
                    INC_BOUND(pd->discipline, -DISCIPLINE_LOST_RANDOMLY,  INT32_MIN, INT32_MAX);
                    break;
                }
            }
            break;
        }
    }

    /***************************************************************************
     * Health Status
     **************************************************************************/

    // Zero health means the demon died
    if (pd->health <= 0)
    {
        PRINT_F("%s died\n", pd->name);
        // Empty and free the event queue
        while(EVT_NONE != dequeueEvt(pd)) {;}
    }
}

/**
 * Print a menu of options, then wait for user input and perform one of the
 * actions
 *
 * @param pd
 * @return true
 * @return false
 */
void ICACHE_FLASH_ATTR takeAction(demon_t* pd, action_t action)
{
    switch (action)
    {
        case ACT_FEED:
        {
            feedDemon(pd);
            break;
        }
        case ACT_PLAY:
        {
            playWithDemon(pd);
            break;
        }
        case ACT_DISCIPLINE:
        {
            disciplineDemon(pd);
            break;
        }
        case ACT_MEDICINE:
        {
            medicineDemon(pd);
            break;
        }
        case ACT_SCOOP:
        {
            scoopPoop(pd);
            break;
        }
        case ACT_QUIT:
        case ACT_NUM_ACTIONS:
        default:
        {
            return;
        }
    }
    updateStatus(pd);
}

/**
 * @brief Initialize the demon
 *
 * @param pd The demon to initialize
 */
void ICACHE_FLASH_ATTR resetDemon(demon_t* pd)
{
    ets_memset(pd, 0, sizeof(demon_t));
    pd->health = STARTING_HEALTH;
    namegen(pd->name);
    pd->name[0] -= ('a' - 'A');

    PRINT_F("%s fell out of a portal\n", pd->name);
}

/**
 * @brief Dequeue an event
 *
 * @param pd
 * @param evt
 */
void ICACHE_FLASH_ATTR enqueueEvt(demon_t* pd, event_t evt)
{
    push(&(pd->evQueue), (void*)evt);
}

/**
 * @brief Enqueue an event
 *
 * @param pd
 * @return event_t
 */
event_t ICACHE_FLASH_ATTR dequeueEvt(demon_t* pd)
{
    return (event_t)shift(&(pd->evQueue));
}
