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

/*============================================================================
 * Prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR flashlightEnterMode(void);
void ICACHE_FLASH_ATTR flashlightExitMode(void);
void ICACHE_FLASH_ATTR flashlightButtonCallback(uint8_t state, int button,
        int down);

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
    0x01,
    0x40,
    0xFF,
};
uint8_t brightnessIdx = 0;

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
    led_t leds[6] = {{0}};
    ets_memset(leds, brightnesses[brightnessIdx], sizeof(leds));
    setLeds(leds, sizeof(leds));
}

/**
 * Deinitializer the flashlight
 */
void ICACHE_FLASH_ATTR flashlightExitMode(void)
{
    // Turn LEDs off
    led_t leds[6] = {{0x00}};
    setLeds(leds, sizeof(leds));
}

/**
 * Adjusts flashlight brightness and strobe
 *
 * @param state A bitmask of all button statuses
 * @param button  The button number which was pressed
 * @param down 1 if the button was pressed, 0 if it was released
 */
void ICACHE_FLASH_ATTR flashlightButtonCallback(uint8_t state, int button,
        int down)
{
    if(down)
    {
        switch(button)
        {
            case 1:
            {
                break;
            }
            case 2:
            {
                brightnessIdx = (brightnessIdx + 1) % (sizeof(brightnesses) / sizeof(brightnesses[0]));
                led_t leds[6] = {{0}};
                ets_memset(leds, brightnesses[brightnessIdx], sizeof(leds));
                setLeds(leds, sizeof(leds));

                break;
            }
        }
    }
}
