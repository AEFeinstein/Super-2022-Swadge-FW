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
 * Defines
 *==========================================================================*/

#define NUM_STROBES 6

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
 * Static Const Variables
 *==========================================================================*/

static const uint8_t brightnesses[] =
{
    0xFF,
    0x40,
    0x01,
};

static const uint32_t strobePeriodsMs[NUM_STROBES][2] =
{
    {0, 0}, // 0 means on forever
    {900, 100}, //off ms, on ms
    {450,  30},
    {300,  20},
    {200,  10},
    {100,  10},
};

/* Oh, what, you think you're clever because you checked the source code for
 * our hidden messages? Why do you think we made this open source?
 */
const char mysteryStrings[16][64] =
{
    "YOU SPOONY BARD",
    "WAKE UP MR FREEMAN",
    "WHAT A HORRIBLE NIGHT TO HAVE A CURSE",
    "THE GALAXY IS AT PEACE",

    "WOULD YOU KINDLY",
    "ITS DANGEROUS TO GO ALONE",
    "SUNNY IS A NICE KITTY",
    "I NEED SCISSORS 61",

    "YOU HAVE DIED OF DYSENTERY",
    "THE CAKE IS A LIE",
    "DOM LOVES BUTTS",
    "BAD TIMES ARE JUST TIMES THAT ARE BAD",

    "THANK YOU SO MUCH A FOR TO PLAYING MY GAME",
    "NO ITEMS FOX ONLY FINAL DESTINATION",
    "OHKHO MGPEX YLW UMTZJVQ VMSK",
    "GREETINGS FROM ADAM CHARLES DAC AND GILLIAN"
};

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

uint8_t brightnessIdx = 0;

os_timer_t strobeTimerOn = {0};
os_timer_t strobeTimerOff = {0};
os_timer_t flashlightButtonHoldTimer = {0};

uint8_t strobeIdx = 0;

bool holdTimerRunning = false;
bool morseInProgress = false;

/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * Initializer the flashlight
 */
void ICACHE_FLASH_ATTR flashlightEnterMode(void)
{
    brightnessIdx = 0;
    strobeIdx = 0;

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

    // Turn LEDs on
    led_t leds[6] = {{0}};
    ets_memset(leds, brightnesses[brightnessIdx], sizeof(leds));
    setLeds(leds, sizeof(leds));
}

/**
 * Deinitializer the flashlight
 */
void ICACHE_FLASH_ATTR flashlightExitMode(void)
{
    // Just in case, do this before disarming strobe timers, as it may start
    // strobing via startFlashlightStrobe()
    endMorseSequence();

    // Disarm all timers
    os_timer_disarm(&strobeTimerOn);
    os_timer_disarm(&strobeTimerOff);
    os_timer_disarm(&flashlightButtonHoldTimer);

    // Turn LEDs off
    led_t leds[6] = {{0x00}};
    setLeds(leds, sizeof(leds));

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
        // Immediately adjust the LEDs if the flashlight isn't strobing,
        // otherwise the strobe will reset the brightness
        if(0 == strobePeriodsMs[strobeIdx][0])
        {
            led_t leds[6] = {{0}};
            ets_memset(leds, brightnesses[brightnessIdx], sizeof(leds));
            setLeds(leds, sizeof(leds));
        }
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
