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
 * Structs
 *==========================================================================*/

typedef struct __attribute__ ((__packed__))
{
    uint8_t g;
    uint8_t r;
    uint8_t b;
}
led_t;

/*============================================================================
 * Function prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR ledPatternEnterMode(void);
void ICACHE_FLASH_ATTR ledPatternButtonCallback(uint8_t state, int button, int down);
void ICACHE_FLASH_ATTR ledPatternTimerCallback(void);

/*============================================================================
 * Variables
 *==========================================================================*/

swadgeMode ledPatternsMode =
{
    .modeName = "ledPatterns",
    .fnEnterMode = ledPatternEnterMode,
    .fnExitMode = NULL,
    .fnTimerCallback = ledPatternTimerCallback,
    .fnButtonCallback = ledPatternButtonCallback,
    .fnAudioCallback = NULL,
    .wifiMode = NO_WIFI,
    .connectionColor = 0x00000000,
    .fnConnectionCallback = NULL,
    .fnPacketCallback = NULL,
};

static volatile color_t color = RED;

static uint8_t brightness = 0x00;
static bool gettingBrighter = true;

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

    led_t ledData[NUM_LIN_LEDS] = {0};
    setLeds((uint8_t*)ledData, sizeof(ledData));
}

/**
 * Called whenever there is a button press
 *
 * @param state A bitmask of all current button states
 * @param button The button ID that triggered this callback
 * @param down The state of the button that triggered this callback
 */
void ICACHE_FLASH_ATTR ledPatternButtonCallback(uint8_t state, int button, int down)
{
    if(1 == button && down)
    {
        color = (color + 1) % MAX_COLORS;
    }
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
    led_t ledData[NUM_LIN_LEDS] = {0};
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
        }
    }
    setLeds((uint8_t*)ledData, sizeof(ledData));

    // If the LEDs have dimmed back to zero, sleep for a while
    if(0x00 == brightness)
    {
        // Sleeeeep
        enterDeepSleep(true, SLEEP_US);
    }
}
