/*
 * morse_code.c
 *
 *  Created on: Dec 6, 2018
 *      Author: adam
 */

/*============================================================================
 * Includes
 *==========================================================================*/

#include "osapi.h"
#include "morse_code.h"
#include "user_main.h"

/*============================================================================
 * Defines
 *==========================================================================*/

#define MORSE_TIME_MS 333

/*============================================================================
 * Variables
 *==========================================================================*/

uint8_t morseStringIdx = 0;
uint8_t morseDotDashIdx = 0;

os_timer_t morseTimerOn = {0};
os_timer_t morseTimerOff = {0};

void (*mFnWhenDone)(void) = NULL;

/*============================================================================
 * Static Const Variables
 *==========================================================================*/

// TODO Replace this with whatever secret string you want
// MUST ONLY CONTAIN ['a'-'z'], ['A'-'Z'], ['0'-'9'] or [' ']
static const char morseString[] = "HELLO WORLD";

// Letter to dot-dash mapping
static const char* alpha[] =
{
    ".-",   //A
    "-...", //B
    "-.-.", //C
    "-..",  //D
    ".",    //E
    "..-.", //F
    "--.",  //G
    "....", //H
    "..",   //I
    ".---", //J
    "-.-",  //K
    ".-..", //L
    "--",   //M
    "-.",   //N
    "---",  //O
    ".--.", //P
    "--.-", //Q
    ".-.",  //R
    "...",  //S
    "-",    //T
    "..-",  //U
    "...-", //V
    ".--",  //W
    "-..-", //X
    "-.--", //Y
    "--..", //Z
};

// Number to dot-dash mapping
static const char* num[] =
{
    "-----", //0
    ".----", //1
    "..---", //2
    "...--", //3
    "....-", //4
    ".....", //5
    "-....", //6
    "--...", //7
    "---..", //8
    "----.", //9
};

/*============================================================================
 * Prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR morseTimerOnCallback(void* timer_arg __attribute__((unused)));
void ICACHE_FLASH_ATTR morseTimerOffCallback(void* timer_arg __attribute__((unused)));

/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * Set up the morse code timers and start blinking
 *
 * @param fnWhenDone A funciton to call once the morse code sequence is finished
 */
void ICACHE_FLASH_ATTR startMorseSequence(void (*fnWhenDone)(void))
{
    // Timer setup
    ets_memset(&morseTimerOn, 0, sizeof(os_timer_t));
    os_timer_disarm(&morseTimerOn);
    os_timer_setfn(&morseTimerOn, morseTimerOnCallback, NULL);

    // Timer setup
    ets_memset(&morseTimerOff, 0, sizeof(os_timer_t));
    os_timer_disarm(&morseTimerOff);
    os_timer_setfn(&morseTimerOff, morseTimerOffCallback, NULL);

    // Variable setup
    mFnWhenDone = fnWhenDone;
    morseStringIdx = 0;
    morseDotDashIdx = 0;

    // Turn LEDs off
    led_t leds[6] = {{0}};
    setLeds(leds, sizeof(leds));

    // Start blinking in 2 * MORSE_TIME_MS
    os_timer_arm(&morseTimerOn, 2 * MORSE_TIME_MS, false);
}

/**
 * Clean up after the morse sequence is done and call the mode's function
 * to return to normal operation
 */
void ICACHE_FLASH_ATTR endMorseSequence(void)
{
    // Kill the morse timers
    os_timer_disarm(&morseTimerOn);
    os_timer_disarm(&morseTimerOff);

    // Call the mode's function to return to normal, if given
    if(NULL != mFnWhenDone)
    {
        mFnWhenDone();
    }
}

