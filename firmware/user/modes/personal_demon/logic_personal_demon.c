/*******************************************************************************
 * Includes
 ******************************************************************************/

#include <osapi.h>
#include <stdint.h>

#include "logic_personal_demon.h"
#include "demon-namegen.h"
#include "mode_personal_demon.h"

/*******************************************************************************
 * Defines
 ******************************************************************************/

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

// TODO once a demon loses discipline, its hard to get it back on track, cascading effect. (no discipline)->refuse stuff->unhappy->(no discipline)
#define DISCIPLINE_GAINED_PER_SCOLDING 4 ///< Scolding increases discipline
#define DISCIPLINE_LOST_RANDOMLY       2 ///< Discipline is randomly lost

#define HEALTH_LOST_PER_SICKNESS  1 ///< Health is lost every turn while sick
#define HEALTH_LOST_PER_OBE_MAL   2 ///< Health is lost every turn while obese or malnourished

#define ACTIONS_UNTIL_TEEN  33
#define ACTIONS_UNTIL_ADULT 66

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

bool eatFood(demon_t* pd);
void feedDemon(demon_t* pd);
void playWithDemon(demon_t* pd);
void scoldDemon(demon_t* pd);
bool disciplineCheck(demon_t* pd);
void medicineDemon(demon_t* pd);
void flushPoop(demon_t* pd);
void spinWheel(demon_t* pd);
void wheelResult(demon_t* pd, action_t result);
void updateStatus(demon_t* pd);

event_t dequeueEvt(demon_t* pd);
bool enqueueEvt(demon_t* pd, event_t evt);

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
        animateEvent(EVT_NO_EAT_SICK);
        // Get a bit hungrier
        INC_BOUND(pd->hunger, HUNGER_GAINED_PER_MEDICINE,  INT32_MIN, INT32_MAX);
    }
    // If the demon has no discipline, it may refuse to eat
    else if (disciplineCheck(pd))
    {
        if(os_random() % 2 == 0)
        {
            animateEvent(EVT_NO_EAT_DISCIPLINE);
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
            animateEvent(EVT_OVEREAT);
        }
    }
    // Normal feeding
    else
    {
        // Normal feeding is successful
        if(eatFood(pd))
        {
            animateEvent(EVT_EAT);
        }
        else
        {
            animateEvent(EVT_NO_EAT_FULL);
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
            else if(pd->hunger < 0)
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
        animateEvent(EVT_NO_PLAY_DISCIPLINE);
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

        animateEvent(EVT_PLAY);
    }

    // Playing makes the demon hungry
    INC_BOUND(pd->hunger, HUNGER_GAINED_PER_PLAY,  INT32_MIN, INT32_MAX);
}

/**
 * Scold the demon
 *
 * @param pd The demon
 */
void ICACHE_FLASH_ATTR scoldDemon(demon_t* pd)
{
    // Count scolding as an action
    INC_BOUND(pd->actionsTaken, 1, 0, INT16_MAX);

    // scolding always reduces happiness
    INC_BOUND(pd->happy, -HAPPINESS_LOST_PER_SCOLDING,  INT32_MIN, INT32_MAX);

    // Discipline only increases if the demon is not sick
    if (false == pd->isSick)
    {
        INC_BOUND(pd->discipline, DISCIPLINE_GAINED_PER_SCOLDING,  INT32_MIN, INT32_MAX);
        animateEvent(EVT_SCOLD);
    }
    else
    {
        animateEvent(EVT_NO_SCOLD_SICK);
    }

    // Disciplining makes the demon hungry
    INC_BOUND(pd->hunger, HUNGER_GAINED_PER_SCOLD,  INT32_MIN, INT32_MAX);
}

/**
 * @brief
 *
 * @return true if the demon has no discipline (won't take action)
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

    if(!pd->isSick)
    {
        animateEvent(EVT_MEDICINE_NOT_SICK);
    }
    // 6/8 chance the demon is healed
    else if (os_random() % 8 < 6)
    {
        animateEvent(EVT_MEDICINE_CURE);
        pd->isSick = false;
    }
    else
    {
        animateEvent(EVT_MEDICINE_FAIL);
    }

    // Giving medicine to the demon makes the demon sad
    INC_BOUND(pd->happy, -HAPPINESS_LOST_PER_MEDICINE,  INT32_MIN, INT32_MAX);

    // Giving medicine to the demon makes the demon hungry
    INC_BOUND(pd->hunger, HUNGER_GAINED_PER_MEDICINE,  INT32_MIN, INT32_MAX);
}

/**
 * Flush one poop
 *
 * @param pd The demon
 */
