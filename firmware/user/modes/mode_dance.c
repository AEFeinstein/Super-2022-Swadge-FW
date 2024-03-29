/*
 * mode_dance.c
 *
 *  Created on: Nov 10, 2018
 *      Author: adam
 */

/*============================================================================
 * Includes
 *==========================================================================*/

#include <osapi.h>
#include <stdint.h>
#include <user_interface.h>

#include "ccconfig.h"
#include "user_main.h"
#include "user_main.h"
#include "embeddedout.h"
#include "mode_dance.h"
#include "hsv_utils.h"

/*============================================================================
 * Typedefs
 *==========================================================================*/

typedef void (*ledDance)(uint32_t, uint32_t, bool);

typedef struct
{
    ledDance func;
    uint32_t arg;
    char* name;
} ledDanceArg;

#define RGB_2_ARG(r,g,b) ((((r)&0xFF) << 16) | (((g)&0xFF) << 8) | (((b)&0xFF)))
#define ARG_R(arg) (((arg) >> 16)&0xFF)
#define ARG_G(arg) (((arg) >>  8)&0xFF)
#define ARG_B(arg) (((arg) >>  0)&0xFF)

/*============================================================================
 * Prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR setDanceLeds(led_t* ledData, uint8_t ledDataLen);
uint32_t ICACHE_FLASH_ATTR danceRand(uint32_t upperBound);

void ICACHE_FLASH_ATTR danceComet(uint32_t tElapsedUs, uint32_t arg, bool reset);
void ICACHE_FLASH_ATTR danceRise(uint32_t tElapsedUs, uint32_t arg, bool reset);
void ICACHE_FLASH_ATTR dancePulse(uint32_t tElapsedUs, uint32_t arg, bool reset);
void ICACHE_FLASH_ATTR danceSmoothRainbow(uint32_t tElapsedUs, uint32_t arg, bool reset);
void ICACHE_FLASH_ATTR danceSharpRainbow(uint32_t tElapsedUs, uint32_t arg, bool reset);
void ICACHE_FLASH_ATTR danceRainbowSolid(uint32_t tElapsedUs, uint32_t arg, bool reset);
void ICACHE_FLASH_ATTR danceBinaryCounter(uint32_t tElapsedUs, uint32_t arg, bool reset);
void ICACHE_FLASH_ATTR danceFire(uint32_t tElapsedUs, uint32_t arg, bool reset);
void ICACHE_FLASH_ATTR dancePoliceSiren(uint32_t tElapsedUs, uint32_t arg, bool reset);
void ICACHE_FLASH_ATTR dancePureRandom(uint32_t tElapsedUs, uint32_t arg, bool reset);
void ICACHE_FLASH_ATTR danceRandomDance(uint32_t tElapsedUs, uint32_t arg, bool reset);
void ICACHE_FLASH_ATTR danceChristmas(uint32_t tElapsedUs, uint32_t arg, bool reset);
void ICACHE_FLASH_ATTR danceNone(uint32_t tElapsedUs, uint32_t arg, bool reset);

/*============================================================================
 * Variables
 *==========================================================================*/

static const ledDanceArg ledDances[] =
{
    {.func = danceComet, .arg = RGB_2_ARG(0xFF, 0, 0), .name = "Comet R"},
    {.func = danceComet, .arg = RGB_2_ARG(0, 0xFF, 0), .name = "Comet G"},
    {.func = danceComet, .arg = RGB_2_ARG(0, 0, 0xFF), .name = "Comet B"},
    {.func = danceComet, .arg = RGB_2_ARG(0, 0, 0),    .name = "Comet RGB"},
    {.func = danceRise,  .arg = RGB_2_ARG(0xFF, 0, 0), .name = "Rise R"},
    {.func = danceRise,  .arg = RGB_2_ARG(0, 0xFF, 0), .name = "Rise G"},
    {.func = danceRise,  .arg = RGB_2_ARG(0, 0, 0xFF), .name = "Rise B"},
    {.func = danceRise,  .arg = RGB_2_ARG(0, 0, 0),    .name = "Rise RGB"},
    {.func = dancePulse, .arg = RGB_2_ARG(0xFF, 0, 0), .name = "Pulse R"},
    {.func = dancePulse, .arg = RGB_2_ARG(0, 0xFF, 0), .name = "Pulse G"},
    {.func = dancePulse, .arg = RGB_2_ARG(0, 0, 0xFF), .name = "Pulse B"},
    {.func = dancePulse, .arg = RGB_2_ARG(0, 0, 0),    .name = "Pulse RGB"},
    {.func = danceSharpRainbow,  .arg = 0, .name = "Rainbow Sharp"},
    {.func = danceSmoothRainbow, .arg = 20000, .name = "Rainbow Slow"},
    {.func = danceSmoothRainbow, .arg =  4000, .name = "Rainbow Fast"},
    {.func = danceRainbowSolid,  .arg = 0, .name = "Rainbow Solid"},
    {.func = danceFire, .arg = RGB_2_ARG(0xFF, 51, 0), .name = "Fire R"},
    {.func = danceFire, .arg = RGB_2_ARG(0, 0xFF, 51), .name = "Fire G"},
    {.func = danceFire, .arg = RGB_2_ARG(51, 0, 0xFF), .name = "Fire B"},
    {.func = danceBinaryCounter, .arg = 0, .name = "Binary"},
    {.func = dancePoliceSiren,   .arg = 0, .name = "Siren"},
    {.func = dancePureRandom,    .arg = 0, .name = "Random"},
    {.func = danceChristmas,     .arg = 1, .name = "Holiday 1"},
    {.func = danceChristmas,     .arg = 0, .name = "Holiday 2"},
    {.func = danceNone,          .arg = 0, .name = "None"},
    {.func = danceRandomDance,   .arg = 0, .name = "???"},
};

