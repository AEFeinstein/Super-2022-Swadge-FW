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
void ICACHE_FLASH_ATTR strobeTimerCallback(void* timer_arg);

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

os_timer_t strobeTimer = {0};

static const uint32_t strobePeriodsMs[] =
{
    0,
    500,
    250,
};
uint8_t strobeIdx = 0;
bool flashlightLedsOn = true;

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
    flashlightLedsOn = true;

    led_t leds[6] = {{0}};
    ets_memset(leds, brightnesses[brightnessIdx], sizeof(leds));
    setLeds(leds, sizeof(leds));

    os_timer_disarm(&strobeTimer);
    os_timer_setfn(&strobeTimer, strobeTimerCallback, NULL);

    if(0 != strobePeriodsMs[strobeIdx])
    {
        os_timer_arm(&strobeTimer, strobePeriodsMs[strobeIdx], true);
    }
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
void ICACHE_FLASH_ATTR flashlightButtonCallback(
    uint8_t state __attribute__((unused)), int button, int down)
{
    if(down)
    {
        switch(button)
        {
            case 1:
            {
                strobeIdx = (strobeIdx + 1) %
                            (sizeof(strobePeriodsMs) / sizeof(strobePeriodsMs[0]));

                os_timer_disarm(&strobeTimer);
                if(0 != strobePeriodsMs[strobeIdx])
                {
                    os_timer_arm(&strobeTimer, strobePeriodsMs[strobeIdx], true);
                }
                break;
            }
            case 2:
            {
                brightnessIdx = (brightnessIdx + 1) %
                                (sizeof(brightnesses) / sizeof(brightnesses[0]));
                led_t leds[6] = {{0}};
                ets_memset(leds, brightnesses[brightnessIdx], sizeof(leds));
                setLeds(leds, sizeof(leds));

                break;
            }
        }
    }
}

/**
 * Callback used to strobe the flashlight LEDs
 *
 * @param timer_arg unused
 */
void ICACHE_FLASH_ATTR strobeTimerCallback(
    void* timer_arg __attribute__((unused)))
{
    led_t leds[6] = {{0}};
    // If LEDs aren't on, turn them on
    if(!flashlightLedsOn)
    {
        ets_memset(leds, brightnesses[brightnessIdx], sizeof(leds));
    }
    setLeds(leds, sizeof(leds));

    // Flip
    flashlightLedsOn = !flashlightLedsOn;
}