void ICACHE_FLASH_ATTR flushPoop(demon_t* pd)
{
    // Flushing counts as an action
    INC_BOUND(pd->actionsTaken, 1, 0, INT16_MAX);

    // Clear a poop
    if (pd->poopCount > 0)
    {
        animateEvent(EVT_FLUSH_POOP);
        INC_BOUND(pd->poopCount, -1,  INT32_MIN, INT32_MAX);
    }
    else
    {
        animateEvent(EVT_FLUSH_NOTHING);
    }

    // Flushing makes the demon hungry
    INC_BOUND(pd->hunger, HUNGER_GAINED_PER_FLUSH,  INT32_MIN, INT32_MAX);
}

/**
 * @brief Start spinning the wheel of fortune
 *
 * @param pd The demon
 */
void ICACHE_FLASH_ATTR spinWheel(demon_t* pd)
{
    // Spinning counts as an action
    INC_BOUND(pd->actionsTaken, 1, 0, INT16_MAX);

    // Start the wheel spinning, random event is received later
    animateEvent(EVT_SPIN_WHEEL);
}

/**
 * @brief Process the result of a wheel spin
 *
 * @param pd The demon
 * @param result The result of the wheel spin
 */
void ICACHE_FLASH_ATTR wheelResult(demon_t* pd, action_t result)
{
    switch (result)
    {
        case ACT_WHEEL_CHALICE:
        {
            break;
        }
        case ACT_WHEEL_DAGGER:
        {
            break;
        }
        case ACT_WHEEL_HEART:
        {
            break;
        }
        case ACT_WHEEL_SKULL:
        {
            break;
        }
        case ACT_FEED:
        case ACT_PLAY:
        case ACT_DISCIPLINE:
        case ACT_MEDICINE:
        case ACT_FLUSH:
        case ACT_WHEEL_OF_FORTUNE:
        case ACT_QUIT:
        case ACT_NUM_ACTIONS:
        default:
        {
            // Not actual wheel results
            break;
        }
    }
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
        animateEvent(EVT_LOST_HEALTH_SICK);
    }

    // The demon randomly gets sick
    switch (pd->age)
    {
        default:
        case AGE_CHILD:
        {
            // Kids are the healthiest
            if (os_random() % 14 == 0)
            {
                enqueueEvt(pd, EVT_GOT_SICK_RANDOMLY);
            }
            break;
        }
        case AGE_TEEN:
        {
            // Teens are a little sicker
            if (os_random() % 12 == 0)
            {
                enqueueEvt(pd, EVT_GOT_SICK_RANDOMLY);
            }
            break;
        }
        case AGE_ADULT:
        {
            // Adults are the sickest
            if (os_random() % 10 == 0)
            {
                enqueueEvt(pd, EVT_GOT_SICK_RANDOMLY);
            }
            break;
        }
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

        animateEvent(EVT_LOST_HEALTH_OBESITY);
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
        animateEvent(EVT_LOST_HEALTH_MALNOURISHMENT);
    }

    /***************************************************************************
     * Discipline Status
     **************************************************************************/

    // Children don't lose discipline, that starts with teens
    // See where EVT_LOST_DISCIPLINE gets dequeued
    if(pd->age != AGE_CHILD)
    {
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
    }

    /***************************************************************************
     * Age status
     **************************************************************************/

    if(pd->age == AGE_CHILD && pd->actionsTaken >= ACTIONS_UNTIL_TEEN)
    {
        animateEvent(EVT_TEENAGER);
        pd->age = AGE_TEEN;
    }
    else if(pd->age == AGE_TEEN && pd->actionsTaken >= ACTIONS_UNTIL_ADULT)
    {
        animateEvent(EVT_ADULT);
        pd->age = AGE_ADULT;
    }

    /***************************************************************************
     * Process one event per call
     **************************************************************************/

    event_t singleEvt = dequeueEvt(pd);
    animateEvent(singleEvt);
    switch(singleEvt)
    {
        default:
        case EVT_NONE:
        case EVT_NUM_EVENTS:
        case EVT_EAT:
        case EVT_OVEREAT:
        case EVT_NO_EAT_SICK:
        case EVT_NO_EAT_DISCIPLINE:
        case EVT_NO_EAT_FULL:
        case EVT_PLAY:
        case EVT_NO_PLAY_DISCIPLINE:
        case EVT_SCOLD:
        case EVT_NO_SCOLD_SICK:
        case EVT_MEDICINE_NOT_SICK:
        case EVT_MEDICINE_CURE:
        case EVT_MEDICINE_FAIL:
        case EVT_FLUSH_POOP:
        case EVT_FLUSH_NOTHING:
        case EVT_SPIN_WHEEL:
        case EVT_LOST_HEALTH_SICK:
        case EVT_LOST_HEALTH_OBESITY:
        case EVT_LOST_HEALTH_MALNOURISHMENT:
        case EVT_TEENAGER:
        case EVT_ADULT:
        case EVT_BORN:
        case EVT_DEAD:
        {
            // Nothing
            break;
        }
        case EVT_GOT_SICK_RANDOMLY:
        {
            if(false == pd->isSick)
            {
                pd->isSick = true;
            }
            break;
        }
        case EVT_GOT_SICK_POOP:
        {
            if(false == pd->isSick)
            {
                pd->isSick = true;
            }
            break;
        }
        case EVT_GOT_SICK_OBESE:
        {
            if(false == pd->isSick)
            {
                pd->isSick = true;
            }
            break;
        }
        case EVT_GOT_SICK_MALNOURISHED:
        {
            if(false == pd->isSick)
            {
                pd->isSick = true;
            }
            break;
        }
        case EVT_POOPED:
        {
            // Make a poop
            pd->poopCount++;
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
                    // Rebellious teenage years lose 1.5x discipline
                    INC_BOUND(pd->discipline, (3 * -DISCIPLINE_LOST_RANDOMLY) / 2,  INT32_MIN, INT32_MAX);
                    break;
                }
                case AGE_ADULT:
                {
                    // Adults calm down a bit
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
        animateEvent(EVT_DEAD);
        // Empty and free the event queue
        while(EVT_NONE != dequeueEvt(pd)) {;}
    }
}

