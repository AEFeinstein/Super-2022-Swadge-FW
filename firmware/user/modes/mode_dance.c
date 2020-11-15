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

typedef void (*ledDance)(uint32_t, bool);

/*============================================================================
 * Prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR setDanceLeds(led_t* ledData, uint8_t ledDataLen);
uint32_t ICACHE_FLASH_ATTR danceRand(uint32_t upperBound);

void ICACHE_FLASH_ATTR danceRotateWhite(uint32_t tElapsedUs, bool reset);
void ICACHE_FLASH_ATTR danceBlinkRed(uint32_t tElapsedUs, bool reset);
void ICACHE_FLASH_ATTR danceRotateTwoBlue(uint32_t tElapsedUs, bool reset);
void ICACHE_FLASH_ATTR danceSmoothRainbow(uint32_t tElapsedUs, bool reset);
void ICACHE_FLASH_ATTR danceSharpRainbow(uint32_t tElapsedUs, bool reset);
void ICACHE_FLASH_ATTR danceSingleRainbow(uint32_t tElapsedUs, bool reset);
void ICACHE_FLASH_ATTR danceRainbowSolid(uint32_t tElapsedUs, bool reset);
void ICACHE_FLASH_ATTR danceBinaryCounter(uint32_t tElapsedUs, bool reset);
void ICACHE_FLASH_ATTR danceFireRed(uint32_t tElapsedUs, bool reset);
void ICACHE_FLASH_ATTR danceFireGreen(uint32_t tElapsedUs, bool reset);
void ICACHE_FLASH_ATTR danceFireBlue(uint32_t tElapsedUs, bool reset);
void ICACHE_FLASH_ATTR danceRotateOneRandomColor(uint32_t tElapsedUs, bool reset);
void ICACHE_FLASH_ATTR dancePulseRandomColor(uint32_t tElapsedUs, bool reset);
void ICACHE_FLASH_ATTR danceStaticPatterns(uint32_t tElapsedUs, bool reset);
void ICACHE_FLASH_ATTR dancePoliceSiren(uint32_t tElapsedUs, bool reset);
void ICACHE_FLASH_ATTR dancePureRandom(uint32_t tElapsedUs, bool reset);
void ICACHE_FLASH_ATTR danceChristmas(uint32_t tElapsedUs, bool reset);
void ICACHE_FLASH_ATTR danceChainsaw(uint32_t tElapsedUs, bool reset);
void ICACHE_FLASH_ATTR danceRandomDance(uint32_t tElapsedUs, bool reset);

/*============================================================================
 * Variables
 *==========================================================================*/

static const uint8_t danceBrightnesses[] =
{
    0x01,
    0x08,
    0x40,
};

static const ledDance ledDances[] =
{
    danceRotateWhite,
    danceBlinkRed,
    danceRotateTwoBlue,
    danceSmoothRainbow,
    danceSharpRainbow,
    danceSingleRainbow,
    danceRainbowSolid,
    danceBinaryCounter,
    danceFireRed,
    danceFireGreen,
    danceFireBlue,
    danceRotateOneRandomColor,
    dancePulseRandomColor,
    danceStaticPatterns,
    dancePoliceSiren,
    dancePureRandom,
    danceChristmas,
    danceChainsaw,
    danceRandomDance
};

uint8_t danceBrightnessIdx = 0;

/*============================================================================
 * Functions
 *==========================================================================*/

/** @return The number of different tances
 */
uint8_t ICACHE_FLASH_ATTR getNumDances(void)
{
    return (sizeof(ledDances) / sizeof(ledDances[0]));
}

/** This is called to clear all dance variables
 */
void ICACHE_FLASH_ATTR danceClearVars(void)
{
    // Reset all dances
    for(uint8_t i = 0; i < getNumDances(); i++)
    {
        ledDances[i](0, true);
    }
}

/** Set the brightness index
 *
 * @param brightness index into danceBrightnesses[]
 */
void ICACHE_FLASH_ATTR setDanceBrightness(uint8_t brightness)
{
    if(brightness > 2)
    {
        brightness = 2;
    }
    danceBrightnessIdx = brightness;
}

/** Intermediate function which adjusts brightness and sets the LEDs
 *
 * @param ledData    The LEDs to be scaled, then set
 * @param ledDataLen The length of the LEDs to set
 */
