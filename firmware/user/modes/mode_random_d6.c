/*
 * mode_random_d6.c
 *
 *  Created on: Oct 19, 2018
 *      Author: adam
 */

/*============================================================================
 * Includes
 *==========================================================================*/

#include "ccconfig.h"
#include "user_main.h"
#include "mode_random_d6.h"
#include <stdint.h>
#include "osapi.h"
#include "embeddedout.h"

/*============================================================================
 * Defines
 *==========================================================================*/

#define NUM_LEDS 6

/*============================================================================
 * Function prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR randInit(void);
void ICACHE_FLASH_ATTR randDeinit(void);
void ICACHE_FLASH_ATTR randButtonCallback(uint8_t state, int button, int down);

void ICACHE_FLASH_ATTR randRollD6(void);
void ICACHE_FLASH_ATTR randLedAnimation(void* arg __attribute__((unused)));
void ICACHE_FLASH_ATTR randLedResult(void* arg __attribute__((unused)));

/*============================================================================
 * Variables
 *==========================================================================*/

swadgeMode randomD6Mode =
{
    .modeName = "random_d6",
    .fnEnterMode = randInit,
    .fnExitMode = randDeinit,
    .fnTimerCallback = NULL,
    .fnButtonCallback = randButtonCallback,
    .fnAudioCallback = NULL,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
};

struct
{
    os_timer_t LedAnimTimer;
    os_timer_t LedResTimer;
    led_t Leds[NUM_LEDS];
    uint8_t Idx;
    bool Running;
} ranD6 =
{
    .LedAnimTimer = {0},
    .LedResTimer = {0},
    .Leds = {{0}},
    .Idx = 0,
    .Running = 0,
};

/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * Initialize by zeroing out everything, setting up the timers, and getting
 * a random number
 */
void ICACHE_FLASH_ATTR randInit(void)
{
    // Clear everything
    ets_memset(&ranD6, 0, sizeof(ranD6));

    // Create and arm the timer
    os_timer_disarm(&ranD6.LedAnimTimer);
    os_timer_setfn(&ranD6.LedAnimTimer, randLedAnimation, NULL);

    // Create and arm the timer
    os_timer_disarm(&ranD6.LedResTimer);
    os_timer_setfn(&ranD6.LedResTimer, randLedResult, NULL);

    // Start the first random roll
    randRollD6();
}

/**
 * Deinitialize this by disarming all timers
 */
void ICACHE_FLASH_ATTR randDeinit(void)
{
    // Disarm the timer
    os_timer_disarm(&ranD6.LedAnimTimer);
    os_timer_disarm(&ranD6.LedResTimer);
}

/**
 * Start the animation to get a random D6 result
 * This only works if an animation isn't already in progress
 */
void ICACHE_FLASH_ATTR randRollD6(void)
{
    if(false == ranD6.Running)
    {
        ranD6.Running = true;

        // This will get randomized in the animation function
        ranD6.Idx = 0;

        // Turn off LEDs
        ets_memset(&ranD6.Leds, 0, sizeof(ranD6.Leds));
        setLeds((uint8_t*)&ranD6.Leds, sizeof(ranD6.Leds));

        // Run the animation every 100ms
        os_timer_arm(&ranD6.LedAnimTimer, 100, true);
        // Show a result in 3s, just once
        os_timer_arm(&ranD6.LedResTimer, 3000, false);
    }
}

/**
 * Called whenever there is a button press. If either button is pressed, call
 * runRandomD6()
 *
 * @param state A bitmask of all current button states
 * @param button The button ID that triggered this callback
 * @param down The state of the button that triggered this callback
 */
void ICACHE_FLASH_ATTR randButtonCallback(uint8_t state __attribute__((unused)),
        int button, int down)
{
    if(((1 == button) || (2 == button)) && down &&
            false == ranD6.Running)
    {
        randRollD6();
    }
}

/**
 * This is called every 100ms. It turns on a random LED with a random color if
 * all LEDs are off, then turns off the LED.
 *
 * @param arg unused
 */
void ICACHE_FLASH_ATTR randLedAnimation(void* arg __attribute__((unused)))
{
    // If all LEDs are off
    if(ranD6.Leds[ranD6.Idx].r == 0 &&
            ranD6.Leds[ranD6.Idx].g == 0 &&
            ranD6.Leds[ranD6.Idx].b == 0)
    {
        // Get a random LED which is not the last one. This is just an animation
        uint8_t newRandomIdx = os_random() % 6;
        while(ranD6.Idx == newRandomIdx)
        {
            newRandomIdx = os_random() % 6;
        }
        ranD6.Idx = newRandomIdx;

        // And a random color
        uint32_t randColor = EHSVtoHEX(os_random() & 0xFF, 0xFF, 0xFF);
        ranD6.Leds[ranD6.Idx].r = (randColor >>  0) & 0xFF;
        ranD6.Leds[ranD6.Idx].g = (randColor >>  8) & 0xFF;
        ranD6.Leds[ranD6.Idx].b = (randColor >> 16) & 0xFF;
    }
    else
    {
        // Turn the LED which is on off
        ranD6.Leds[ranD6.Idx].r = 0;
        ranD6.Leds[ranD6.Idx].g = 0;
        ranD6.Leds[ranD6.Idx].b = 0;
    }

    // Draw the LEDs
    setLeds((uint8_t*)&ranD6.Leds, sizeof(ranD6.Leds));
}

/**
 * This is run three seconds after the animation starts. Clear any lingering
 * LEDs, then show a random pattern, 1-6
 *
 * @param arg unused
 */
void ICACHE_FLASH_ATTR randLedResult(void* arg __attribute__((unused)))
{
    // Stop the animation
    os_timer_disarm(&ranD6.LedAnimTimer);
    ets_memset(&ranD6.Leds, 0, sizeof(ranD6.Leds));

    // Get a random number between 0 and 4294967291. Using just a uint32_t, the
    // random result is slightly weighted towards 0,1, and 2
    uint32_t randomResult = os_random();
    while(randomResult >= 4294967292u)
    {
        randomResult = os_random();
    }
    // Mod the number to get a random number between 1 and 6
    randomResult = 1 + (randomResult % 6);

    // Draw the result
    showLedCount(randomResult, getLedColorPerNumber(randomResult - 1));

    // Not running anymore
    ranD6.Running = false;
}