uint8_t danceBrightness = 1;

/*============================================================================
 * Functions
 *==========================================================================*/

/** @return The number of different tances
 */
uint8_t ICACHE_FLASH_ATTR getNumDances(void)
{
    return (lengthof(ledDances));
}

/**
 * @brief Get the Dance Name
 *
 * @param idx  The index of the dance
 * @return the dance name
 */
char* getDanceName(uint8_t idx)
{
    return ledDances[idx].name;
}

/** This is called to clear specific dance variables
 */
void ICACHE_FLASH_ATTR danceClearVars(uint8_t idx)
{
    // Reset the specific dance
    ledDances[idx].func(0, ledDances[idx].arg, true);
}

/** Set the brightness index
 *
 * @param brightness LEDs get divided by this before being set
 */
void ICACHE_FLASH_ATTR setDanceBrightness(uint8_t brightness)
{
    if (0 == brightness)
    {
        brightness = 1;
    }
    danceBrightness = brightness;
}

/** Intermediate function which adjusts brightness and sets the LEDs
 *
 * @param ledData    The LEDs to be scaled, then set
 * @param ledDataLen The length of the LEDs to set
 */
void ICACHE_FLASH_ATTR setDanceLeds(led_t* ledData, uint8_t ledDataLen)
{
    led_t ledDataTmp[ledDataLen / sizeof(led_t)];
    uint8_t i;
    for(i = 0; i < ledDataLen / sizeof(led_t); i++)
    {
        ledDataTmp[i].r = ledData[i].r / danceBrightness;
        ledDataTmp[i].g = ledData[i].g / danceBrightness;
        ledDataTmp[i].b = ledData[i].b / danceBrightness;
    }
    setLeds(ledDataTmp, ledDataLen);
}

/** Get a random number from a range.
 *
 * This isn't true-random, unless bound is a power of 2. But it's close enough?
 * The problem is that os_random() returns a number between [0, 2^64), and the
 * size of the range may not be even divisible by bound
 *
 * For what it's worth, this is what Arduino's random() does. It lies!
 *
 * @param bound An upper bound of the random range to return
 * @return A number in the range [0,bound), which does not include bound
 */
uint32_t ICACHE_FLASH_ATTR danceRand(uint32_t bound)
{
    if(bound == 0)
    {
        return 0;
    }
    return os_random() % bound;
}

/** Call this to animate LEDs. Dances use the system time for animations, so this
 * should be called reasonably fast for smooth operation
 *
 * @param danceIdx The index of the dance to display.
 */
void ICACHE_FLASH_ATTR danceLeds(uint8_t danceIdx)
{
    static uint32_t tLast = 0;
    if(0 == tLast)
    {
        tLast = system_get_time();
    }
    else
    {
        uint32_t tNow = system_get_time();
        uint32_t tElapsedUs = tNow - tLast;
        tLast = tNow;
        ledDances[danceIdx].func(tElapsedUs, ledDances[danceIdx].arg, false);
    }
}

/** This animation is set to be called every 100 ms
 * Rotate a single white LED around the hexagon
 *
 * @param tElapsedUs The time elapsed since last call, in microseconds
 * @param reset      true to reset this dance's variables
 */
