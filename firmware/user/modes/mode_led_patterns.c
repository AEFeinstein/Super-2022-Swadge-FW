/*
 * mode_led_patterns.c
 *
 *  Created on: Oct 19, 2018
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
 * Defines
 *==========================================================================*/

#define STEP_SIZE 16
#define SLEEP_US  2000000

/*============================================================================
 * Enums
 *==========================================================================*/

typedef enum
{
    RED,
    GREEN,
    BLUE,
    MAX_COLORS
} color_t;

/*============================================================================
 * Function prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR ledPatternEnterMode(void);
void ICACHE_FLASH_ATTR ledPatternExitMode(void);
void ICACHE_FLASH_ATTR ledPatternButtonCallback(uint8_t state, int button, int down);
//void ICACHE_FLASH_ATTR ledPatternTimerCallback(void);
void ICACHE_FLASH_ATTR rainbowPatternTimerCallback(void* arg);

/*============================================================================
 * Variables
 *==========================================================================*/

swadgeMode ledPatternsMode =
{
    .modeName = "ledPatterns",
    .fnEnterMode = ledPatternEnterMode,
    .fnExitMode = ledPatternExitMode,
    .fnTimerCallback = NULL,
    .fnButtonCallback = ledPatternButtonCallback,
    .fnAudioCallback = NULL,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
};

static volatile color_t color = RED;

static uint8_t brightness = 0x00;
static bool gettingBrighter = true;
uint16_t rainbowDegree = 0;

os_timer_t ledPatternTimer = {0};

/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * Initialize by turning off all LEDs
 *
 * @return true, initialization never fails
 */
void ICACHE_FLASH_ATTR ledPatternEnterMode(void)
{
    color = RED;
    brightness = 0x00;
    gettingBrighter = true;

    led_t ledData[NUM_LIN_LEDS] = {{0}};
    setLeds(ledData, sizeof(ledData));

    os_printf("%s\r\n", __func__);

    os_timer_disarm(&ledPatternTimer);
    os_timer_setfn(&ledPatternTimer, rainbowPatternTimerCallback, NULL);
    os_timer_arm(&ledPatternTimer, 7, true);
}

/**
 *
 */
void ICACHE_FLASH_ATTR ledPatternExitMode(void)
{
    os_timer_disarm(&ledPatternTimer);
}

/**
 * Called whenever there is a button press
 *
 * @param state A bitmask of all current button states
 * @param button The button ID that triggered this callback
 * @param down The state of the button that triggered this callback
 */
void ICACHE_FLASH_ATTR ledPatternButtonCallback(uint8_t state __attribute__((unused)), int button, int down)
{
    if(1 == button && down)
    {
        color = (color + 1) % MAX_COLORS;
    }
}

/**
 * TODO
 */
void ICACHE_FLASH_ATTR rainbowPatternTimerCallback(void* arg __attribute__((unused)))
{
    led_t leds[6] = {{0}};
    rainbowDegree = (rainbowDegree + 1) % 256;

    if(gettingBrighter)
    {
        brightness++;
        if(0xFF == brightness)
        {
            os_printf("%s max\r\n", __func__);
            gettingBrighter = false;
        }
    }
    else
    {
        brightness--;
        if(0x00 == brightness)
        {
            os_printf("%s min\r\n", __func__);
            os_timer_disarm(&ledPatternTimer);
            enterDeepSleep(true, SLEEP_US);
        }
    }

    uint8_t i;
    for(i = 0; i < 6; i++)
    {
        int16_t angle = (((i * 256) / 6) + rainbowDegree) % 256;
        uint32_t color = EHSVtoHEX(angle, 0xFF, brightness);

        leds[i].r = (color >>  0) & 0xFF;
        leds[i].g = (color >>  8) & 0xFF;
        leds[i].b = (color >> 16) & 0xFF;
    }

    setLeds(leds, sizeof(leds));
}

/**
 * Called every 100ms, this updates the LED state
 */
void ICACHE_FLASH_ATTR ledPatternTimerCallback(void)
{
    // Either get brighter or dimmer
    if(true == gettingBrighter)
    {
        brightness += STEP_SIZE;
        if(256 - STEP_SIZE == brightness)
        {
            gettingBrighter = false;
        }
    }
    else
    {
        brightness -= STEP_SIZE;
        if(0x00 == brightness)
        {
            gettingBrighter = true;
        }
    }

    // Set the current LEDs
    led_t ledData[NUM_LIN_LEDS] = {{0}};
    uint8_t idx;
    for(idx = 0; idx < NUM_LIN_LEDS; idx++)
    {
        switch(color)
        {
            case RED:
            {
                ledData[idx].r = brightness;
                break;
            }
            case GREEN:
            {
                ledData[idx].g = brightness;
                break;
            }
            case BLUE:
            {
                ledData[idx].b = brightness;
                break;
            }
            case MAX_COLORS:
            {
                break;
            }
        }
    }
    setLeds(ledData, sizeof(ledData));

    // If the LEDs have dimmed back to zero, sleep for a while
    if(0x00 == brightness)
    {
        // Sleeeeep
        enterDeepSleep(true, SLEEP_US);
    }
}
