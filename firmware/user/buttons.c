/*============================================================================
 * Includes
 *==========================================================================*/

#include <osapi.h>
#include <user_interface.h>

#include "buttons.h"
#include "user_main.h"

/*============================================================================
 * Structs
 *==========================================================================*/

typedef struct
{
    uint8_t stat;
    int btn;
    int down;
    uint32_t time;
} buttonEvt;

/*============================================================================
 * Defines
 *==========================================================================*/

#define NUM_BUTTON_EVTS      10
#define DEBOUNCE_US      200000
#define DEBOUNCE_US_FAST   7000

/*============================================================================
 * Variables
 *==========================================================================*/

volatile buttonEvt buttonQueue[NUM_BUTTON_EVTS] = {{0}};
volatile uint8_t buttonEvtHead = 0;
volatile uint8_t buttonEvtTail = 0;
uint32_t lastButtonPress[4] = {0};
bool debounceEnabled = true;

/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * This function is called from gpioInterrupt() as the callback for button
 * interrupts. It is called every time a button is pressed or released
 *
 * This is an interrupt, so it can't be ICACHE_FLASH_ATTR. It quickly queues
 * button events
 *
 * @param stat A bitmask of all button statuses
 * @param btn The button number which was pressed
 * @param down 1 if the button was pressed, 0 if it was released
 */
void HandleButtonEventIRQ( uint8_t stat, int btn, int down )
{
    // Queue up the button event
    buttonQueue[buttonEvtTail].stat = stat;
    buttonQueue[buttonEvtTail].btn = btn;
    buttonQueue[buttonEvtTail].down = down;
    buttonQueue[buttonEvtTail].time = system_get_time();
    buttonEvtTail = (buttonEvtTail + 1) % NUM_BUTTON_EVTS;
}

/**
 * Process queued button events synchronously
 */
void ICACHE_FLASH_ATTR HandleButtonEventSynchronous(void)
{
    if(buttonEvtHead != buttonEvtTail)
    {
        uint32_t debounceUs;
        if(debounceEnabled)
        {
            debounceUs = DEBOUNCE_US;
        }
        else
        {
            debounceUs = DEBOUNCE_US_FAST;
        }

        if(0 != buttonQueue[buttonEvtHead].btn &&
                buttonQueue[buttonEvtHead].time - lastButtonPress[buttonQueue[buttonEvtHead].btn] < debounceUs)
        {
            ; // Consume this event below, don't count it as a press
        }
        else
        {
            swadgeModeButtonCallback(buttonQueue[buttonEvtHead].stat,
                                     buttonQueue[buttonEvtHead].btn,
                                     buttonQueue[buttonEvtHead].down);

            // Note the time of this button press
            lastButtonPress[buttonQueue[buttonEvtHead].btn] = buttonQueue[buttonEvtHead].time;
        }

        // Increment the head
        buttonEvtHead = (buttonEvtHead + 1) % NUM_BUTTON_EVTS;
    }
}

/**
 * Enable or disable button debounce for non-mode switch buttons
 *
 * @param enable true to enable, false to disable
 */
void ICACHE_FLASH_ATTR enableDebounce(bool enable)
{
    debounceEnabled = enable;
}