void ICACHE_FLASH_ATTR danceComet(uint32_t tElapsedUs, uint32_t arg, bool reset)
{
    static int32_t ledCount = 0;
    static uint8_t rainbow = 0;
    static int32_t msCount = 0;
    static uint32_t tAccumulated = 0;
    static led_t leds[NUM_LIN_LEDS] = {{0}};

    if(reset)
    {
        ledCount = 0;
        rainbow = 0;
        msCount = 80;
        tAccumulated = 2000;
        ets_memset(leds, sizeof(leds), 0);
        return;
    }

    bool ledsUpdated = false;

    tAccumulated += tElapsedUs;
    while(tAccumulated >= 2000)
    {
        tAccumulated -= 2000;
        for(uint8_t i = 0; i < NUM_LIN_LEDS; i++)
        {
            if(leds[i].r > 0)
            {
                leds[i].r--;
            }
            if(leds[i].g > 0)
            {
                leds[i].g--;
            }
            if(leds[i].b > 0)
            {
                leds[i].b--;
            }
        }
        msCount++;

        if(msCount % 10 == 0)
        {
            rainbow++;
        }

        if(msCount >= 80)
        {
            if(0 == arg)
            {
                int32_t color = EHSVtoHEX(rainbow, 0xFF, 0xFF);
                leds[ledCount].r = (color >>  0) & 0xFF;
                leds[ledCount].g = (color >>  8) & 0xFF;
                leds[ledCount].b = (color >> 16) & 0xFF;
            }
            else
            {
                leds[ledCount].r = ARG_R(arg);
                leds[ledCount].g = ARG_G(arg);
                leds[ledCount].b = ARG_B(arg);
            }
            ledCount = (ledCount + 1) % NUM_LIN_LEDS;
            msCount = 0;
        }
        ledsUpdated = true;
    }

    if(ledsUpdated)
    {
        setDanceLeds(leds, sizeof(leds));
    }
}

/** This animation is set to be called every 100ms
 * Blink all LEDs red for on for 500ms, then off for 500ms
 *
 * @param tElapsedUs The time elapsed since last call, in microseconds
 * @param reset      true to reset this dance's variables
 */
void ICACHE_FLASH_ATTR dancePulse(uint32_t tElapsedUs, uint32_t arg, bool reset)
{
    static uint8_t ledVal = 0;
    static uint8_t randColor = 0;
    static bool goingUp = true;
    static uint32_t tAccumulated = 0;

    if(reset)
    {
        ledVal = 0;
        randColor = 0;
        goingUp = true;
        tAccumulated = 5000;
        return;
    }

    // Declare some LEDs, all off
    led_t leds[NUM_LIN_LEDS] = {{0}};
    bool ledsUpdated = false;

    tAccumulated += tElapsedUs;
    while(tAccumulated >= 5000)
    {
        tAccumulated -= 5000;

        if(goingUp)
        {
            ledVal++;
            if(255 == ledVal)
            {
                goingUp = false;
            }
        }
        else
        {
            ledVal--;
            if(0 == ledVal)
            {
                goingUp = true;
                randColor = danceRand(256);
            }
        }

        for (int i = 0; i < NUM_LIN_LEDS; i++)
        {
            if(0 == arg)
            {
                int32_t color = EHSVtoHEX(randColor, 0xFF, 0xFF);
                leds[i].r = (ledVal * ((color >>  0) & 0xFF) >> 8);
                leds[i].g = (ledVal * ((color >>  8) & 0xFF) >> 8);
                leds[i].b = (ledVal * ((color >> 16) & 0xFF) >> 8);
            }
            else
            {
                leds[i].r = (ledVal * ARG_R(arg)) >> 8;
                leds[i].g = (ledVal * ARG_G(arg)) >> 8;
                leds[i].b = (ledVal * ARG_B(arg)) >> 8;
            }
        }
        ledsUpdated = true;
    }

    if(ledsUpdated)
    {
        setDanceLeds(leds, sizeof(leds));
    }
}

/** This animation is set to be called every 100 ms
 * Rotate a single white LED around the hexagon
 *
 * @param tElapsedUs The time elapsed since last call, in microseconds
 * @param reset      true to reset this dance's variables
 */