/**
 * Timer callback to turn on an LED. If the next thing is a dot or dash, the LED
 * will turn on. If it's a gap between chars or a space, it'll won't turn LEDs
 * on, but will set a timer to call itself again after the proper wait time
 *
 * Morse code rules:
 * 1. The length of a dot is one unit.
 * 2. A dash is three units.
 * 3. The space between parts of the same letter is one unit.
 * 4. The space between letters is three units.
 * 5. The space between words is seven units.
 *
 * @param timer_arg unused
 */
void ICACHE_FLASH_ATTR morseTimerOnCallback(void* timer_arg __attribute__((unused)))
{
    // Get the dots and dashes for the current char
    const char* dotsAndDashes = NULL;
    char morseChar = morseString[morseStringIdx];
    if('a' <= morseChar && morseChar <= 'z')
    {
        dotsAndDashes = alpha[morseChar - 'a'];
    }
    else if('A' <= morseChar && morseChar <= 'Z')
    {
        dotsAndDashes = alpha[morseChar - 'A'];
    }
    else if('0' <= morseChar && morseChar <= '9')
    {
        dotsAndDashes = num[morseChar - '0'];
    }
    else
    {
        // Invalid char somehow
        // Spaces and the end of the string are handled elsewhere
        endMorseSequence();
        return;
    }

    // Dot, dash, or null (end of char's dots & dashes)
    switch(dotsAndDashes[morseDotDashIdx])
    {
        case '.':
        {
            // Turn on the LEDs for a dot
            led_t leds[6] = {{0}};
            uint8_t i;
            for(i = 0; i < 6; i++)
            {
                leds[i].r = 0xFF;
                leds[i].g = 0xFF;
                leds[i].b = 0x00;
            }
            setLeds(leds, sizeof(leds));

            // 1. The length of a dot is one unit.
            os_timer_arm(&morseTimerOff, MORSE_TIME_MS, false);
            morseDotDashIdx++;
            return;
        }
        case '-':
        {
            // Turn on the LEDs for a dash
            led_t leds[6] = {{0}};
            uint8_t i;
            for(i = 0; i < 6; i++)
            {
                leds[i].r = 0xFF;
                leds[i].g = 0x00;
                leds[i].b = 0xFF;
            }
            setLeds(leds, sizeof(leds));

            // 2. A dash is three units.
            os_timer_arm(&morseTimerOff, 3 * MORSE_TIME_MS, false);
            morseDotDashIdx++;
            return;
        }
        case 0: // NULL
        {
            // end of this char, don't turn the LEDs on
            // Reset bit count
            morseDotDashIdx = 0;

            // Peek at the next letter. Spaces between words get more time than
            // between letters in a word
            morseStringIdx++;
            switch(morseString[morseStringIdx])
            {
                case 0: // NULL
                {
                    // End of string
                    endMorseSequence();
                    return;
                }
                case ' ':
                {
                    // 5. The space between words is seven units.
                    morseStringIdx++;
                    // Only wait 6 units b/c one unit was already waited for
                    // after the previous dot/dash
                    os_timer_arm(&morseTimerOn, 6 * MORSE_TIME_MS, false);
                    return;
                }
                default:
                {
                    // 4. The space between letters is three units.
                    // Only wait 2 units b/c one unit was already waited for
                    // after the previous dot/dash
                    os_timer_arm(&morseTimerOn, 2 * MORSE_TIME_MS, false);
                    return;
                }
            }
            return;
        }
        default:
        {
            // Invalid char
            endMorseSequence();
            return;
        }
    }
}

/**
 * Callback to turn off the LEDs after a dot or a dash. Arms a timer to display
 * the next dot or dash
 *
 * @param timer_arg
 */
void ICACHE_FLASH_ATTR morseTimerOffCallback(void* timer_arg __attribute__((unused)))
{
    // Turn off the LEDs
    led_t leds[6] = {{0}};
    setLeds(leds, sizeof(leds));

    // 3. The space between parts of the same letter is one unit.
    // Turn on the next light in MORSE_TIME_MS
    os_timer_arm(&morseTimerOn, MORSE_TIME_MS, false);
}