/**
 * Print a menu of options, then wait for user input and perform one of the
 * actions
 *
 * @param pd
 * @return true if the mode quit, false if it did not
 */
bool ICACHE_FLASH_ATTR takeAction(demon_t* pd, action_t action)
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
            scoldDemon(pd);
            break;
        }
        case ACT_MEDICINE:
        {
            medicineDemon(pd);
            break;
        }
        case ACT_FLUSH:
        {
            flushPoop(pd);
            break;
        }
        case ACT_WHEEL_OF_FORTUNE:
        {
            spinWheel(pd);
            break;
        }
        case ACT_WHEEL_CHALICE:
        case ACT_WHEEL_DAGGER:
        case ACT_WHEEL_HEART:
        case ACT_WHEEL_SKULL:
        {
            wheelResult(pd, action);
            break;
        }
        case ACT_QUIT:
        {
            switchToSwadgeMode(0);
            return true; // pd will be uninitialized after this
        }
        case ACT_NUM_ACTIONS:
        default:
        {
            return false;
        }
    }
    updateStatus(pd);
    return false;
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
    pd->species = (os_random() % getNumDemonSpecies());

    animateEvent(EVT_BORN);
}

/**
 * @brief Dequeue an event
 *
 * @param pd
 * @param evt
 */
bool ICACHE_FLASH_ATTR enqueueEvt(demon_t* pd, event_t evt)
{
    uint8_t arrLen = sizeof(pd->evQueue) / sizeof(pd->evQueue[0]);
    for(uint8_t i = 0; i < arrLen; i++)
    {
        if(EVT_NONE == pd->evQueue[(pd->evQueueIdx + i) % arrLen])
        {
            pd->evQueue[(pd->evQueueIdx + i) % arrLen] = evt;
            return true;
        }
    }
    return false;
}

/**
 * @brief Enqueue an event
 *
 * @param pd
 * @return event_t
 */
event_t ICACHE_FLASH_ATTR dequeueEvt(demon_t* pd)
{
    event_t evt = pd->evQueue[pd->evQueueIdx];
    pd->evQueue[pd->evQueueIdx] = EVT_NONE;
    pd->evQueueIdx = (pd->evQueueIdx + 1) % (sizeof(pd->evQueue) / sizeof(pd->evQueue[0]));
    return evt;
}

/**
 * @brief TODO
 *
 * @param pd
 * @return true
 * @return false
 */
bool ICACHE_FLASH_ATTR isDemonObese(demon_t* pd)
{
    if (pd->hunger < OBESE_THRESHOLD)
    {
        return true;
    }
    return false;
}

/**
 * @brief TODO
 *
 * @param pd
 * @return true
 * @return false
 */
bool ICACHE_FLASH_ATTR isDemonThin(demon_t* pd)
{
    if (pd->hunger > MALNOURISHED_THRESHOLD)
    {
        return true;
    }
    return false;
}
