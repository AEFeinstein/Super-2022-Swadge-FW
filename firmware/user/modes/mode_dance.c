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
#include "custom_commands.h"

/*============================================================================
 * Defines
 *==========================================================================*/

#define SECRET_UNLOCK_CODE 0b101001000011010

/*============================================================================
 * Structs
 *==========================================================================*/

typedef struct
{
    os_timer_t timer;       ///< This is a system timer
    void (*timerFn)(void*); ///< This is a function which will be attached to the timer
    uint32_t period;        ///< This is the period, in ms, at which the function will be called
} timerWithPeriod;

/*============================================================================
 * Prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR danceEnterMode(void);
void ICACHE_FLASH_ATTR danceExitMode(void);
void ICACHE_FLASH_ATTR danceButtonCallback(uint8_t state, int button, int down);

void ICACHE_FLASH_ATTR unlockAnimation(void* arg);
void ICACHE_FLASH_ATTR danceTimerMode1(void* arg);
void ICACHE_FLASH_ATTR danceTimerMode2(void* arg);
void ICACHE_FLASH_ATTR danceTimerMode3(void* arg);
void ICACHE_FLASH_ATTR danceTimerMode4(void* arg);
void ICACHE_FLASH_ATTR danceTimerMode5(void* arg);
void ICACHE_FLASH_ATTR danceTimerMode6(void* arg);
void ICACHE_FLASH_ATTR danceTimerMode7(void* arg);
void ICACHE_FLASH_ATTR danceTimerMode8(void* arg);
/*============================================================================
 * Variables
 *==========================================================================*/

/**
 * This is the mode struct of callback pointers so the swadge knows how to
 * interact with this mode
 */
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

/**
 * This is an array of timerWithPeriod structs. Each is a separate animation
 * which is cycled around by button presses
 */
timerWithPeriod danceTimers[] =
{
    {
        .timer = {0},
        .timerFn = danceTimerMode1,
        .period = 100
    },
    {
        .timer = {0},
        .timerFn = danceTimerMode2,
        .period = 100
    },
    {
        .timer = {0},
        .timerFn = danceTimerMode3,
        .period = 100
    },
    {
        .timer = {0},
        .timerFn = danceTimerMode4,
        .period = 5
    },
    {
        .timer = {0},
        .timerFn = danceTimerMode5,
        .period = 100
    },
    {
        .timer = {0},
        .timerFn = danceTimerMode6,
        .period = 80
    },
    {
        .timer = {0},
        .timerFn = danceTimerMode7,
        .period = 500
    },
    {
        .timer = {0},
        .timerFn = danceTimerMode8,
        .period = 300
    }
};

/// Stuff for the secret unlock code
os_timer_t unlockAnimationTimer = {0};
uint8_t unlockBlinks = 0;
uint16_t buttonHistory = 0;

/// This is the current dance being animated
uint8_t currentDance = 0;

/// This is a state variable used in animations
int ledCount = 0;
int ledCount2 = 0;
bool led_bool = true;
/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * This initializer is called whenever dance mode is entered
 */
void ICACHE_FLASH_ATTR danceEnterMode(void)
{
    // Loop through danceTimers[] and attach each function to each timer
    uint8_t i;
    for (i = 0; i < sizeof(danceTimers) / sizeof(danceTimers[0]); i++)
    {
        os_timer_disarm(&danceTimers[i].timer);
        os_timer_setfn(&danceTimers[i].timer, danceTimers[i].timerFn, NULL);
    }

    os_timer_disarm(&unlockAnimationTimer);
    os_timer_setfn(&unlockAnimationTimer, unlockAnimation, NULL);

    // Start the first timer in danceTimers[]
    os_timer_arm(&danceTimers[0].timer, danceTimers[0].period, true);
}

/**
 * This is called whenever dance mode is exited
 */
void ICACHE_FLASH_ATTR danceExitMode(void)
{
    // Loop through danceTimers[] and disarm timer
    uint8_t i;
    for (i = 0; i < sizeof(danceTimers) / sizeof(danceTimers[0]); i++)
    {
        os_timer_disarm(&danceTimers[i].timer);
    }
}

/**
 * This is called whenever a button is pressed or released
 * Button 1 will cycle through animations when pressed
 * Button 2 is currently unused
 *
 * @param state  A bitmask of all button states
 * @param button The button which caused this event
 * @param down   true if the button was pressed, false if it was released
 */
