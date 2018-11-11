/*
 * mode_dance.c
 *
 *  Created on: Nov 10, 2018
 *      Author: adam
 */

/*============================================================================
 * Includes
 *==========================================================================*/

#include "ccconfig.h"
#include "user_main.h"
#include "mode_led_patterns.h"
#include <stdint.h>
#include "user_interface.h"
#include "user_main.h"
#include "osapi.h"
#include "embeddedout.h"

/*============================================================================
 * Prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR danceEnterMode(void);
void ICACHE_FLASH_ATTR danceExitMode(void);
void ICACHE_FLASH_ATTR danceButtonCallback(uint8_t state, int button, int down);

void ICACHE_FLASH_ATTR danceTimerCallback(void* arg);

/*============================================================================
 * Variables
 *==========================================================================*/

swadgeMode dancesMode =
{
    .modeName = "dances",
    .fnEnterMode = danceEnterMode,
    .fnExitMode = danceExitMode,
    .fnTimerCallback = NULL,
    .fnButtonCallback = danceButtonCallback,
    .fnAudioCallback = NULL,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
};

os_timer_t danceTimer = {0};

/*============================================================================
 * Functions
 *==========================================================================*/

void ICACHE_FLASH_ATTR danceEnterMode(void)
{
    // Set up and arm a timer to be called every 1 ms
    os_timer_disarm(&danceTimer);
    os_timer_setfn(&danceTimer, danceTimerCallback, NULL);
    os_timer_arm(&danceTimer, 1, true); // Timer 1ms set here
}

void ICACHE_FLASH_ATTR danceExitMode(void)
{
    // Disarm the timer
    os_timer_disarm(&danceTimer);
}

void ICACHE_FLASH_ATTR danceButtonCallback(uint8_t state __attribute__((unused)),
        int button, int down)
{
    if(down)
    {
        if(1 == button)
        {
            // Button 1 pressed
        }
        else if(2 == button)
        {
            // Button 2 pressed
        }
    }
}

void ICACHE_FLASH_ATTR danceTimerCallback(void* arg __attribute__((unused)))
{
    led_t leds[6] = {{0}};

    // Draw LEDs here!!

    setLeds((uint8_t*)&leds[0], sizeof(leds));
}
