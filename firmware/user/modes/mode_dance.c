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

void ICACHE_FLASH_ATTR danceTimerMode1(void* arg);
void ICACHE_FLASH_ATTR danceTimerMode2(void* arg);

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

typedef struct
{
    os_timer_t timer;
    void (*timerFn)(void*);
    uint32_t period;
} timerWithPeriod;

timerWithPeriod danceTimers[] =
{
    {
        .timer = {0},
        .timerFn = danceTimerMode1,
        .period = 1
    },
    {
        .timer = {0},
        .timerFn = danceTimerMode2,
        .period = 100
    }
};

uint8_t currentDance = 0;

int ledCount = 0;

/*============================================================================
 * Functions
 *==========================================================================*/

void ICACHE_FLASH_ATTR danceEnterMode(void)
{
    // Set up and arm a timer to be called every 1 ms
    uint8_t i;
    for (i = 0; i < sizeof(danceTimers) / sizeof(danceTimers[0]); i++)
    {
        os_timer_disarm(&danceTimers[i].timer);
        os_timer_setfn(&danceTimers[i].timer, danceTimers[i].timerFn, NULL);
    }

    os_timer_arm(&danceTimers[0].timer, danceTimers[0].period, true);
}

void ICACHE_FLASH_ATTR danceExitMode(void)
{
    // Disarm the timer
    uint8_t i;
    for (i = 0; i < sizeof(danceTimers) / sizeof(danceTimers[0]); i++)
    {
        os_timer_disarm(&danceTimers[i].timer);
    }
}

void ICACHE_FLASH_ATTR danceButtonCallback(uint8_t state __attribute__((unused)),
        int button, int down)
{
    if(down)
    {
        if(1 == button)
        {
            // Button 1 pressed
            os_timer_disarm(&danceTimers[currentDance].timer);

            currentDance++;
            if(currentDance >= sizeof(danceTimers) / sizeof(danceTimers[0]))
            {
                currentDance = 0;
            }
            os_timer_arm(&danceTimers[currentDance].timer, danceTimers[currentDance].period, true);
        }
        else if(2 == button)
        {
            // Button 2 pressed
        }
    }
}

void ICACHE_FLASH_ATTR danceTimerMode1(void* arg __attribute__((unused)))
{
    led_t leds[6] = {{0}};

    ledCount = ledCount + 1;
    if(ledCount > 5)
    {
        ledCount = 0;
    }

    leds[ledCount].r = 255;
    leds[ledCount].g = 255;
    leds[ledCount].b = 255;

    // Draw LEDs here!!

    setLeds(leds, sizeof(leds));
}

void ICACHE_FLASH_ATTR danceTimerMode2(void* arg __attribute__((unused)))
{
    led_t leds[6] = {{0}};

    ledCount = ledCount + 1;
    if(ledCount > 5)
    {
        ledCount = 0;
    }

    leds[ledCount].r = 255;
    leds[ledCount].g = 0;
    leds[ledCount].b = 0;

    // Draw LEDs here!!

    setLeds(leds, sizeof(leds));
}