void ICACHE_FLASH_ATTR danceRise(uint32_t tElapsedUs, uint32_t arg, bool reset)
{
    static int16_t levels[NUM_LIN_LEDS / 2] = {0, -256, -512};
    static bool rising[NUM_LIN_LEDS / 2] = {true, true, true};
    static uint8_t angle = 0;
    static uint32_t tAccumulated = 0;

    if(reset)
    {
        for(uint8_t i = 0; i < NUM_LIN_LEDS / 2; i++)
        {
            levels[i] = i * -256;
            rising[i] = true;
        }
        angle = 0;
        tAccumulated = 800;
        return;
    }

    bool ledsUpdated = false;
    led_t leds[NUM_LIN_LEDS] = {{0}};

    tAccumulated += tElapsedUs;
    while(tAccumulated >= 800)
    {
        tAccumulated -= 800;

        if(true == rising[0] && 0 == levels[0])
        {
            angle = danceRand(256);
        }

        for(uint8_t i = 0; i < NUM_LIN_LEDS / 2; i++)
        {
            if(rising[i])
            {
                levels[i]++;
                if(levels[i] == 255)
                {
                    rising[i] = false;
                }
            }
            else
            {
                levels[i]--;
                if(levels[i] == -512)
                {
                    rising[i] = true;
                }
            }
        }

        if(0 == arg)
        {
            int32_t color = EHSVtoHEX(angle, 0xFF, 0xFF);
            for(uint8_t i = 0; i < NUM_LIN_LEDS / 2; i++)
            {
                if(levels[i] > 0)
                {
                    leds[i].r = (levels[i] * ((color >>  0) & 0xFF) >> 8);
                    leds[i].g = (levels[i] * ((color >>  8) & 0xFF) >> 8);
                    leds[i].b = (levels[i] * ((color >> 16) & 0xFF) >> 8);

                    leds[NUM_LIN_LEDS - 1 - i].r = (levels[i] * ((color >>  0) & 0xFF) >> 8);
                    leds[NUM_LIN_LEDS - 1 - i].g = (levels[i] * ((color >>  8) & 0xFF) >> 8);
                    leds[NUM_LIN_LEDS - 1 - i].b = (levels[i] * ((color >> 16) & 0xFF) >> 8);
                }
            }
        }
        else
        {
            for(uint8_t i = 0; i < NUM_LIN_LEDS / 2; i++)
            {
                if(levels[i] > 0)
                {
                    leds[i].r = (levels[i] * ARG_R(arg)) >> 8;
                    leds[i].g = (levels[i] * ARG_G(arg)) >> 8;
                    leds[i].b = (levels[i] * ARG_B(arg)) >> 8;

                    leds[NUM_LIN_LEDS - 1 - i].r = (levels[i] * ARG_R(arg)) >> 8;
                    leds[NUM_LIN_LEDS - 1 - i].g = (levels[i] * ARG_G(arg)) >> 8;
                    leds[NUM_LIN_LEDS - 1 - i].b = (levels[i] * ARG_B(arg)) >> 8;
                }
            }
        }
        ledsUpdated = true;
    }

    if(ledsUpdated)
    {
        setDanceLeds(leds, sizeof(leds));
    }
}

/** Smoothly rotate a color wheel around the hexagon
 *
 * @param tElapsedUs The time elapsed since last call, in microseconds
 * @param reset      true to reset this dance's variables
 */
void ICACHE_FLASH_ATTR danceSmoothRainbow(uint32_t tElapsedUs, uint32_t arg , bool reset)
{
    static uint32_t tAccumulated = 0;
    static uint8_t ledCount = 0;

    if(reset)
    {
        ledCount = 0;
        tAccumulated = arg;
        return;
    }

    // Declare some LEDs, all off
    led_t leds[NUM_LIN_LEDS] = {{0}};
    bool ledsUpdated = false;

    tAccumulated += tElapsedUs;
    while(tAccumulated >= arg)
    {
        tAccumulated -= arg;
        ledsUpdated = true;

        ledCount--;

        uint8_t i;
        for(i = 0; i < NUM_LIN_LEDS; i++)
        {
            int16_t angle = ((((i * 256) / NUM_LIN_LEDS)) + ledCount) % 256;
            uint32_t color = EHSVtoHEX(angle, 0xFF, 0xFF);

            leds[i].r = (color >>  0) & 0xFF;
            leds[i].g = (color >>  8) & 0xFF;
            leds[i].b = (color >> 16) & 0xFF;
        }
    }
    // Output the LED data, actually turning them on
    if(ledsUpdated)
    {
        setDanceLeds(leds, sizeof(leds));
    }
}

/** Sharply rotate a color wheel around the hexagon
 *
 * @param tElapsedUs The time elapsed since last call, in microseconds
 * @param reset      true to reset this dance's variables
 */
