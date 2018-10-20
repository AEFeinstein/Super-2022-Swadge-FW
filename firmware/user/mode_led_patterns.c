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

typedef enum
{
    LEFT,
    RIGHT,
    MAX_DIRECTION
} direction_t;

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
    .shouldConnect = NULL_MODE,
    .connectionColor = 0x00000000,
    .fnConnectionCallback = NULL,
    .fnPacketCallback = NULL,
    .next = NULL
};

static led_t ledData[NUM_LIN_LEDS] = {0};
static uint8_t currentLed = 0;
static volatile color_t color = RED;
static volatile direction_t direction = LEFT;

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
    direction = LEFT;
    currentLed = 0;
    memset(ledData, 0, sizeof(ledData));
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
    else if(2 == button && down)
    {
        direction = (direction + 1) % MAX_DIRECTION;
    }
}

/**
 * Called every 100ms, this updates the LED state
 */
void ICACHE_FLASH_ATTR ledPatternTimerCallback(void)
{
    memset(ledData, 0, sizeof(ledData));

    switch(direction)
    {
        case LEFT:
        {
            currentLed = (currentLed + 1) % NUM_LIN_LEDS;
            break;
        }
        case RIGHT:
        {
            if(0 == currentLed)
            {
                currentLed = NUM_LIN_LEDS - 1;
            }
            else
            {
                currentLed = (currentLed - 1) % NUM_LIN_LEDS;
            }
            break;
        }
    }

    switch(color)
    {
        case RED:
        {
            ledData[currentLed].r = 0xFF;
            break;
        }
        case GREEN:
        {
            ledData[currentLed].g = 0xFF;
            break;
        }
        case BLUE:
        {
            ledData[currentLed].b = 0xFF;
            break;
        }
    }

    setLeds((uint8_t*)ledData, sizeof(ledData));
}
