/*------------------------------------------------------------------------------
 * Includes
 *----------------------------------------------------------------------------*/

#include "synced_timer.h"
#include "linked_list.h"

// #define debugTmr(t) os_printf("%s::%d -- %p: armed %s, repeat %s, src %d\n", __func__, __LINE__, t, t->isArmed?"true":"false", t->isRepeat?"true":"false", t->shouldRunCnt)
#define debugTmr(t)

/*------------------------------------------------------------------------------
 * Function Prototypes
 *----------------------------------------------------------------------------*/

static void ICACHE_FLASH_ATTR incShouldRun(void* arg);

/*------------------------------------------------------------------------------
 * Variables
 *----------------------------------------------------------------------------*/

static list_t syncedTimerList = {0};

/*------------------------------------------------------------------------------
 * Functions
 *----------------------------------------------------------------------------*/

/**
 * Internal function which is used as the actual timer function for os_timer.
 * All this does is is increment a count to run the actual timer function, which
 * is done in syncedTimersCheck().
 *
 * os_timers should be quick, so this allows us to register slow functions to
 * timers
 *
 * @param arg A pointer to the syncedTimer
 */
static void ICACHE_FLASH_ATTR incShouldRun(void* arg)
{
    debugTmr(((syncedTimer_t*)arg));
    ((syncedTimer_t*)arg)->shouldRunCnt++;
}

/**
 * Set timer callback function. The timer callback function must be set before
 * arming a timer.
 *
 * This is a wrapper for os_timer_setfn().
 *
 * @param timer     The timer struct
 * @param timerFunc The timer callback function
 * @param arg       An arg to pass to the timer callback function
 */
void ICACHE_FLASH_ATTR syncedTimerSetFn(syncedTimer_t* timer,
                                        os_timer_func_t* timerFunc, void* arg)
{
    // os_timer_setfn() can only be invoked when the timer is not enabled, i.e.,
    // after os_timer_disarm() or before os_timer_arm(), so call os_timer_disarm
    os_timer_disarm(&(timer->osTimer));

    // Save the function parameters
    timer->timerFunc = timerFunc;
    timer->arg = arg;

    // Zero out the variables
    timer->shouldRunCnt = 0;
    timer->isArmed = false;
    timer->isRepeat = false;
    ets_memset(&(timer->osTimer), 0, sizeof(os_timer_t));

    // Set the timer function
    os_timer_setfn(&(timer->osTimer), incShouldRun, timer);
}

/**
 * Arm a timer callback function to be called at some time or interval.
 * This is a wrapper for os_timer_arm();
 *
 * @param timer       The timer struct
 * @param time        The number of milliseconds to call this timer in
 * @param repeat_flag true to have this timer repeat, false to have it run once
 */
void ICACHE_FLASH_ATTR syncedTimerArm(syncedTimer_t* timer, uint32_t time,
                                      bool repeat_flag)
{
    // For the same timer, os_timer_arm() cannot be invoked repeatedly.
    // os_timer_disarm() should be invoked first, so call os_timer_disarm()
    os_timer_disarm(&(timer->osTimer));

    // Then arm it with the parameters
    os_timer_arm(&(timer->osTimer), time, repeat_flag);
    timer->isArmed = true;
    timer->isRepeat = repeat_flag;

    debugTmr(timer);

    // Check to see if this timer is in the list already
    node_t* currentNode = syncedTimerList.first;
    while (currentNode != NULL)
    {
        if(currentNode->val == timer)
        {
            // Already in the list, so return
            return;
        }
        currentNode = currentNode->next;
    }

    // If it isn't in the list, push it into the list
    push(&syncedTimerList, (void*)timer);
}

/**
 * Disarm a timer.
 *
 * This is a wrapper for os_timer_disarm()
 *
 * @param timer The timer struct
 */
void ICACHE_FLASH_ATTR syncedTimerDisarm(syncedTimer_t* timer)
{
    // Disarm the timer
    os_timer_disarm(&(timer->osTimer));
    timer->isArmed = false;
    timer->isRepeat = false;
    // And make sure the function isn't called again
    timer->shouldRunCnt = 0;
    // This will get removed from the list in syncedTimersCheck()
    debugTmr(timer);
}

/**
 * Check each synced timer and call it's respective function as many times as
 * necessary.
 *
 * This must be called from procTask(), where functions can run for long times
 * without disrupting the system.
 */
void ICACHE_FLASH_ATTR syncedTimersCheck(void)
{
    // For each timer, call the function if it's time
    node_t* currentNode = syncedTimerList.first;
    while (currentNode != NULL)
    {
        syncedTimer_t* timer = (syncedTimer_t*) currentNode->val;

        // For as many times as the function should be called
        while(0 < timer->shouldRunCnt)
        {
            debugTmr(timer);
            // Call the function if it's still armed
            if(true == timer->isArmed)
            {
                // If this is not a repeating timer, mark it disarmed
                if(false == timer->isRepeat)
                {
                    timer->isArmed = false;
                }
                // Then call the timer function, this may rearm the timer
                timer->timerFunc(timer->arg);
                debugTmr(timer);
            }

            // Decrement, making sure not to underflow
            if(0 < timer->shouldRunCnt)
            {
                timer->shouldRunCnt--;
            }
        }

        // Iterate to the next timer
        currentNode = currentNode->next;
    }

    // After the functions have been called, remove disarmed timers from the list
    currentNode = syncedTimerList.first;
    while (currentNode != NULL)
    {
        // Save the next node because currentNode may be freed below
        node_t* next = currentNode->next;

        // If the timer isn't armed anymore
        if(false == (((syncedTimer_t*) currentNode->val))->isArmed)
        {
            // Remove it from the list
            removeEntry(&syncedTimerList, currentNode);
        }

        // Iterate!
        currentNode = next;
    }
}
