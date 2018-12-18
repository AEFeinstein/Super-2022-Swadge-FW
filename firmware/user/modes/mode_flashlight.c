/*
 * mode_flashlight.c
 *
 *  Created on: Oct 19, 2018
 *      Author: adam
 */

/*============================================================================
 * Includes
 *==========================================================================*/

#include "osapi.h"
#include "user_main.h"
#include "mode_flashlight.h"
#include "morse_code.h"

/*============================================================================
 * Prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR flashlightEnterMode(void);
void ICACHE_FLASH_ATTR flashlightExitMode(void);
void ICACHE_FLASH_ATTR flashlightButtonCallback(uint8_t state, int button,
        int down);
void ICACHE_FLASH_ATTR strobeTimerOnCallback(void* timer_arg);
void ICACHE_FLASH_ATTR strobeTimerOffCallback(void* timer_arg);
void ICACHE_FLASH_ATTR holdButtonCallback(void* timer_arg);
void ICACHE_FLASH_ATTR startFlashlightStrobe(void);

/*============================================================================
 * Variables
 *==========================================================================*/

swadgeMode flashlightMode =
{
    .modeName = "flashlight",
    .fnEnterMode = flashlightEnterMode,
    .fnExitMode = flashlightExitMode,
    .fnTimerCallback = NULL,
    .fnButtonCallback = flashlightButtonCallback,
    .fnAudioCallback = NULL,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
};

static const uint8_t brightnesses[] =
{
    0xFF,
    0x40,
    0x01,
};
uint8_t brightnessIdx = 0;

os_timer_t strobeTimerOn = {0};
os_timer_t strobeTimerOff = {0};
os_timer_t flashlightButtonHoldTimer = {0};

#define NUM_STROBES 5
static const uint32_t strobePeriodsMs[NUM_STROBES][2] =
{
    {0, 0}, // 0 means on forever
    {900, 100}, //off ms, on ms
    {450,  50},
    {300,  50},
    {200,  50},
    {100,  40},
    // {60,   30},
    // {25,   20}
};
uint8_t strobeIdx = 0;

bool holdTimerRunning = false;
bool morseInProgress = false;

const char mysteryStrings[16][64] =
{
    "YOU SPOONY BARD",
    "WAKE UP MR FREEMAN",
    "WHAT A HORRIBLE NIGHT TO HAVE A CURSE",
    "THE GALAXY IS AT PEACE",

    "WAR WAR NEVER CHANGES",
    "ITS DANGEROUS TO GO ALONE",
    "THE YEAR IS 20XX",
    "I NEED SCISSORS 61",

    "YOU HAVE DIED OF DYSENTERY",
    "THE CAKE IS A LIE",
    "WOULD YOU KINDLY",
    "BAD TIMES ARE JUST TIMES THAT ARE BAD",

    "THANK YOU SO MUCH A FOR TO PLAYING MY GAME",
    "NO ITEMS FOX ONLY FINAL DESTINATION",
    "OHKHO MGPEX YLW UMTZJVQ VMSK",
    "GREETINGS FROM ADAM CHARLES DAC AND GILLIAN"
};

/*
* Oh, what, you think you're clever because you checked the source code for
* our hidden messages? Why do you think we made this open source?
*/

/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * Initializer the flashlight
 */
void ICACHE_FLASH_ATTR flashlightEnterMode(void)
{
    // Turn LEDs on
    brightnessIdx = 0;
    strobeIdx = 0;

    led_t leds[6] = {{0}};
    ets_memset(leds, brightnesses[brightnessIdx], sizeof(leds));
    setLeds(leds, sizeof(leds));

    os_timer_disarm(&strobeTimerOn);
    os_timer_setfn(&strobeTimerOn, strobeTimerOnCallback, NULL);

    os_timer_disarm(&strobeTimerOff);
    os_timer_setfn(&strobeTimerOff, strobeTimerOffCallback, NULL);

    os_timer_disarm(&flashlightButtonHoldTimer);
    os_timer_setfn(&flashlightButtonHoldTimer, holdButtonCallback, NULL);

    if(0 != strobePeriodsMs[strobeIdx][0])
    {
        os_timer_arm(&strobeTimerOn, strobePeriodsMs[strobeIdx][0], false);
    }

    holdTimerRunning = false;
    morseInProgress = false;

    // Disable debounce because the hold button logic cares about quick releases
    enableDebounce(false);
}

/**
 * Deinitializer the flashlight
 */
