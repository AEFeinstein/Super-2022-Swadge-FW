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
void ICACHE_FLASH_ATTR strobeTimerOnCallback(void* timer_arg);
void ICACHE_FLASH_ATTR strobeTimerOffCallback(void* timer_arg);

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

#define NUM_STROBES 5
static const uint32_t strobePeriodsMs[NUM_STROBES][2] =
{
    {0, 0}, // 0 means on forever
    {900, 100}, //off ms, on ms
    {450,  50},
    {300,  50},
    {200,  50},
    {100,  50},    
};
uint8_t strobeIdx = 0;

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

    if(0 != strobePeriodsMs[strobeIdx][0])
    {
        os_timer_arm(&strobeTimerOn, strobePeriodsMs[strobeIdx][0], false);
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

    os_timer_disarm(&strobeTimerOn);
    os_timer_disarm(&strobeTimerOff);
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
            case 2:
            {
                strobeIdx = (strobeIdx + 1) % NUM_STROBES;

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
                break;
            }
            case 1:
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
