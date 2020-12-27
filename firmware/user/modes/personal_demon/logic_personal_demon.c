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

#define INC_BOUND(base, inc, lbound, ubound)  \
    do{                                       \
        if (base + (inc) > (ubound)) {        \
            base = (ubound);                  \
        } else if (base + (inc) < (lbound)) { \
            base = (lbound);                  \
        } else {                              \
            base += (inc);                    \
        }                                     \
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

/*** Probabilities ***/

#define PCT_SICK_NO_EAT        50 ///< Percent chance a demon may refuse food
#define PCT_DISCIPLINE_NO_EAT  50

#define PCT_DISCIPLINE_1       50 ///< A little negative discipline may fail
#define PCT_DISCIPLINE_2       63
#define PCT_DISCIPLINE_3       75
#define PCT_DISCIPLINE_4       88 ///< A lot of negative discipline will probably fail
#define PCT_DISCIPLINE_TEEN    25 ///< Teenagers sometimes fail for no reason
#define PCT_DISCIPLINE_ADULT   13 ///< Adults too, but less common

#define PCT_MEDICINE_SUCCESS   85 ///< Percent chance medicine works

#define PCT_RANDOM_SICK_KID     7 ///< Percent chance a demon gets sick randomly
#define PCT_RANDOM_SICK_TEEN    8
#define PCT_RANDOM_SICK_ADULT  10 ///< Adults are naturally sicker

#define PCT_POOP_SICK_1        25 ///< Percent change poop makes a demon sick
#define PCT_POOP_SICK_2        50
#define PCT_POOP_SICK_3        75
#define PCT_POOP_SICK_4       100 ///< Four poops is the plague

#define PCT_SICK_OBESITY       63 ///< Percent chance a status makes the demon sick
#define PCT_SICK_MALNOURISHED  63

#define PCT_LOST_DISCIPLINE_P  13 ///< Percent chance a demon loses discipline, based on happiness
#define PCT_LOST_DISCIPLINE_0  25
#define PCT_LOST_DISCIPLINE_1  50
#define PCT_LOST_DISCIPLINE_2  75
#define PCT_LOST_DISCIPLINE_3 100 ///< -3 happiness always loses discipline

#define DIGEST_MIN              3 ///< Minimum number of cycles to digest food
#define DIGEST_MAX              7 ///< Maximum number of cycles to digest food

/*** Stat bounds ***/

#define MIN_HUNGER       INT32_MIN ///< Plus means full. See HUNGER_LOST_PER_FEEDING
#define MAX_HUNGER       INT32_MAX ///< Minus means hungry

#define MIN_HAPPY        INT32_MIN ///< Plus means happy. See HAPPINESS_GAINED_PER_GAME
#define MAX_HAPPY        INT32_MAX ///< Minus means sad

#define MIN_DISCIPLINE   INT32_MIN ///< Plus means good boi. See DISCIPLINE_GAINED_PER_SCOLDING
#define MAX_DISCIPLINE   INT32_MAX ///< Minus means rowdy. See DISCIPLINE_LOST_RANDOMLY

#define MIN_POOP                 0 ///< Poop count, self explanatory
#define MAX_POOP         INT32_MAX

#define MIN_HEALTH               0 ///< Health bounds, mostly for drawing hearts
#define MAX_HEALTH STARTING_HEALTH

#define MIN_ACTIONS              0 ///< Action bounds, to prevent overflows
#define MAX_ACTIONS      INT32_MAX

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
    INC_BOUND(pd->actionsTaken, 1, MIN_ACTIONS, MAX_ACTIONS);

    // If the demon is sick, there's a 50% chance it refuses to eat
    if (pd->isSick && ((os_random() % 100) < PCT_SICK_NO_EAT))
    {
        animateEvent(EVT_NO_EAT_SICK);
        // Get a bit hungrier
        pd->hungerLast = pd->hunger;
        INC_BOUND(pd->hunger, HUNGER_GAINED_PER_MEDICINE, MIN_HUNGER, MAX_HUNGER);
    }
    // If the demon has no discipline, it may refuse to eat
    else if (disciplineCheck(pd))
    {
        if(((os_random() % 100) < PCT_DISCIPLINE_NO_EAT))
        {
            animateEvent(EVT_NO_EAT_DISCIPLINE);
            // Get a bit hungrier
            pd->hungerLast = pd->hunger;
            INC_BOUND(pd->hunger, HUNGER_GAINED_PER_MEDICINE, MIN_HUNGER, MAX_HUNGER);
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
                INC_BOUND(pd->happy, HAPPINESS_GAINED_PER_FEEDING_WHEN_HUNGRY, MIN_HAPPY, MAX_HAPPY);
            }
            else if(pd->hunger < 0)
            {
                INC_BOUND(pd->happy, -HAPPINESS_LOST_PER_FEEDING_WHEN_FULL, MIN_HAPPY, MAX_HAPPY);
            }

            // Give the food between 4 and 7 cycles to digest
            pd->stomach[i] = DIGEST_MIN + (os_random() % (DIGEST_MAX - DIGEST_MIN));

            // Feeding always makes the demon less hungry
            pd->hungerLast = pd->hunger;
            INC_BOUND(pd->hunger, -HUNGER_LOST_PER_FEEDING, MIN_HUNGER, MAX_HUNGER);
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
    INC_BOUND(pd->actionsTaken, 1, MIN_ACTIONS, MAX_ACTIONS);

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
                INC_BOUND(pd->happy, HAPPINESS_GAINED_PER_GAME, MIN_HAPPY, MAX_HAPPY);
                break;
            }
            case AGE_ADULT:
            {
                // Adults don't get as happy per play as kids
                INC_BOUND(pd->happy, HAPPINESS_GAINED_PER_GAME / 2, MIN_HAPPY, MAX_HAPPY);
                break;
            }
        }

        animateEvent(EVT_PLAY);
    }

    // Playing makes the demon hungry
    pd->hungerLast = pd->hunger;
    INC_BOUND(pd->hunger, HUNGER_GAINED_PER_PLAY, MIN_HUNGER, MAX_HUNGER);
}