void ICACHE_FLASH_ATTR danceButtonCallback(uint8_t state __attribute__((unused)),
        int button, int down)
{
    // If a button was pressed, not released
    if(down)
    {
        // Check if the secret code was entered
        buttonHistory <<= 1;
        buttonHistory  |= (button == 1 ? 1 : 0);
        if(SECRET_UNLOCK_CODE == buttonHistory)
        {
            // If it was, unlock all the patterns
            setGameWinsToMax();
            // Disarm the current animation, it will get rearmed after the pattern
            os_timer_disarm(&danceTimers[currentDance].timer);
            // Show a little thing
            unlockBlinks = 0;
            os_timer_arm(&unlockAnimationTimer, 200, true);
            return;
        }

        // Button 1 pressed
        if(1 == button)
        {
            // Stop the current animation
            os_timer_disarm(&danceTimers[currentDance].timer);

            // Iterate to the next animation
            currentDance++;
            if(currentDance >= sizeof(danceTimers) / sizeof(danceTimers[0]))
            {
                currentDance = 0;
            }

            // Start the next animation
            os_timer_arm(&danceTimers[currentDance].timer, danceTimers[currentDance].period, true);
        }
        else if(2 == button)
        {
            // Button 2 pressed
            // TODO do something here??
        }
    }
}

/**
 * This animation is shown when the unlock code is entered
 * @param arg
 */
void ICACHE_FLASH_ATTR unlockAnimation(void* arg __attribute__((unused)))
{
    // Set the LEDs to either on (white) or off
    led_t leds[6] = {{0}};
    if(unlockBlinks % 2 == 0)
    {
        ets_memset(leds, 0x80, sizeof(leds));
    }
    setLeds(leds, sizeof(leds));
    unlockBlinks++;

    // All done
    if(unlockBlinks == 8)
    {
        // Resume normal animation
        os_timer_disarm(&unlockAnimationTimer);
        os_timer_arm(&danceTimers[currentDance].timer, danceTimers[currentDance].period, true);
    }
}

/**
 * This animation is set to be called every 1 ms
 *
 * @param arg unused
 */
void ICACHE_FLASH_ATTR danceTimerMode1(void* arg __attribute__((unused)))
{
    // Declare some LEDs, all off
    led_t leds[6] = {{0}};

    // Skip to the next LED around the hexagon
    ledCount = ledCount + 1;
    if(ledCount > 5)
    {
        ledCount = 0;
    }

    // Turn the current LED on, full bright white
    leds[ledCount].r = 255;
    leds[ledCount].g = 255;
    leds[ledCount].b = 255;

    // Output the LED data, actually turning them on
    setLeds(leds, sizeof(leds));
}

/**
 * This animation is set to be called every 100ms
 *
 * @param arg unused
 */
void ICACHE_FLASH_ATTR danceTimerMode2(void* arg __attribute__((unused)))
{
    // Declare some LEDs, all off
    led_t leds[6] = {{0}};

    // Skip to the next LED around the hexagon
    ledCount = ledCount + 1;
    if(ledCount > 5)
    {
        ledCount = 0;
    }

    // Turn the current LED on, full bright red
    leds[ledCount].r = 255;
    leds[ledCount].g = 0;
    leds[ledCount].b = 0;

    // Output the LED data, actually turning them on
    setLeds(leds, sizeof(leds));
}



/**
 * This animation is set to be called every 100ms
 *
 * @param arg unused
 */
void ICACHE_FLASH_ATTR danceTimerMode3(void* arg __attribute__((unused)))
{
    // Declare some LEDs, all off
    led_t leds[6] = {{0}};

    // Skip to the next LED around the hexagon
    ledCount = ledCount - 1;
    if(ledCount < 0)
    {
        ledCount = 5;
    }

    // Turn the current LED on, full bright red
    leds[ledCount].r = 0;
    leds[ledCount].g = 0;
    leds[ledCount].b = 255;

    leds[(6 - ledCount ) % 6].r = 0;
    leds[(6 - ledCount ) % 6].g = 0;
    leds[(6 - ledCount ) % 6].b = 255;

    // Output the LED data, actually turning them on
    setLeds(leds, sizeof(leds));
}


/**
 *
 * @param arg unused
 */
