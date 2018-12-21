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
#include "mode_colorchord.h"

/*============================================================================
 * Defines
 *==========================================================================*/

#define NUM_STRINGS            6
#define GUITAR_OFFSET          0
#define CHROMATIC_OFFSET       6 // adjust start point by quartertones   
#define SENSITIVITY            5 // TODO Change sensitivity here, Adam.

/// Helper macro to return an integer clamped within a range (MIN to MAX)
#define CLAMP(X, MIN, MAX) ( ((X) > (MAX)) ? (MAX) : ( ((X) < (MIN)) ? (MIN) : (X)) )
/// Helper macro to return the absolute value of an integer
#define ABS(X) (((X) < 0) ? -(X) : (X))

typedef enum
{
    GUITAR_TUNER = 0,
    SEMITONE_0,
    SEMITONE_1,
    SEMITONE_2,
    SEMITONE_3,
    SEMITONE_4,
    SEMITONE_5,
    SEMITONE_6,
    SEMITONE_7,
    SEMITONE_8,
    SEMITONE_9,
    SEMITONE_10,
    SEMITONE_11,
    MAX_GUITAR_MODES
} tuner_mode_t;

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

uint32_t semitone_intensitiy_filt = 0;
int32_t semitone_diff_filt = 0;

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

tuner_mode_t currentMode = GUITAR_TUNER;

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

    // Initialize memory
    ets_memset(intensities_filt, 0, sizeof(intensities_filt));
    ets_memset(diffs_filt, 0, sizeof(diffs_filt));
    guitarSamplesProcessed = 0;
    guitarTunerOverrideLeds = false;
    currentMode = GUITAR_TUNER;
    semitone_intensitiy_filt = 0;
    semitone_diff_filt = 0;

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
 * TODO
 *
 * @param idx
 * @return
 */
inline int16_t getSemiMagnitude(uint16_t idx)
{
    if(idx < 0)
    {
        idx += FIXBPERO;
    }
    if(idx > FIXBPERO - 1)
    {
        idx -= FIXBPERO;
    }
    return folded_bins[idx];
}

/**
 * TODO
 *
 * @param idx
 * @return
 */
inline int16_t getSemiDiffAround(uint16_t idx)
{
    return getSemiMagnitude(idx + 1) - getSemiMagnitude(idx - 1);
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

        led_t colors[NUM_STRINGS] = {{0}};

        switch(currentMode)
        {
            case GUITAR_TUNER:
            {
                // Guitar tuner magic
                uint32_t i;
                for( i = 0; i < NUM_STRINGS; i++ )
                {
                    // Pick out the current magnitude and filter it
                    intensities_filt[i] = (getMagnitude(freqBinIdxs[i] + GUITAR_OFFSET)  + intensities_filt[i]) - (intensities_filt[i] >> 5);

                    // Pick out the difference around current magnitude and filter it too
                    diffs_filt[i] =       (getDiffAround(freqBinIdxs[i] + GUITAR_OFFSET) + diffs_filt[i])       - (diffs_filt[i] >> 5);

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
                break;
            }
            default:
            {
                uint8_t semitoneIdx = (currentMode - SEMITONE_0) * 2;
                // Pick out the current magnitude and filter it
                semitone_intensitiy_filt = (getSemiMagnitude(semitoneIdx + CHROMATIC_OFFSET)  + semitone_intensitiy_filt) -
                                           (semitone_intensitiy_filt >> 5);

                // Pick out the difference around current magnitude and filter it too
                semitone_diff_filt =       (getSemiDiffAround(semitoneIdx + CHROMATIC_OFFSET) + semitone_diff_filt)       -
                                           (semitone_diff_filt >> 5);

                // This is the magnitude of the target frequency bin, cleaned up
                int16_t intensity = (semitone_intensitiy_filt >> SENSITIVITY) - 40; // drop a baseline.
                intensity = CLAMP(intensity, 0, 255);

                //This is the tonal difference.  You "calibrate" out the intensity.
                int16_t tonalDiff = (semitone_diff_filt >> SENSITIVITY) * 200 / (intensity + 1);

                // TODO thaeli, this is where you come in.
                // tonal diff is -32768 to 32767. if its within -10 to 10, it's in tune. positive means too sharp, negative means too flat
                // intensity is how 'loud' that frequency is, 0 to 255. you'll have to play around with values
                //os_printf("thaeli, semitone %2d, tonal diff %6d, intensity %3d\r\n", currentMode, tonalDiff, intensity);
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
                    uint32_t i;
                    for (i=0; i < 6; i++){
                        colors[i].r = CLAMP(red, 0, 255);
                        colors[i].g = CLAMP(grn, 0, 255);
                        colors[i].b = CLAMP(blu, 0, 255);
                    }
                
                break;
            }
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

        switch(button)
        {
            case 1:
            {
                currentMode = (currentMode + 1) % MAX_GUITAR_MODES;
                os_printf("enter mode %2d", currentMode);
                led_t leds[6] = {{0}};
                // Start a timer to restore LED functionality to colorchord
                guitarTunerOverrideLeds = true;
                os_timer_disarm(&guitarLedOverrideTimer);
                os_timer_arm(&guitarLedOverrideTimer, 1000, false);
                
                if (currentMode == 0) {
                    // for guitar mode we flash all LEDs
                    uint8_t i;
                    for(i = 0; i < 6; i++)
                    {
                        // yellow
                        leds[i].r = 255;
                        leds[i].g = 255;
                        leds[i].b = 0;
                    }
                } else {
                    int32_t red, grn, blu, loc;
                    if (currentMode < 7) {
                        // cyan
                        red = 0;
                        grn = 255;
                        blu = 255;
                    } else {
                        // magenta
                        red = 255;
                        grn = 0;
                        blu = 255;
                    }
                    loc = (currentMode % 6);
                    os_printf("mode %2d, loc %1d\r\n", currentMode, loc);
                    leds[loc].r = red;
                    leds[loc].g = grn;
                    leds[loc].b = blu;
                }
                setLeds(leds, sizeof(leds));
                break;
            }
            case 2:
            {
                cycleColorchordSensitivity();
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
void ICACHE_FLASH_ATTR guitarLedOverrideReset(void* timer_arg __attribute__((unused)))
{
    guitarTunerOverrideLeds = false;
}
