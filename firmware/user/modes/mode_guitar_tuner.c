/*
 * mode_guitartuner.c
 *
 *  Created on: Oct 19, 2018
 *      Author: adam
 */

/*============================================================================
 * Includes
 *==========================================================================*/

#include <limits.h>
#include "user_main.h"
#include "mode_guitar_tuner.h"
#include "DFT32.h"
#include "embeddedout.h"
#include "osapi.h"

/*============================================================================
 * Defines
 *==========================================================================*/

#define NUM_STRINGS 6
#define OFS         0
#define SENSITIVITY 5 // TODO Change sensitivity here, Adam.

/// Helper macro to return an integer clamped within a range (MIN to MAX)
#define CLAMP(X, MIN, MAX) ( ((X) > (MAX)) ? (MAX) : ( ((X) < (MIN)) ? (MIN) : (X)) )
/// Helper macro to return the absolute value of an integer
#define ABS(X) (((X) < 0) ? -(X) : (X))

/*============================================================================
 * Prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR guitarTunerEnterMode(void);
void ICACHE_FLASH_ATTR guitarTunerExitMode(void);
void ICACHE_FLASH_ATTR guitarTunerSampleHandler(int32_t samp);
void ICACHE_FLASH_ATTR guitarTunerButtonCallback(uint8_t state __attribute__((unused)),
        int button, int down);
void ICACHE_FLASH_ATTR guitarLedOverrideReset(void* timer_arg __attribute__((unused)));
inline int16_t getMagnitude(uint16_t idx);
inline int16_t getDiffAround(uint16_t idx);

/*============================================================================
 * Variables
 *==========================================================================*/

swadgeMode guitarTunerMode =
{
    .modeName = "guitarTuner",
    .fnEnterMode = guitarTunerEnterMode,
    .fnExitMode = guitarTunerExitMode,
    .fnTimerCallback = NULL,
    .fnButtonCallback = guitarTunerButtonCallback,
    .fnAudioCallback = guitarTunerSampleHandler,
    .wifiMode = SOFT_AP,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
};

static int guitarSamplesProcessed = 0;
static uint32_t intensities_filt[NUM_STRINGS] = {0};
static int32_t diffs_filt[NUM_STRINGS] = {0};

os_timer_t guitarLedOverrideTimer = {0};
bool guitarTunerOverrideLeds = false;

/**
 * Indicies into fuzzed_bins[], a realtime DFT of sorts
 * fuzzed_bins[0] = A ... 1/2 steps are every 2.
 */
const uint16_t freqBinIdxs[NUM_STRINGS] =
{
    38, // E string needs to skip an octave... Can't read sounds this low.
    62, // e
    52, // B
    44, // G
    34, // D = A + 5 half steps = 34
    24  // A string is exactly at note #24
};

/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * Initializer for guitar tuner
 */
void ICACHE_FLASH_ATTR guitarTunerEnterMode(void)
{
    // Piggyback on colorchord's DSP
    InitColorChord();

    guitarTunerOverrideLeds = false;

    // Setup the LED override timer, but don't arm it
    ets_memset(&guitarLedOverrideTimer, 0, sizeof(os_timer_t));
    os_timer_disarm(&guitarLedOverrideTimer);
    os_timer_setfn(&guitarLedOverrideTimer, guitarLedOverrideReset, NULL);
}

/**
 * Called when guitar tuner is exited, it disarms the timer
 */
void ICACHE_FLASH_ATTR guitarTunerExitMode(void)
{
    // Disarm the timer
    os_timer_disarm(&guitarLedOverrideTimer);
}

/**
 * Inline helper function to get the magnitude of a frequency bin from fuzzed_bins[]
 *
 * @param idx The index to get a magnitude from
 * @return A signed magnitude, even though fuzzed_bins[] is unsigned
 */
inline int16_t getMagnitude(uint16_t idx)
{
    return fuzzed_bins[idx];
}

/**
 * Inline helper function to get the difference in magnitudes around a given
 * frequency bin from fuzzed_bins[]
 *
 * @param idx The index to get a difference in magnitudes around
 * @return The difference in magnitudes of the bins before and after the given index
 */
inline int16_t getDiffAround(uint16_t idx)
{
    return getMagnitude(idx + 1) - getMagnitude(idx - 1);
}

