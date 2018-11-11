/*
 * mode_colorchord.c
 *
 *  Created on: Oct 19, 2018
 *      Author: adam
 */

/*============================================================================
 * Includes
 *==========================================================================*/

#include "user_main.h"
#include "mode_colorchord.h"
#include "DFT32.h"
#include "embeddedout.h"
#include "osapi.h"

/*============================================================================
 * Prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR colorchordEnterMode(void);
void ICACHE_FLASH_ATTR colorchordExitMode(void);
void ICACHE_FLASH_ATTR colorchordSampleHandler(int32_t samp);
void ICACHE_FLASH_ATTR colorchordButtonCallback(uint8_t state __attribute__((unused)),
        int button, int down);
void ICACHE_FLASH_ATTR ccLedOverrideReset(void* timer_arg __attribute__((unused)));

/*============================================================================
 * Variables
 *==========================================================================*/

swadgeMode colorchordMode =
{
    .modeName = "colorchord",
    .fnEnterMode = colorchordEnterMode,
    .fnExitMode = colorchordExitMode,
    .fnTimerCallback = NULL,
    .fnButtonCallback = colorchordButtonCallback,
    .fnAudioCallback = colorchordSampleHandler,
    .wifiMode = SOFT_AP,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
};

static int samplesProcessed = 0;

os_timer_t ccLedOverrideTimer = {0};
bool ccOverrideLeds = false;

/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * Initializer for colorchord
 */
void ICACHE_FLASH_ATTR colorchordEnterMode(void)
{
    InitColorChord();

    ccOverrideLeds = false;

    // Setup the LED override timer, but don't arm it
    ets_memset(&ccLedOverrideTimer, 0, sizeof(os_timer_t));
    os_timer_disarm(&ccLedOverrideTimer);
    os_timer_setfn(&ccLedOverrideTimer, ccLedOverrideReset, NULL);
}

/**
 * Called when colorchord is exited, it disarms the timer
 */
void ICACHE_FLASH_ATTR colorchordExitMode(void)
{
    // Disarm the timer
    os_timer_disarm(&ccLedOverrideTimer);
}

/**
 * This is called every time an audio sample is read from the ADC
 * This processes the sample and will display update the LEDs every
 * 128 samples
 *
 * @param samp A 32 bit audio sample read from the ADC (microphone)
 */
void ICACHE_FLASH_ATTR colorchordSampleHandler(int32_t samp)
{
    //os_printf("%s :: %d\r\n", __func__, __LINE__);
    PushSample32( samp );
    samplesProcessed++;

    // If 128 samples have been processed
    if( samplesProcessed == 128 )
    {
        // Don't bother if colorchord is inactive
        if( !COLORCHORD_ACTIVE )
        {
            return;
        }

        // Colorchord magic
        HandleFrameInfo();

        // Update the LEDs as necessary
        switch( COLORCHORD_OUTPUT_DRIVER )
        {
            case 0:
            {
                UpdateLinearLEDs();
                break;
            }
            case 1:
            {
                UpdateAllSameLEDs();
                break;
            }
        };

        // Push out the LED data
        if(!ccOverrideLeds)
        {
            setLeds( (led_t*)ledOut, USE_NUM_LIN_LEDS * 3 );
        }

        // Reset the sample count
        samplesProcessed = 0;
    }
}

/**
 * Button callback for colorchord. Button 1 adjusts the output LED mode and
 * button 2 adjusts the sensitivity
 *
 * @param state  A bitmask of all button states, unused
 * @param button The button which triggered this event
 * @param down   true if the button was pressed, false if it was released
 */
void ICACHE_FLASH_ATTR colorchordButtonCallback(
    uint8_t state __attribute__((unused)), int button, int down)
{
    if(down)
    {
        // Start a timer to restore LED functionality to colorchord
        ccOverrideLeds = true;
        os_timer_disarm(&ccLedOverrideTimer);
        os_timer_arm(&ccLedOverrideTimer, 1000, false);

        switch(button)
        {
            case 1:
            {
                // gCOLORCHORD_OUTPUT_DRIVER can be either 0 for multiple LED
                // colors or 1 for all the same LED color
                CCS.gCOLORCHORD_OUTPUT_DRIVER =
                    (CCS.gCOLORCHORD_OUTPUT_DRIVER + 1) % 2;

                led_t leds[6] = {{0}};
                if(CCS.gCOLORCHORD_OUTPUT_DRIVER)
                {
                    // All the same LED
                    uint8_t i;
                    for(i = 0; i < 6; i++)
                    {
                        leds[i].r = 0;
                        leds[i].g = 0;
                        leds[i].b = 255;
                    }
                }
                else
                {
                    // Multiple output colors
                    uint8_t i;
                    for(i = 0; i < 6; i++)
                    {
                        uint32_t color = getLedColorPerNumber(i, 0xFF);
                        leds[i].r = (color >>  0) & 0xFF;
                        leds[i].g = (color >>  8) & 0xFF;
                        leds[i].b = (color >> 16) & 0xFF;
                    }
                }
                setLeds(leds, sizeof(leds));
                break;
            }
            case 2:
            {
                // The initial value is 16, so this math gets the amps
                // [0, 8, 16, 24, 32, 40]
                CCS.gINITIAL_AMP = (CCS.gINITIAL_AMP + 8) % 48;

                // Override the LEDs to show the sensitivity, 1-6
                led_t leds[6] = {{0}};
                int i;
                for(i = 0; i < (CCS.gINITIAL_AMP / 8) + 1; i++)
                {
                    leds[(6 - i) % 6].b = 0xFF;
                }
                setLeds(leds, sizeof(leds));
                break;
            }
        }
    }
}

/**
 * This timer function is called 1s after a button press to restore LED
 * functionality to colorchord. If a button is pressed multiple times, the timer
 * will only call after it's idle
 *
 * @param timer_arg unused
 */
void ICACHE_FLASH_ATTR ccLedOverrideReset(void* timer_arg __attribute__((unused)))
{
    ccOverrideLeds = false;
}