void ICACHE_FLASH_ATTR danceSharpRainbow(uint32_t tElapsedUs, uint32_t arg __attribute__((unused)), bool reset)
{
    static int32_t ledCount = 0;
    static uint32_t tAccumulated = 0;

    if(reset)
    {
        ledCount = 0;
        tAccumulated = 300000;
        return;
    }

    // Declare some LEDs, all off
    led_t leds[NUM_LIN_LEDS] = {{0}};
    bool ledsUpdated = false;

    tAccumulated += tElapsedUs;
    while(tAccumulated >= 300000)
    {
        tAccumulated -= 300000;
        ledsUpdated = true;

        ledCount = ledCount + 1;
        if(ledCount > NUM_LIN_LEDS - 1)
        {
            ledCount = 0;
        }

        uint8_t i;
        for(i = 0; i < NUM_LIN_LEDS; i++)
        {
            int16_t angle = (((i * 256) / NUM_LIN_LEDS)) % 256;
            uint32_t color = EHSVtoHEX(angle, 0xFF, 0xFF);

            leds[(i + ledCount) % NUM_LIN_LEDS].r = (color >>  0) & 0xFF;
            leds[(i + ledCount) % NUM_LIN_LEDS].g = (color >>  8) & 0xFF;
            leds[(i + ledCount) % NUM_LIN_LEDS].b = (color >> 16) & 0xFF;
        }
    }
    // Output the LED data, actually turning them on
    if(ledsUpdated)
    {
        setDanceLeds(leds, sizeof(leds));
    }
}

/** Counts up to 64 in binary. At 64, the color is held for ~3s
 * The 'on' color is smoothly iterated over the color wheel. The 'off'
 * color is also iterated over the color wheel, 180 degrees offset from 'on'
 * @param tElapsedUs The time elapsed since last call, in microseconds
 * @param reset      true to reset this dance's variables
 */
void ICACHE_FLASH_ATTR danceBinaryCounter(uint32_t tElapsedUs, uint32_t arg __attribute__((unused)), bool reset)
{
    static int32_t ledCount = 0;
    static int32_t ledCount2 = 0;
    static bool led_bool = false;
    static uint32_t tAccumulated = 0;

    if(reset)
    {
        ledCount = 0;
        ledCount2 = 0;
        led_bool = false;
        tAccumulated = 300000;
        return;
    }

    // Declare some LEDs, all off
    led_t leds[NUM_LIN_LEDS] = {{0}};
    bool ledsUpdated = false;

    tAccumulated += tElapsedUs;
    while(tAccumulated >= 300000)
    {
        tAccumulated -= 300000;
        ledsUpdated = true;

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
        uint32_t colorOn = EHSVtoHEX(angle, 0xFF, 0xFF);
        uint32_t colorOff = EHSVtoHEX((angle + 128) % 256, 0xFF, 0xFF);

        uint8_t i;
        uint8_t j;
        for(i = 0; i < NUM_LIN_LEDS; i++)
        {
            if(ledCount2 >= 64)
            {
                leds[i].r = (colorOn >>  0) & 0xFF;
                leds[i].g = (colorOn >>  8) & 0xFF;
                leds[i].b = (colorOn >> 16) & 0xFF;
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
                    leds[(j) % NUM_LIN_LEDS].r = (colorOn >>  0) & 0xFF;
                    leds[(j) % NUM_LIN_LEDS].g = (colorOn >>  8) & 0xFF;
                    leds[(j) % NUM_LIN_LEDS].b = (colorOn >> 16) & 0xFF;
                }
                else
                {
                    leds[(j) % NUM_LIN_LEDS].r = (colorOff >>  0) & 0xFF;
                    leds[(j) % NUM_LIN_LEDS].g = (colorOff >>  8) & 0xFF;
                    leds[(j) % NUM_LIN_LEDS].b = (colorOff >> 16) & 0xFF;
                }
            }
        }
    }
    // Output the LED data, actually turning them on
    if(ledsUpdated)
    {
        setDanceLeds(leds, sizeof(leds));
    }
}

/**
 * Fire pattern. All LEDs are random amount of red, and fifth that of green.
 * The LEDs towards the bottom have a brighter base and more randomness. The
 * LEDs towards the top are dimmer and have less randomness.
 *
 * @param tElapsedUs The time elapsed since last call, in microseconds
 * @param reset      true to reset this dance's variables
 */