/**
 * Scold the demon
 *
 * @param pd The demon
 */
void ICACHE_FLASH_ATTR scoldDemon(demon_t* pd)
{
    // Count scolding as an action
    INC_BOUND(pd->actionsTaken, 1, MIN_ACTIONS, MAX_ACTIONS);

    // scolding always reduces happiness
    INC_BOUND(pd->happy, -HAPPINESS_LOST_PER_SCOLDING, MIN_HAPPY, MAX_HAPPY);

    // Discipline only increases if the demon is not sick
    if (false == pd->isSick)
    {
        INC_BOUND(pd->discipline, DISCIPLINE_GAINED_PER_SCOLDING, MIN_DISCIPLINE, MAX_DISCIPLINE);
        animateEvent(EVT_SCOLD);
    }
    else
    {
        animateEvent(EVT_NO_SCOLD_SICK);
    }

    // Disciplining makes the demon hungry
    pd->hungerLast = pd->hunger;
    INC_BOUND(pd->hunger, HUNGER_GAINED_PER_SCOLD, MIN_HUNGER, MAX_HUNGER);
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
                return (os_random() % 100) < PCT_DISCIPLINE_1;
            }
            case -2:
            {
                return (os_random() % 100) < PCT_DISCIPLINE_2;
            }
            case -3:
            {
                return (os_random() % 100) < PCT_DISCIPLINE_3;
            }
            default:
            {
                return (os_random() % 100) < PCT_DISCIPLINE_4;
            }
        }
    }
    else if(AGE_TEEN == pd->age)
    {
        return (os_random() % 100) < PCT_DISCIPLINE_TEEN;
    }
    else if(AGE_ADULT == pd->age)
    {
        return (os_random() % 100) < PCT_DISCIPLINE_ADULT;
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
    INC_BOUND(pd->actionsTaken, 1, MIN_ACTIONS, MAX_ACTIONS);

    if(!pd->isSick)
    {
        animateEvent(EVT_MEDICINE_NOT_SICK);
    }
    // 85.16% chance the demon is healed
    else if ((os_random() % 100) < PCT_MEDICINE_SUCCESS)
    {
        animateEvent(EVT_MEDICINE_CURE);
        pd->isSick = false;
    }
    else
    {
        animateEvent(EVT_MEDICINE_FAIL);
    }

    // Giving medicine to the demon makes the demon sad
    INC_BOUND(pd->happy, -HAPPINESS_LOST_PER_MEDICINE, MIN_HAPPY, MAX_HAPPY);

    // Giving medicine to the demon makes the demon hungry
    pd->hungerLast = pd->hunger;
    INC_BOUND(pd->hunger, HUNGER_GAINED_PER_MEDICINE, MIN_HUNGER, MAX_HUNGER);
}