void ICACHE_FLASH_ATTR flashlightExitMode(void)
{
    // Turn LEDs off
    led_t leds[6] = {{0x00}};
    setLeds(leds, sizeof(leds));

    os_timer_disarm(&strobeTimerOn);
    os_timer_disarm(&strobeTimerOff);

    // Just in case
    endMorseSequence();

    // And enable debounce again
    enableDebounce(true);
}

/**
 * Adjusts flashlight brightness and strobe
 *
 * @param state A bitmask of all button statuses
 * @param button  The button number which was pressed
 * @param down 1 if the button was pressed, 0 if it was released
 */
void ICACHE_FLASH_ATTR flashlightButtonCallback(
    uint8_t state __attribute__((unused)), int button, int down)
{
    if(morseInProgress)
    {
        /* Ignore button presses while morse code is displaying. This will be
         * reset in flashlightEnterMode(), called when morse finishes
         */
        return;
    }

    if(down)
    {
        switch(button)
        {
            case 2:
            {
                // Right button cycles strobes
                strobeIdx = (strobeIdx + 1) % NUM_STROBES;
                startFlashlightStrobe();
                break;
            }
            case 1:
            {
                // Left button start a one time timer 1.5s from now
                os_timer_disarm(&flashlightButtonHoldTimer);
                os_timer_arm(&flashlightButtonHoldTimer, 1500, false);
                holdTimerRunning = true;
                break;
            }
        }
    }
    else if(1 == button && holdTimerRunning)
    {
        // If the left button is released while the timer hasn't expired yet

        // Disarm the timer
        os_timer_disarm(&flashlightButtonHoldTimer);

        // cycle the brightness
        brightnessIdx = (brightnessIdx + 1) %
                        (sizeof(brightnesses) / sizeof(brightnesses[0]));
        led_t leds[6] = {{0}};
        ets_memset(leds, brightnesses[brightnessIdx], sizeof(leds));
        setLeds(leds, sizeof(leds));
    }
}

/**
 * Start the flashlight strobe (or solid). Can be called when cycling through
 * strobes or at the end of the morse code sequence
 */
void ICACHE_FLASH_ATTR startFlashlightStrobe(void)
{
    morseInProgress = false;

    os_timer_disarm(&strobeTimerOn);
    os_timer_disarm(&strobeTimerOff);

    if(0 != strobePeriodsMs[strobeIdx][0])
    {
        os_timer_arm(&strobeTimerOn, strobePeriodsMs[strobeIdx][0], false);
    }
    else
    {
        // If there is no strobe, turn the LEDs on
        led_t leds[6] = {{0}};
        ets_memset(leds, brightnesses[brightnessIdx], sizeof(leds));
        setLeds(leds, sizeof(leds));
    }
}

/**
 * Callback used to strobe the flashlight LEDs
 *
 * @param timer_arg unused
 */
void ICACHE_FLASH_ATTR strobeTimerOnCallback(
    void* timer_arg __attribute__((unused)))
{
    // Turn LEDs on
    led_t leds[6] = {{0}};
    ets_memset(leds, brightnesses[brightnessIdx], sizeof(leds));
    setLeds(leds, sizeof(leds));

    // Flip
    os_timer_disarm(&strobeTimerOn);
    os_timer_arm(&strobeTimerOff, strobePeriodsMs[strobeIdx][1], false);
}

/**
 * Callback used to strobe the flashlight LEDs
 *
 * @param timer_arg unused
 */
void ICACHE_FLASH_ATTR strobeTimerOffCallback(
    void* timer_arg __attribute__((unused)))
{
    // Turn LEDs off
    led_t leds[6] = {{0}};
    setLeds(leds, sizeof(leds));

    // Flip
    os_timer_disarm(&strobeTimerOff);
    os_timer_arm(&strobeTimerOn, strobePeriodsMs[strobeIdx][0], false);
}

/**
 * Called if the left button is held down for 1.5s. Disarmed when the button
 * is released
 *
 * @param timer_arg unused
 */
void ICACHE_FLASH_ATTR holdButtonCallback(
    void* timer_arg __attribute__((unused)))
{
    holdTimerRunning = false;

    // Stop strobing
    os_timer_disarm(&strobeTimerOn);
    os_timer_disarm(&strobeTimerOff);

    // Show some random morse code
    uint8_t randIdx = os_random() & 0x0F; // 0-15
    startMorseSequence(mysteryStrings[randIdx], startFlashlightStrobe);
    morseInProgress = true;
}