void ICACHE_FLASH_ATTR danceFire(uint32_t tElapsedUs, uint32_t arg, bool reset)
{
    static uint32_t tAccumulated = 0;

    if(reset)
    {
        tAccumulated = 100000;
        return;
    }

    // Declare some LEDs, all off
    led_t leds[NUM_LIN_LEDS] = {{0}};
    bool ledsUpdated = false;

    tAccumulated += tElapsedUs;
    while(tAccumulated >= 75000)
    {
        tAccumulated -= 75000;
        ledsUpdated = true;

        uint8_t randC;

        // Base
        randC = danceRand(105) + 150;
        leds[0].r = (randC * ARG_R(arg)) / 256;
        leds[0].g = (randC * ARG_G(arg)) / 256;
        leds[0].b = (randC * ARG_B(arg)) / 256;
        randC = danceRand(105) + 150;
        leds[5].r = (randC * ARG_R(arg)) / 256;
        leds[5].g = (randC * ARG_G(arg)) / 256;
        leds[5].b = (randC * ARG_B(arg)) / 256;

        // Mid
        randC = danceRand(32) + 16;
        leds[1].r = (randC * ARG_R(arg)) / 256;
        leds[1].g = (randC * ARG_G(arg)) / 256;
        leds[1].b = (randC * ARG_B(arg)) / 256;
        randC = danceRand(32) + 16;
        leds[4].r = (randC * ARG_R(arg)) / 256;
        leds[4].g = (randC * ARG_G(arg)) / 256;
        leds[4].b = (randC * ARG_B(arg)) / 256;

        // Tip
        randC = danceRand(16) + 4;
        leds[2].r = (randC * ARG_R(arg)) / 256;
        leds[2].g = (randC * ARG_G(arg)) / 256;
        leds[2].b = (randC * ARG_B(arg)) / 256;
        randC = danceRand(16) + 4;
        leds[3].r = (randC * ARG_R(arg)) / 256;
        leds[3].g = (randC * ARG_G(arg)) / 256;
        leds[3].b = (randC * ARG_B(arg)) / 256;
    }
    if(ledsUpdated)
    {
        setDanceLeds(leds, sizeof(leds));
    }
}

/** police sirens, flash half red then half blue
 *
 * @param tElapsedUs The time elapsed since last call, in microseconds
 * @param reset      true to reset this dance's variables
 */
void ICACHE_FLASH_ATTR dancePoliceSiren(uint32_t tElapsedUs, uint32_t arg __attribute__((unused)), bool reset)
{
    static int32_t ledCount;
    static uint32_t tAccumulated = 0;

    if(reset)
    {
        ledCount = 0;
        tAccumulated = 120000;
        return;
    }

    // Declare some LEDs, all off
    led_t leds[NUM_LIN_LEDS] = {{0}};
    bool ledsUpdated = false;

    tAccumulated += tElapsedUs;
    while(tAccumulated >= 120000)
    {
        tAccumulated -= 120000;
        ledsUpdated = true;

        // Skip to the next LED around the hexagon
        ledCount = ledCount + 1;
        if(ledCount > NUM_LIN_LEDS)
        {
            ledCount = 0;

        }

        uint8_t i;
        if(ledCount < (NUM_LIN_LEDS >> 1))
        {
            uint32_t red = EHSVtoHEX(245, 0xFF, 0xFF); // Red, hint of blue
            for(i = 0; i < (NUM_LIN_LEDS >> 1); i++)
            {
                leds[i].r = (red >>  0) & 0xFF;
                leds[i].g = (red >>  8) & 0xFF;
                leds[i].b = (red >> 16) & 0xFF;
            }
        }
        else
        {
            uint32_t blue = EHSVtoHEX(180, 0xFF, 0xFF); // Blue, hint of red
            for(i = (NUM_LIN_LEDS >> 1); i < NUM_LIN_LEDS; i++)
            {
                leds[i].r = (blue >>  0) & 0xFF;
                leds[i].g = (blue >>  8) & 0xFF;
                leds[i].b = (blue >> 16) & 0xFF;
            }
        }
    }
    // Output the LED data, actually turning them on
    if(ledsUpdated)
    {
        setDanceLeds(leds, sizeof(leds));
    }
}

/** Turn a random LED on to a random color, one at a time
 *
 * @param tElapsedUs The time elapsed since last call, in microseconds
 * @param reset      true to reset this dance's variables
 */
void ICACHE_FLASH_ATTR dancePureRandom(uint32_t tElapsedUs, uint32_t arg __attribute__((unused)), bool reset)
{
    static uint32_t tAccumulated = 0;
    static uint8_t randLedMask = 0;
    static uint32_t randColor = 0;
    static uint8_t ledVal = 0;
    static bool ledRising = true;
    static uint32_t randInterval = 5000;

    if(reset)
    {
        randInterval = 5000;
        tAccumulated = randInterval;
        randLedMask = 0;
        randColor = 0;
        ledVal = 0;
        ledRising = true;
        return;
    }

    // Declare some LEDs, all off
    led_t leds[NUM_LIN_LEDS] = {{0}};
    bool ledsUpdated = false;

    tAccumulated += tElapsedUs;
    while(tAccumulated >= randInterval)
    {
        tAccumulated -= randInterval;

        if(0 == ledVal)
        {
            randColor = danceRand(256);
            randLedMask = danceRand(1 << NUM_LIN_LEDS);
            randInterval = 500 + danceRand(4096);
            ledVal++;
        }
        else if(ledRising)
        {
            ledVal++;
            if(255 == ledVal)
            {
                ledRising = false;
            }
        }
        else
        {
            ledVal--;
            if(0 == ledVal)
            {
                ledRising = true;
            }
        }

        ledsUpdated = true;
        uint32_t color = EHSVtoHEX(randColor, 0xFF, ledVal);
        for(uint8_t i = 0; i < NUM_LIN_LEDS; i++)
        {
            if((1 << i) & randLedMask)
            {
                leds[i].r = (color >>  0) & 0xFF;
                leds[i].g = (color >>  8) & 0xFF;
                leds[i].b = (color >> 16) & 0xFF;
            }
        }
    }
    // Output the LED data, actually turning them on
    if(ledsUpdated)
    {
        setDanceLeds(leds, sizeof(leds));
    }
}