void ICACHE_FLASH_ATTR setDanceLeds(led_t* ledData, uint8_t ledDataLen)
{
    uint8_t i;
    for(i = 0; i < ledDataLen / sizeof(led_t); i++)
    {
        ledData[i].r = ledData[i].r / danceBrightnesses[danceBrightnessIdx];
        ledData[i].g = ledData[i].g / danceBrightnesses[danceBrightnessIdx];
        ledData[i].b = ledData[i].b / danceBrightnesses[danceBrightnessIdx];
    }
    setLeds(ledData, ledDataLen);
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
        ledDances[danceIdx](tElapsedUs, false);
    }
}

/** This animation is set to be called every 100 ms
 * Rotate a single white LED around the hexagon
 *
 * @param tElapsedUs The time elapsed since last call, in microseconds
 * @param reset      true to reset this dance's variables
 */
void ICACHE_FLASH_ATTR danceRotateWhite(uint32_t tElapsedUs, bool reset)
{
    static int32_t ledCount = 0;
    static uint32_t tAccumulated = 0;

    if(reset)
    {
        ledCount = 0;
        tAccumulated = 0;
        return;
    }

    // Declare some LEDs, all off
    led_t leds[NUM_LIN_LEDS] = {{0}};
    bool ledsUpdated = false;

    tAccumulated += tElapsedUs;
    while(tAccumulated > 100000)
    {
        tAccumulated -= 100000;
        ledsUpdated = true;

        // Skip to the next LED around the hexagon
        ledCount = ledCount + 1;
        if(ledCount > NUM_LIN_LEDS)
        {
            ledCount = 0;
        }

        // Turn the current LED on, full bright white
        leds[ledCount].r = 255;
        leds[ledCount].g = 255;
        leds[ledCount].b = 255;
    }
    // Output the LED data, actually turning them on
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
void ICACHE_FLASH_ATTR danceBlinkRed(uint32_t tElapsedUs, bool reset)
{
    static int32_t ledCount = 0;
    static uint32_t tAccumulated = 0;

    if(reset)
    {
        ledCount = 0;
        tAccumulated = 0;
        return;
    }

    // Declare some LEDs, all off
    led_t leds[NUM_LIN_LEDS] = {{0}};
    bool ledsUpdated = false;

    tAccumulated += tElapsedUs;
    while(tAccumulated > 100000)
    {
        tAccumulated -= 100000;
        ledsUpdated = true;

        ledCount = ledCount + 1;
        if(ledCount < 5)
        {
            // full bright red
            for (int i = 0; i < NUM_LIN_LEDS; i++)
            {
                leds[i].r = 200;
            }
        }
        else
        {
            // off
            for (int i = 0; i < NUM_LIN_LEDS; i++)
            {
                leds[i].r = 0;
            }
        }

        if(ledCount > 10)
        {
            ledCount = 0;
        }
    }
    if(ledsUpdated)
    {
        setDanceLeds(leds, sizeof(leds));
    }
}

/** This animation is set to be called every 100ms
 * Rotates two blue LEDs, one clockwise and one counterclockwise
 *
 * @param tElapsedUs The time elapsed since last call, in microseconds
 * @param reset      true to reset this dance's variables
 */
void ICACHE_FLASH_ATTR danceRotateTwoBlue(uint32_t tElapsedUs, bool reset)
{
    static int32_t ledCount = 0;
    static uint32_t tAccumulated = 0;

    if(reset)
    {
        ledCount = 0;
        tAccumulated = 0;
        return;
    }

    // Declare some LEDs, all off
    led_t leds[NUM_LIN_LEDS] = {{0}};
    bool ledsUpdated = false;

    tAccumulated += tElapsedUs;
    while(tAccumulated > 100000)
    {
        tAccumulated -= 100000;
        ledsUpdated = true;

        // Skip to the next LED around the hexagon
        ledCount = ledCount - 1;
        if(ledCount < 0 || ledCount > NUM_LIN_LEDS - 1)
        {
            ledCount = NUM_LIN_LEDS - 1;
        }

        // Turn the current LED on, full bright blue
        leds[ledCount].r = 0;
        leds[ledCount].g = 0;
        leds[ledCount].b = 255;

        leds[(NUM_LIN_LEDS - ledCount ) % NUM_LIN_LEDS].r = 0;
        leds[(NUM_LIN_LEDS - ledCount ) % NUM_LIN_LEDS].g = 0;
        leds[(NUM_LIN_LEDS - ledCount ) % NUM_LIN_LEDS].b = 255;
    }
    // Output the LED data, actually turning them on
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
void ICACHE_FLASH_ATTR danceSmoothRainbow(uint32_t tElapsedUs, bool reset)
{
    static uint32_t tAccumulated = 0;
    static int32_t ledCount = 0;

    if(reset)
    {
        ledCount = 0;
        tAccumulated = 0;
        return;
    }

    // Declare some LEDs, all off
    led_t leds[NUM_LIN_LEDS] = {{0}};
    bool ledsUpdated = false;

    tAccumulated += tElapsedUs;
    while(tAccumulated > 5000)
    {
        tAccumulated -= 5000;
        ledsUpdated = true;

        ledCount = ledCount + 1;
        if(ledCount > 256)
        {
            ledCount = 0;
        }

        uint8_t i;
        for(i = 0; i < NUM_LIN_LEDS; i++)
        {
            int16_t angle = (((i * 256) / NUM_LIN_LEDS)) + ledCount % 256;
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
void ICACHE_FLASH_ATTR danceSharpRainbow(uint32_t tElapsedUs, bool reset)
{
    static int32_t ledCount = 0;
    static uint32_t tAccumulated = 0;

    if(reset)
    {
        ledCount = 0;
        tAccumulated = 0;
        return;
    }

    // Declare some LEDs, all off
    led_t leds[NUM_LIN_LEDS] = {{0}};
    bool ledsUpdated = false;

    tAccumulated += tElapsedUs;
    while(tAccumulated > 100000)
    {
        tAccumulated -= 100000;
        ledsUpdated = true;

        ledCount = ledCount + 1;
        if(ledCount > NUM_LIN_LEDS - 1)
        {
            ledCount = 0;
        }

        uint8_t i;
        for(i = 0; i < NUM_LIN_LEDS; i++)
        {
            int16_t angle = (((i * 256)  / NUM_LIN_LEDS)) % 256;
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

/** Rotate a single LED around the hexagon while smoothy iterating its color
 * around the color wheel
 *
 * @param tElapsedUs The time elapsed since last call, in microseconds
 * @param reset      true to reset this dance's variables
 */
void ICACHE_FLASH_ATTR danceSingleRainbow(uint32_t tElapsedUs, bool reset)
{
    static int32_t ledCount = 0;
    static uint32_t tAccumulated = 0;

    if(reset)
    {
        ledCount = 0;
        tAccumulated = 0;
        return;
    }

    // Declare some LEDs, all off
    led_t leds[NUM_LIN_LEDS] = {{0}};
    bool ledsUpdated = false;

    tAccumulated += tElapsedUs;
    while(tAccumulated > 80000)
    {
        tAccumulated -= 80000;
        ledsUpdated = true;

        ledCount = ledCount - 1;
        if(ledCount < 0 || ledCount > 255)
        {
            ledCount = 255;
        }
        int16_t angle = ledCount % 256;
        uint32_t color = EHSVtoHEX(angle, 0xFF, 0xFF);

        leds[(ledCount) % NUM_LIN_LEDS].r = (color >>  0) & 0xFF;
        leds[(ledCount) % NUM_LIN_LEDS].g = (color >>  8) & 0xFF;
        leds[(ledCount) % NUM_LIN_LEDS].b = (color >> 16) & 0xFF;
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
void ICACHE_FLASH_ATTR danceBinaryCounter(uint32_t tElapsedUs, bool reset)
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
        tAccumulated = 0;
        return;
    }

    // Declare some LEDs, all off
    led_t leds[NUM_LIN_LEDS] = {{0}};
    bool ledsUpdated = false;

    tAccumulated += tElapsedUs;
    while(tAccumulated > 300000)
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
void ICACHE_FLASH_ATTR danceFireRed(uint32_t tElapsedUs, bool reset)
{
    static uint32_t tAccumulated = 0;

    if(reset)
    {
        tAccumulated = 0;
        return;
    }

    // Declare some LEDs, all off
    led_t leds[NUM_LIN_LEDS] = {{0}};
    bool ledsUpdated = false;

    tAccumulated += tElapsedUs;
    while(tAccumulated > 100000)
    {
        tAccumulated -= 100000;
        ledsUpdated = true;

        leds[3].r = danceRand(120) + 135;
        leds[3].g = leds[3].r / 5;

        leds[4].r = danceRand(80) + 80;
        leds[4].g = leds[4].r / 5;
        leds[2].r = danceRand(80) + 80;
        leds[2].g = leds[2].r / 5;

        leds[5].r = danceRand(50) + 40;
        leds[5].g = leds[5].r / 5;
        leds[1].r = danceRand(50) + 40;
        leds[1].g = leds[1].r / 5;

        leds[0].r = danceRand(10) + 10;
        leds[0].g = leds[0].r / 5;
    }
    if(ledsUpdated)
    {
        setDanceLeds(leds, sizeof(leds));
    }
}

/**
 * Fire pattern. All LEDs are random amount of green, and fifth that of blue.
 * The LEDs towards the bottom have a brighter base and more randomness. The
 * LEDs towards the top are dimmer and have less randomness.
 *
 * @param tElapsedUs The time elapsed since last call, in microseconds
 * @param reset      true to reset this dance's variables
 */
void ICACHE_FLASH_ATTR danceFireGreen(uint32_t tElapsedUs, bool reset)
{
    static uint32_t tAccumulated = 0;

    if(reset)
    {
        tAccumulated = 0;
        return;
    }

    // Declare some LEDs, all off
    led_t leds[NUM_LIN_LEDS] = {{0}};
    bool ledsUpdated = false;

    tAccumulated += tElapsedUs;
    while(tAccumulated > 100000)
    {
        tAccumulated -= 100000;
        ledsUpdated = true;

        leds[3].g = danceRand(120) + 135;
        leds[3].b = leds[3].g / 5;

        leds[4].g = danceRand(80) + 80;
        leds[4].b = leds[4].g / 5;
        leds[2].g = danceRand(80) + 80;
        leds[2].b = leds[2].g / 5;

        leds[5].g = danceRand(50) + 40;
        leds[5].b = leds[5].g / 5;
        leds[1].g = danceRand(50) + 40;
        leds[1].b = leds[1].g / 5;

        leds[0].g = danceRand(10) + 10;
        leds[0].b = leds[0].g / 5;
    }
    if(ledsUpdated)
    {
        setDanceLeds(leds, sizeof(leds));
    }
}

/**
 * Fire pattern. All LEDs are random amount of blue, and fifth that of green.
 * The LEDs towards the bottom have a brighter base and more randomness. The
 * LEDs towards the top are dimmer and have less randomness.
 *
 * @param tElapsedUs The time elapsed since last call, in microseconds
 * @param reset      true to reset this dance's variables
 */
void ICACHE_FLASH_ATTR danceFireBlue(uint32_t tElapsedUs, bool reset)
{
    static uint32_t tAccumulated = 0;

    if(reset)
    {
        tAccumulated = 0;
        return;
    }

    // Declare some LEDs, all off
    led_t leds[NUM_LIN_LEDS] = {{0}};
    bool ledsUpdated = false;

    tAccumulated += tElapsedUs;
    while(tAccumulated > 100000)
    {
        tAccumulated -= 100000;
        ledsUpdated = true;

        leds[3].b = danceRand(120) + 135;
        leds[3].g = leds[3].b / 5;

        leds[4].b = danceRand(80) + 80;
        leds[4].g = leds[4].b / 5;
        leds[2].b = danceRand(80) + 80;
        leds[2].g = leds[2].b / 5;

        leds[5].b = danceRand(50) + 40;
        leds[5].g = leds[5].b / 5;
        leds[1].b = danceRand(50) + 40;
        leds[1].g = leds[1].b / 5;

        leds[0].b = danceRand(10) + 10;
        leds[0].g = leds[0].b / 5;
    }
    if(ledsUpdated)
    {
        setDanceLeds(leds, sizeof(leds));
    }
}

/** This animation is set to be called every 100 ms
 * Rotate a single LED around the hexagon, giving it a new random color for each
 * rotation
 *
 * @param tElapsedUs The time elapsed since last call, in microseconds
 * @param reset      true to reset this dance's variables
 */
void ICACHE_FLASH_ATTR danceRotateOneRandomColor(uint32_t tElapsedUs, bool reset)
{
    static int32_t ledCount = 0;
    static int32_t color_save = 0;
    static uint32_t tAccumulated = 0;

    if(reset)
    {
        ledCount = 0;
        color_save = 0;
        tAccumulated = 0;
        return;
    }

    // Declare some LEDs, all off
    led_t leds[NUM_LIN_LEDS] = {{0}};
    bool ledsUpdated = false;

    tAccumulated += tElapsedUs;
    while(tAccumulated > 100000)
    {
        tAccumulated -= 100000;
        ledsUpdated = true;

        // Skip to the next LED around the hexagon
        ledCount = ledCount + 1;
        if(ledCount > NUM_LIN_LEDS - 1)
        {
            ledCount = 0;
            color_save = EHSVtoHEX(danceRand(256), 0xFF, 0xFF);
        }

        // Turn the current LED on, full bright white
        leds[ledCount].r = (color_save >>  0) & 0xFF;
        leds[ledCount].g = (color_save >>  8) & 0xFF;
        leds[ledCount].b = (color_save >> 16) & 0xFF;
    }
    // Output the LED data, actually turning them on
    if(ledsUpdated)
    {
        setDanceLeds(leds, sizeof(leds));
    }
}

/** Pulse all LEDs smoothly on and off. For each pulse, pick a random color
 *
 * @param tElapsedUs The time elapsed since last call, in microseconds
 * @param reset      true to reset this dance's variables
 */
void ICACHE_FLASH_ATTR dancePulseRandomColor(uint32_t tElapsedUs, bool reset)
{
    static int32_t ledCount = 0;
    static int32_t ledCount2 = 0;
    static int32_t color_save = 0;
    static uint32_t tAccumulated = 0;

    if(reset)
    {
        ledCount = 0;
        ledCount2 = 0;
        color_save = 0;
        tAccumulated = 0;
        return;
    }

    // Declare some LEDs, all off
    led_t leds[NUM_LIN_LEDS] = {{0}};
    bool ledsUpdated = false;

    tAccumulated += tElapsedUs;
    while(tAccumulated > 2000)
    {
        tAccumulated -= 2000;
        ledsUpdated = true;

        // Skip to the next LED around the hexagon
        ledCount = ledCount + 1;

        if(ledCount > 510)
        {
            ledCount = 0;
            ledCount2 = danceRand(256);
        }
        int intensity = ledCount;
        if(ledCount > 255)
        {
            intensity = 510 - ledCount;
        }
        color_save = EHSVtoHEX(ledCount2, 0xFF, intensity);
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

/** Show a static pattern for 30s, then show another static pattern for 30s
 *
 * @param tElapsedUs The time elapsed since last call, in microseconds
 * @param reset      true to reset this dance's variables
 */
void ICACHE_FLASH_ATTR danceStaticPatterns(uint32_t tElapsedUs, bool reset)
{
    static int32_t timerCount = 0;
    static uint32_t tAccumulated = 0;

    if(reset)
    {
        timerCount = 0;
        tAccumulated = 0;
        return;
    }

    // Declare some LEDs, all off
    led_t leds[NUM_LIN_LEDS] = {{0}};
    bool ledsUpdated = false;

    tAccumulated += tElapsedUs;
    while(tAccumulated > 100000)
    {
        tAccumulated -= 100000;
        ledsUpdated = true;

        timerCount++;
        if (timerCount > 600)
        {
            timerCount = 0;
        }

        if (timerCount < 300)
        {
            // Turn the current LED on GREEN
            leds[0].r = 13;
            leds[0].g = 255;
            leds[0].b = 32;

            leds[1].r = 40;
            leds[1].g = 80;
            leds[1].b = 50;

            leds[2].r = 13;
            leds[2].g = 255;
            leds[2].b = 32;

            leds[3].r = 152;
            leds[3].g = 113;
            leds[3].b = 20;

            leds[4].r = 13;
            leds[4].g = 255;
            leds[4].b = 32;

            leds[5].r = 40;
            leds[5].g = 80;
            leds[5].b = 50;
        }
        else
        {
            // Turn the current LED on RED
            leds[0].r = 255;
            leds[0].g = 32;
            leds[0].b = 32;

            leds[1].r = 80;
            leds[1].g = 50;
            leds[1].b = 50;

            leds[2].r = 255;
            leds[2].g = 32;
            leds[2].b = 32;

            leds[3].r = 152;
            leds[3].g = 113;
            leds[3].b = 20;

            leds[4].r = 255;
            leds[4].g = 32;
            leds[4].b = 32;

            leds[5].r = 80;
            leds[5].g = 50;
            leds[5].b = 50;
        }
    }
    // Output the LED data, actually turning them on
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
void ICACHE_FLASH_ATTR dancePoliceSiren(uint32_t tElapsedUs, bool reset)
{
    static int32_t ledCount;
    static uint32_t tAccumulated = 0;

    if(reset)
    {
        ledCount = 0;
        tAccumulated = 0;
        return;
    }

    // Declare some LEDs, all off
    led_t leds[NUM_LIN_LEDS] = {{0}};
    bool ledsUpdated = false;

    tAccumulated += tElapsedUs;
    while(tAccumulated > 120000)
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
void ICACHE_FLASH_ATTR dancePureRandom(uint32_t tElapsedUs, bool reset)
{
    static uint32_t tAccumulated = 0;

    if(reset)
    {
        tAccumulated = 0;
        return;
    }

    // Declare some LEDs, all off
    led_t leds[NUM_LIN_LEDS] = {{0}};
    bool ledsUpdated = false;

    tAccumulated += tElapsedUs;
    while(tAccumulated > 100000)
    {
        tAccumulated -= 100000;
        ledsUpdated = true;
        uint32_t rand_color = EHSVtoHEX(danceRand(255), 0xFF, 0xFF);
        int rand_light = danceRand(NUM_LIN_LEDS);
        leds[rand_light].r = (rand_color >>  0) & 0xFF;
        leds[rand_light].g = (rand_color >>  8) & 0xFF;
        leds[rand_light].b = (rand_color >> 16) & 0xFF;
    }
    // Output the LED data, actually turning them on
    if(ledsUpdated)
    {
        setDanceLeds(leds, sizeof(leds));
    }
}

/** Christmas lights. Picks random target hues (red or green) and saturations for
 * random LEDs at random intervals, then smoothly iterates towards those targets.
 * All LEDs are shown with a randomness added to their brightness for a little
 * sparkle
 *
 * @param tElapsedUs The time elapsed since last call, in microseconds
 * @param reset      true to reset this dance's variables
 */
void ICACHE_FLASH_ATTR danceChristmas(uint32_t tElapsedUs, bool reset)
{
    static int32_t ledCount = 0;
    static int32_t ledCount2 = 0;
    static uint8_t color_hue_save[NUM_LIN_LEDS] = {0};
    static uint8_t color_saturation_save[NUM_LIN_LEDS] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    static uint8_t current_color_hue[NUM_LIN_LEDS] = {0};
    static uint8_t current_color_saturation[NUM_LIN_LEDS] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    static uint32_t tAccumulated = 0;

    if(reset)
    {
        ledCount = 0;
        ledCount2 = 0;
        ets_memset(color_hue_save, 0, sizeof(color_hue_save));
        ets_memset(color_saturation_save, 0xFF, sizeof(color_hue_save));
        ets_memset(current_color_hue, 0, sizeof(color_hue_save));
        ets_memset(current_color_saturation, 0xFF, sizeof(color_hue_save));
        tAccumulated = 0;
        return;
    }

    // Declare some LEDs, all off
    led_t leds[NUM_LIN_LEDS] = {{0}};
    bool ledsUpdated = false;

    tAccumulated += tElapsedUs;
    while(tAccumulated > 7000)
    {
        tAccumulated -= 7000;
        ledsUpdated = true;

        ledCount += 1;
        if(ledCount > ledCount2)
        {
            ledCount = 0;
            ledCount2 = danceRand(1000) + 50; // 350ms to 7350ms
            int color_picker = danceRand(NUM_LIN_LEDS - 1);
            int node_select = danceRand(NUM_LIN_LEDS);

            if(color_picker < 2)
            {
                color_hue_save[node_select] = 0;
                color_saturation_save[node_select] = danceRand(15) + 240;
            }
            else if (color_picker < 4)
            {
                color_hue_save[node_select] = 86;
                color_saturation_save[node_select] = danceRand(15) + 240;
            }
            else
            {
                color_saturation_save[node_select] = danceRand(25);
            }
        }

        uint8_t i;
        for(i = 0; i < NUM_LIN_LEDS; i++)
        {
            if(current_color_hue[i] > color_hue_save[i])
            {
                current_color_hue[i] -= 1;
            }
            else if (current_color_hue[i] < color_hue_save[i])
            {
                current_color_hue[i] += 1;
            }

            if(current_color_saturation[i] > color_saturation_save[i])
            {
                current_color_saturation[i] -= 1;
            }
            else if (current_color_saturation[i] < color_saturation_save[i])
            {
                current_color_saturation[i] += 1;
            }
        }

        for(i = 0; i < NUM_LIN_LEDS; i++)
        {
            leds[i].r = (EHSVtoHEX(current_color_hue[i],  current_color_saturation[i], danceRand(15) + 240) >>  0) & 0xFF;
            leds[i].g = (EHSVtoHEX(current_color_hue[i],  current_color_saturation[i], danceRand(15) + 240) >>  8) & 0xFF;
            leds[i].b = (EHSVtoHEX(current_color_hue[i],  current_color_saturation[i], danceRand(15) + 240) >> 16) & 0xFF;
        }
    }
    // Output the LED data, actually turning them on
    if(ledsUpdated)
    {
        setDanceLeds(leds, sizeof(leds));
    }
}

/**
 * Chainsaw blade. Use EHSVtoHEX() to smoothly iterate, but only draw red
 *
 * @param tElapsedUs The time elapsed since last call, in microseconds
 * @param reset      true to reset this dance's variables
 */
void ICACHE_FLASH_ATTR danceChainsaw(uint32_t tElapsedUs, bool reset)
{
    static int32_t ledCount = 0;
    static uint32_t tAccumulated = 0;

    if(reset)
    {
        ledCount = 0;
        tAccumulated = 0;
        return;
    }

    // Declare some LEDs, all off
    led_t leds[NUM_LIN_LEDS] = {{0}};
    bool ledsUpdated = false;

    tAccumulated += tElapsedUs;
    while(tAccumulated > 10000)
    {
        tAccumulated -= 10000;
        ledsUpdated = true;

        ledCount = ledCount + 2;
        if(ledCount > 256)
        {
            ledCount = 0;
        }

        uint8_t i;
        for(i = 0; i < NUM_LIN_LEDS; i++)
        {
            int16_t angle = 256 - (((i * 256) / NUM_LIN_LEDS)) + ledCount % 256;
            uint32_t color = EHSVtoHEX(angle, 0xFF, 0xFF);
            leds[i].r = (color >>  0) & 0xFF;
        }
    }
    // Output the LED data, actually turning them on
    if(ledsUpdated)
    {
        setDanceLeds(leds, sizeof(leds));
    }
}

/** Turn on all LEDs and smooth iterate their singular color around the color wheel
 * Note, called the 7th but comes after danceChristmas(uint32_t tElapsedUs, bool reset). Must come before
 * freeze_color()
 *
 * @param tElapsedUs The time elapsed since last call, in microseconds
 * @param reset      true to reset this dance's variables
 */
void ICACHE_FLASH_ATTR danceRainbowSolid(uint32_t tElapsedUs, bool reset)
{
    static int32_t ledCount = 0;
    static int32_t color_save = 0;
    static uint32_t tAccumulated = 0;

    if(reset)
    {
        ledCount = 0;
        color_save = 0;
        tAccumulated = 0;
        return;
    }

    // Declare some LEDs, all off
    led_t leds[NUM_LIN_LEDS] = {{0}};
    bool ledsUpdated = false;

    tAccumulated += tElapsedUs;
    while(tAccumulated > 70000)
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
void ICACHE_FLASH_ATTR danceRandomDance(uint32_t tElapsedUs, bool reset)
{
    static int32_t random_choice = -1;
    static uint32_t tAccumulated = 0;

    if(reset)
    {
        random_choice = -1;
        tAccumulated = 0;
        return;
    }

    if(-1 == random_choice)
    {
        random_choice = danceRand(getNumDances() - 1); // exclude the random mode
    }

    tAccumulated += tElapsedUs;
    while(tAccumulated > 4500000)
    {
        tAccumulated -= 4500000;
        random_choice = danceRand(getNumDances() - 1); // exclude the random mode
        ledDances[random_choice](0, true);
    }

    ledDances[random_choice](tElapsedUs, false);
}