/**
 * Flush one poop
 *
 * @param pd The demon
 */
void ICACHE_FLASH_ATTR flushPoop(demon_t* pd)
{
    // Flushing counts as an action
    INC_BOUND(pd->actionsTaken, 1, MIN_ACTIONS, MAX_ACTIONS);

    // Clear a poop
    if (pd->poopCount > 0)
    {
        animateEvent(EVT_FLUSH_POOP);
        INC_BOUND(pd->poopCount, -1, MIN_POOP, MAX_POOP);
    }
    else
    {
        animateEvent(EVT_FLUSH_NOTHING);
    }

    // Flushing makes the demon hungry
    pd->hungerLast = pd->hunger;
    INC_BOUND(pd->hunger, HUNGER_GAINED_PER_FLUSH, MIN_HUNGER, MAX_HUNGER);
}

/**
 * @brief Start spinning the wheel of fortune
 *
 * @param pd The demon
 */
void ICACHE_FLASH_ATTR spinWheel(demon_t* pd)
{
    // Spinning counts as an action
    INC_BOUND(pd->actionsTaken, 1, MIN_ACTIONS, MAX_ACTIONS);

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
            // The magic chalice always pulls hunger towards 0
            if(pd->hunger > 0)
            {
                // If the demon is hungry, feed it
                pd->hungerLast = pd->hunger;
                INC_BOUND(pd->hunger, -(2 * HUNGER_LOST_PER_FEEDING), 0, MAX_HUNGER);
            }
            else if(pd->hunger < 0)
            {
                // If the demon is full, make it less hungry
                pd->hungerLast = pd->hunger;
                INC_BOUND(pd->hunger, 2 * HUNGER_LOST_PER_FEEDING, MIN_HUNGER, 0);
            }
            // Also heals sickness
            pd->isSick = false;
            break;
        }
        case ACT_WHEEL_DAGGER:
        {
            // Lose some discipline and happiness
            INC_BOUND(pd->discipline, -DISCIPLINE_LOST_RANDOMLY, MIN_DISCIPLINE, MAX_DISCIPLINE);
            INC_BOUND(pd->happy, -HAPPINESS_LOST_PER_FEEDING_WHEN_FULL, MIN_HAPPY, MAX_HAPPY);
            break;
        }
        case ACT_WHEEL_HEART:
        {
            // Gain a heart
            INC_BOUND(pd->health, STARTING_HEALTH / 4, MIN_HEALTH, MAX_HEALTH);
            break;
        }
        case ACT_WHEEL_SKULL:
        {
            // Lose a half a heart
            INC_BOUND(pd->health, -STARTING_HEALTH / 8, MIN_HEALTH, MAX_HEALTH);
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
        INC_BOUND(pd->health, -HEALTH_LOST_PER_SICKNESS, MIN_HEALTH, MAX_HEALTH);
        animateEvent(EVT_LOST_HEALTH_SICK);
    }

    // The demon randomly gets sick
    switch (pd->age)
    {
        default:
        case AGE_CHILD:
        {
            // Kids are the healthiest
            if ((os_random() % 100) < PCT_RANDOM_SICK_KID)
            {
                enqueueEvt(pd, EVT_GOT_SICK_RANDOMLY);
            }
            break;
        }
        case AGE_TEEN:
        {
            // Teens are a little sicker
            if ((os_random() % 100) < PCT_RANDOM_SICK_TEEN)
            {
                enqueueEvt(pd, EVT_GOT_SICK_RANDOMLY);
            }
            break;
        }
        case AGE_ADULT:
        {
            // Adults are the sickest
            if ((os_random() % 100) < PCT_RANDOM_SICK_ADULT)
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
    if(pd->poopCount > 0)
    {
        switch(pd->poopCount)
        {
            case 1:
            {
                if((os_random() % 100) < PCT_POOP_SICK_1)
                {
                    enqueueEvt(pd, EVT_GOT_SICK_POOP);
                }
                break;
            }
            case 2:
            {
                if((os_random() % 100) < PCT_POOP_SICK_2)
                {
                    enqueueEvt(pd, EVT_GOT_SICK_POOP);
                }
                break;
            }
            case 3:
            {
                if((os_random() % 100) < PCT_POOP_SICK_3)
                {
                    enqueueEvt(pd, EVT_GOT_SICK_POOP);
                }
                break;
            }
            default:
            {
                if((os_random() % 100) < PCT_POOP_SICK_4)
                {
                    enqueueEvt(pd, EVT_GOT_SICK_POOP);
                }
                break;
            }
        }
    }

    // Being around poop makes the demon sad
    if (pd->poopCount > 0)
    {
        INC_BOUND(pd->happy, -HAPPINESS_LOST_PER_STANDING_POOP, MIN_HAPPY, MAX_HAPPY);
    }

    /***************************************************************************
     * Hunger Status
     **************************************************************************/

    // If the demon is too full (obese))
    if (pd->hunger < OBESE_THRESHOLD)
    {
        // 5/8 chance the demon becomes sick
        if((os_random() % 100) < PCT_SICK_OBESITY)
        {
            enqueueEvt(pd, EVT_GOT_SICK_OBESE);
        }

        if(pd->hungerLast < OBESE_THRESHOLD)
        {
            // decrease the health
            INC_BOUND(pd->health, -HEALTH_LOST_PER_OBE_MAL, MIN_HEALTH, MAX_HEALTH);
            animateEvent(EVT_LOST_HEALTH_OBESITY);
        }
    }
    else if (pd->hunger > MALNOURISHED_THRESHOLD)
    {
        // 5/8 chance the demon becomes sick
        if((os_random() % 100) < PCT_SICK_MALNOURISHED)
        {
            enqueueEvt(pd, EVT_GOT_SICK_MALNOURISHED);
        }

        if(pd->hungerLast > MALNOURISHED_THRESHOLD)
        {
            // decrease the health
            INC_BOUND(pd->health, -HEALTH_LOST_PER_OBE_MAL, MIN_HEALTH, MAX_HEALTH);
            animateEvent(EVT_LOST_HEALTH_MALNOURISHMENT);
        }
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
        if (pd->happy > 0)
        {
            if((os_random() % 100) < PCT_LOST_DISCIPLINE_P)
            {
                enqueueEvt(pd, EVT_LOST_DISCIPLINE);
            }
        }
        else if (pd->happy <= 0)
        {
            switch(pd->happy)
            {
                case 0:
                {
                    if((os_random() % 100) < PCT_LOST_DISCIPLINE_0)
                    {
                        enqueueEvt(pd, EVT_LOST_DISCIPLINE);
                    }
                    break;
                }
                case -1:
                {
                    if((os_random() % 100) < PCT_LOST_DISCIPLINE_1)
                    {
                        enqueueEvt(pd, EVT_LOST_DISCIPLINE);
                    }
                    break;
                }
                case -2:
                {
                    if((os_random() % 100) < PCT_LOST_DISCIPLINE_2)
                    {
                        enqueueEvt(pd, EVT_LOST_DISCIPLINE);
                    }
                    break;
                }
                default:
                {
                    if((os_random() % 100) < PCT_LOST_DISCIPLINE_3)
                    {
                        enqueueEvt(pd, EVT_LOST_DISCIPLINE);
                    }
                    break;
                }
            }
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
        case EVT_WHEEL_CHALICE:
        case EVT_WHEEL_DAGGER:
        case EVT_WHEEL_HEART:
        case EVT_WHEEL_SKULL:
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
                    INC_BOUND(pd->discipline, (3 * -DISCIPLINE_LOST_RANDOMLY) / 2, MIN_DISCIPLINE, MAX_DISCIPLINE);
                    break;
                }
                case AGE_ADULT:
                {
                    // Adults calm down a bit
                    INC_BOUND(pd->discipline, -DISCIPLINE_LOST_RANDOMLY, MIN_DISCIPLINE, MAX_DISCIPLINE);
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