void ICACHE_FLASH_ATTR danceTimerMode4(void* arg __attribute__((unused)))
{
    led_t leds[6] = {{0}};

    ledCount = ledCount + 1;
    if(ledCount > 256)
    {
        ledCount = 0;
    }

    uint8_t i;
    for(i = 0; i < 6; i++)
    {
        int16_t angle = (((i * 256)  / 6)) + ledCount % 256;
        uint32_t color = EHSVtoHEX(angle, 0xFF, 0xFF);

        leds[i].r = (color >>  0) & 0xFF;
        leds[i].g = (color >>  8) & 0xFF;
        leds[i].b = (color >> 16) & 0xFF;
    }

    // Output the LED data, actually turning them on
    setLeds(leds, sizeof(leds));
}



/**
 *
 * @param arg unused
 */
void ICACHE_FLASH_ATTR danceTimerMode5(void* arg __attribute__((unused)))
{
    led_t leds[6] = {{0}};

    ledCount = ledCount + 1;
    if(ledCount > 5)
    {
        ledCount = 0;
    }

    uint8_t i;
    for(i = 0; i < 6; i++)
    {
        int16_t angle = (((i * 256)  / 6)) % 256;
        uint32_t color = EHSVtoHEX(angle, 0xFF, 0xFF);

        leds[(i + ledCount) % 6].r = (color >>  0) & 0xFF;
        leds[(i + ledCount) % 6].g = (color >>  8) & 0xFF;
        leds[(i + ledCount) % 6].b = (color >> 16) & 0xFF;
    }

    // Output the LED data, actually turning them on
    setLeds(leds, sizeof(leds));
}


/**
 *
 * @param arg unused
 */
void ICACHE_FLASH_ATTR danceTimerMode6(void* arg __attribute__((unused)))
{
    led_t leds[6] = {{0}};

    ledCount = ledCount - 1;
    if(ledCount < 0)
    {
        ledCount = 255;
    }
    int16_t angle = ledCount % 256;
    uint32_t color = EHSVtoHEX(angle, 0xFF, 0xFF);

    leds[(ledCount) % 6].r = (color >>  0) & 0xFF;
    leds[(ledCount) % 6].g = (color >>  8) & 0xFF;
    leds[(ledCount) % 6].b = (color >> 16) & 0xFF;



    // Output the LED data, actually turning them on
    setLeds(leds, sizeof(leds));
}


/**
 * slowly changes colors
 * @param arg unused
 */
void ICACHE_FLASH_ATTR danceTimerMode7(void* arg __attribute__((unused)))
{
    led_t leds[6] = {{0}};

    ledCount = ledCount + 1;
    if(ledCount > 255)
    {
        ledCount = 0;
    }
    int16_t angle = ledCount % 256;
    uint32_t color = EHSVtoHEX(angle, 0xFF, 0xFF);

    uint8_t i;
    for(i = 0; i < 6; i++)
    {

        leds[i].r = (color >>  0) & 0xFF;
        leds[i].g = (color >>  8) & 0xFF;
        leds[i].b = (color >> 16) & 0xFF;
    }

    // Output the LED data, actually turning them on
    setLeds(leds, sizeof(leds));
}



/**
 * counts up in bianry
 * @param arg unused
 */
void ICACHE_FLASH_ATTR danceTimerMode8(void* arg __attribute__((unused)))
{
    led_t leds[6] = {{0}};

    ledCount = ledCount + 1;
    ledCount2 = ledCount2 + 1;
    if(ledCount2 > 75)
    {
        led_bool = !led_bool;
        ledCount2 = 0;
    }
    if(ledCount > 255)
    {
        ledCount = 0;
    }
    int16_t angle = ledCount % 256;
    uint32_t color = EHSVtoHEX(angle, 0xFF, 0xFF);

    uint8_t i;
    uint8_t j;
    for(i = 0; i < 6; i++)
    {
        if(ledCount2 >= 64)
        {
            leds[i].r = (color >>  0) & 0xFF;
            leds[i].g = (color >>  8) & 0xFF;
            leds[i].b = (color >> 16) & 0xFF;
        }
        else
        {

            if(led_bool)
            {
                j = 6 - i;
            }
            else
            {
                j = i;
            }
            if((ledCount2 >> i) & 1)
            {
                leds[(j) % 6].r = (color >>  0) & 0xFF;
                leds[(j) % 6].g = (color >>  8) & 0xFF;
                leds[(j) % 6].b = (color >> 16) & 0xFF;
            }
            else
            {
                leds[(j) % 6].r = 70;
                leds[(j) % 6].g = 0;
                leds[(j) % 6].b = 200;
            }
        }
    }
    // Output the LED data, actually turning them on
    setLeds(leds, sizeof(leds));
}