/**
 * Turn on all LEDs and smooth iterate their singular color around the color wheel
 *
 * @param tElapsedUs The time elapsed since last call, in microseconds
 * @param reset      true to reset this dance's variables
 */
void ICACHE_FLASH_ATTR danceRainbowSolid(uint32_t tElapsedUs, uint32_t arg __attribute__((unused)), bool reset)
{
    static int32_t ledCount = 0;
    static int32_t color_save = 0;
    static uint32_t tAccumulated = 0;

    if(reset)
    {
        ledCount = 0;
        color_save = 0;
        tAccumulated = 70000;
        return;
    }

    // Declare some LEDs, all off
    led_t leds[NUM_LIN_LEDS] = {{0}};
    bool ledsUpdated = false;

    tAccumulated += tElapsedUs;
    while(tAccumulated >= 70000)
    {
        tAccumulated -= 70000;
        ledsUpdated = true;

        ledCount = ledCount + 1;
        if(ledCount > 255)
        {
            ledCount = 0;
        }
        int16_t angle = ledCount % 256;
        color_save = EHSVtoHEX(angle, 0xFF, 0xFF);

        uint8_t i;
        for(i = 0; i < NUM_LIN_LEDS; i++)
        {
            leds[i].r = (color_save >>  0) & 0xFF;
            leds[i].g = (color_save >>  8) & 0xFF;
            leds[i].b = (color_save >> 16) & 0xFF;
        }
    }
    // Output the LED data, actually turning them on
    if(ledsUpdated)
    {
        setDanceLeds(leds, sizeof(leds));
    }
}

/** Called ever 1ms
 * Pick a random dance mode and call it at its period for 4.5s. Then pick
 * another random dance and repeat
 *
 * @param tElapsedUs The time elapsed since last call, in microseconds
 * @param reset      true to reset this dance's variables
 */
void ICACHE_FLASH_ATTR danceRandomDance(uint32_t tElapsedUs, uint32_t arg __attribute__((unused)), bool reset)
{
    static int32_t random_choice = -1;
    static uint32_t tAccumulated = 0;

    if(reset)
    {
        random_choice = -1;
        tAccumulated = 4500000;
        return;
    }

    if(-1 == random_choice)
    {
        random_choice = danceRand(getNumDances() - 2); // exclude the random mode, excluding random & none
    }

    tAccumulated += tElapsedUs;
    while(tAccumulated >= 4500000)
    {
        tAccumulated -= 4500000;
        random_choice = danceRand(getNumDances() - 2); // exclude the random & none mode
        ledDances[random_choice].func(0, ledDances[random_choice].arg, true);
    }

    ledDances[random_choice].func(tElapsedUs, ledDances[random_choice].arg, false);
}

/** Holiday lights. Picks random target hues (red or green) or (blue or yellow) and saturations for
 * random LEDs at random intervals, then smoothly iterates towards those targets.
 * All LEDs are shown with a randomness added to their brightness for a little
 * sparkle
 *
 * @param tElapsedUs The time elapsed since last call, in microseconds
 * @param arg        unused
 * @param reset      true to reset this dance's variables
 */