/**
 * This is called every time an audio sample is read from the ADC
 * This processes the sample and will display update the LEDs every
 * 128 samples
 *
 * @param samp A 32 bit audio sample read from the ADC (microphone)
 */
void ICACHE_FLASH_ATTR guitarTunerSampleHandler(int32_t samp)
{
    //os_printf("%s :: %d\r\n", __func__, __LINE__);
    PushSample32( samp );
    guitarSamplesProcessed++;

    // If 128 samples have been processed
    if( guitarSamplesProcessed == 128 )
    {
        // Don't bother if colorchord is inactive
        if( !COLORCHORD_ACTIVE )
        {
            return;
        }

        // Colorchord magic
        HandleFrameInfo();

        // Guitar tuner magic
        led_t colors[NUM_STRINGS] = {{0}};
        uint32_t i;
        for( i = 0; i < NUM_STRINGS; i++ )
        {
            // Pick out the current magnitude and filter it
            intensities_filt[i] = (getMagnitude(freqBinIdxs[i] + OFS)  + intensities_filt[i]) - (intensities_filt[i] >> 5);

            // Pick out the difference around current magnitude and filter it too
            diffs_filt[i] =       (getDiffAround(freqBinIdxs[i] + OFS) + diffs_filt[i])       - (diffs_filt[i] >> 5);

            // This is the magnitude of the target frequency bin, cleaned up
            int16_t intensity = (intensities_filt[i] >> SENSITIVITY) - 40; // drop a baseline.
            intensity = CLAMP(intensity, 0, 255);

            //This is the tonal difference.  You "calibrate" out the intensity.
            int16_t tonalDiff = (diffs_filt[i] >> SENSITIVITY) * 200 / (intensity + 1);

            int32_t red, grn, blu;
            // Is the note in tune, i.e. is the magnitude difference in surrounding bins small?
            if( (ABS(tonalDiff) < 10) )
            {
                // Note is in tune, make it white
                red = 255;
                grn = 255;
                blu = 255;
            }
            else
            {
                // Check if the note is sharp or flat
                if( tonalDiff > 0 )
                {
                    // Note too sharp, make it red
                    red = 255;
                    grn = blu = 255 - (tonalDiff) * 15;
                }
                else
                {
                    // Note too flat, make it blue
                    blu = 255;
                    grn = red = 255 - (-tonalDiff) * 15;
                }

                // Make sure LED output isn't more than 255
                red = CLAMP(red, INT_MIN, 255);
                grn = CLAMP(grn, INT_MIN, 255);
                blu = CLAMP(blu, INT_MIN, 255);
            }

            // Scale each LED's brightness by the filtered intensity for that bin
            red = (red >> 3 ) * ( intensity >> 3);
            grn = (grn >> 3 ) * ( intensity >> 3);
            blu = (blu >> 3 ) * ( intensity >> 3);

            // Set the LED, ensure each channel is between 0 and 255
            colors[i].r = CLAMP(red, 0, 255);
            colors[i].g = CLAMP(grn, 0, 255);
            colors[i].b = CLAMP(blu, 0, 255);
        }

        // If the LEDs aren't overridden
        if( !guitarTunerOverrideLeds )
        {
            // Draw the LEDs
            setLeds( colors, sizeof(colors) );
        }

        // Reset the sample count
        guitarSamplesProcessed = 0;
    }
}

/**
 * Button callback for guitar mode. TODO button 2 adjusts the sensitivity
 *
 * @param state  A bitmask of all button states, unused
 * @param button The button which triggered this event
 * @param down   true if the button was pressed, false if it was released
 */
void ICACHE_FLASH_ATTR guitarTunerButtonCallback(
    uint8_t state __attribute__((unused)), int button __attribute__((unused)), int down)
{

    if(down)
    {
        // Start a timer to restore LED functionality to colorchord
        guitarTunerOverrideLeds = true;
        os_timer_disarm(&guitarLedOverrideTimer);
        os_timer_arm(&guitarLedOverrideTimer, 1000, false);
#if 0
        switch(button)
        {
            case 1:
            {
                // Do nothing
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
#endif
    }
}

/**
 * This timer function is called 1s after a button press to restore LED
 * functionality to colorchord. If a button is pressed multiple times, the timer
 * will only call after it's idle
 *
 * @param timer_arg unused
 */
void ICACHE_FLASH_ATTR guitarLedOverrideReset(void* timer_arg __attribute__((unused)))
{
    guitarTunerOverrideLeds = false;
}
