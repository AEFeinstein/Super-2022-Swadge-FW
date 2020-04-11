/*------------------------------------------------------------------------------
 * Includes
 *----------------------------------------------------------------------------*/

#include "synced_timer.h"
#include "linked_list.h"

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
    // And make sure the function isn't called again
    timer->shouldRunCnt = 0;

    // Find this timer in the list and remove it
    node_t* currentNode = syncedTimerList.first;
    uint16_t idx = 0;
    while (currentNode != NULL)
    {
        if(currentNode->val == timer)
        {
            // Timer found in the list, remove it and return
            remove(&syncedTimerList, idx);
            return;
        }
        currentNode = currentNode->next;
        idx++;
    }
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
    /* We can't safely call the callbacks while iterating, so save the function
     * pointers args, and number of times the function should be called in some
     * temporary memory, then call them after iterating.
     * The temporary memory is large enough to accomodate the entire list
     */
    uint32_t origNumTimers = syncedTimerList.length;
    os_timer_func_t* funcsToCall[origNumTimers];
    void* argsToUse[origNumTimers];
    uint32_t callsToMake[origNumTimers];
    uint32_t numFuncsToCall = 0;

    // For each timer
    node_t* currentNode = syncedTimerList.first;
    while (currentNode != NULL)
    {
        syncedTimer_t* timer = (syncedTimer_t*) currentNode->val;

        bool shouldBeCalled = false;
        callsToMake[numFuncsToCall] = 0;
        // For as many times as the function should be called
        while(0 < timer->shouldRunCnt)
        {
            shouldBeCalled = true;
            // Save it to be called later
            funcsToCall[numFuncsToCall] = timer->timerFunc;
            argsToUse[numFuncsToCall] = timer->arg;
            callsToMake[numFuncsToCall]++;

            // Decrement, making sure not to underflow
            if(0 < timer->shouldRunCnt)
            {
                timer->shouldRunCnt--;
            }
        }
        // If this timer was called, move to the next index
        if(shouldBeCalled)
        {
            numFuncsToCall++;
        }

        // Iterate to the next timer
        currentNode = currentNode->next;
    }

    // Call the functions that need to be called
    for(uint32_t idx = 0; idx < numFuncsToCall; idx++)
    {
        while(callsToMake[idx]--)
        {
            funcsToCall[idx](argsToUse[idx]);
        }
    }
}