void ICACHE_FLASH_ATTR danceChristmas(uint32_t tElapsedUs, uint32_t arg, bool reset)
{
    static int32_t ledCount = 0;
    static int32_t ledCount2 = 0;
    static uint8_t color_hue_save[NUM_LIN_LEDS] = {0};
    static uint8_t color_saturation_save[NUM_LIN_LEDS] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    static uint8_t current_color_hue[NUM_LIN_LEDS] = {0};
    static uint8_t current_color_saturation[NUM_LIN_LEDS] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    static uint8_t target_value[NUM_LIN_LEDS] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    static uint8_t current_value[NUM_LIN_LEDS] = {0};

    static uint32_t tAccumulated = 0;
    static uint32_t tAccumulatedValue = 0;

    if(reset)
    {
        ledCount = 0;
        ledCount2 = 0;
        ets_memset(color_saturation_save, 0xFF, sizeof(color_saturation_save));
        ets_memset(current_color_saturation, 0xFF, sizeof(current_color_saturation));
        ets_memset(target_value, 0xFF, sizeof(target_value));
        ets_memset(current_value, 0x00, sizeof(current_value));
        if(arg)
        {
            ets_memset(color_hue_save, 0, sizeof(color_hue_save));
            ets_memset(current_color_hue, 0, sizeof(current_color_hue)); // All red
        }
        else
        {
            ets_memset(color_hue_save, 171, sizeof(color_hue_save));
            ets_memset(current_color_hue, 171, sizeof(current_color_hue)); // All blue
        }
        tAccumulated = 0;
        tAccumulatedValue = 0;
        return;
    }

    // Declare some LEDs, all off
    led_t leds[NUM_LIN_LEDS] = {{0}};
    bool ledsUpdated = false;

    // Run a faster loop for LED brightness updates, this gives a twinkling effect
    tAccumulatedValue += tElapsedUs;
    while(tAccumulatedValue > 3500)
    {
        tAccumulatedValue -= 3500;

        uint8_t i;
        for(i = 0; i < NUM_LIN_LEDS; i++)
        {
            if(current_value[i] == target_value[i])
            {
                if(0xFF == target_value[i])
                {
                    // Reached full bright, pick new target value
                    target_value[i] = danceRand(64) + 192;
                }
                else
                {
                    // Reached target value, reset target to full bright
                    target_value[i] = 0xFF;
                }
            }
            // Smoothly move to the target value
            else if(current_value[i] > target_value[i])
            {
                current_value[i] -= 1;
            }
            else if (current_value[i] < target_value[i])
            {
                current_value[i] += 1;
            }
        }
    }

    // Run a slower loop for hue and saturation updates
    tAccumulated += tElapsedUs;
    while(tAccumulated > 7000)
    {
        tAccumulated -= 7000;

        ledCount += 1;
        if(ledCount > ledCount2)
        {
            ledCount = 0;
            ledCount2 = danceRand(1000) + 50; // 350ms to 7350ms
            int color_picker = danceRand(NUM_LIN_LEDS - 1);
            int node_select = danceRand(NUM_LIN_LEDS);

            if (color_picker < 4)
            {
                // Flip some color targets
                if(arg)
                {
                    if(color_hue_save[node_select] == 0) // red
                    {
                        color_hue_save[node_select] = 86; // green
                    }
                    else
                    {
                        color_hue_save[node_select] = 0; // red
                    }
                }
                else
                {
                    if(color_hue_save[node_select] == 171) // blue
                    {
                        color_hue_save[node_select] = 43; // yellow
                    }
                    else
                    {
                        color_hue_save[node_select] = 171; // blue
                    }
                }
                // Pick a random saturation target
                color_saturation_save[node_select] = danceRand(15) + 240;
            }
            else
            {
                // Whiteish target
                color_saturation_save[node_select] = danceRand(25);
            }
        }

        uint8_t i;
        for(i = 0; i < NUM_LIN_LEDS; i++)
        {
            // Smoothly move hue to the target
            if(current_color_hue[i] > color_hue_save[i])
            {
                current_color_hue[i] -= 1;
            }
            else if (current_color_hue[i] < color_hue_save[i])
            {
                current_color_hue[i] += 1;
            }

            // Smoothly move saturation to the target
            if(current_color_saturation[i] > color_saturation_save[i])
            {
                current_color_saturation[i] -= 1;
            }
            else if (current_color_saturation[i] < color_saturation_save[i])
            {
                current_color_saturation[i] += 1;
            }
        }

        // Calculate actual LED values
        for(i = 0; i < NUM_LIN_LEDS; i++)
        {
            leds[i].r = (EHSVtoHEX(current_color_hue[i], current_color_saturation[i], current_value[i]) >>  0) & 0xFF;
            leds[i].g = (EHSVtoHEX(current_color_hue[i], current_color_saturation[i], current_value[i]) >>  8) & 0xFF;
            leds[i].b = (EHSVtoHEX(current_color_hue[i], current_color_saturation[i], current_value[i]) >> 16) & 0xFF;
        }
        ledsUpdated = true;
    }
    // Output the LED data, actually turning them on
    if(ledsUpdated)
    {
        setDanceLeds(leds, sizeof(leds));
    }
}

/**
 * @brief Blank the LEDs
 *
 * @param tElapsedUs
 * @param arg
 * @param reset
 */
void ICACHE_FLASH_ATTR danceNone(uint32_t tElapsedUs __attribute__((unused)),
                                 uint32_t arg __attribute__((unused)), bool reset)
{
    if(reset)
    {
        led_t leds[NUM_LIN_LEDS] = {{0}};
        setDanceLeds(leds, sizeof(leds));
    }
}
