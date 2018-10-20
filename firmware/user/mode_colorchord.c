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

/*============================================================================
 * Prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR colorchordEnterMode(void);
void ICACHE_FLASH_ATTR colorchordSampleHandler(int32_t samp);

/*============================================================================
 * Variables
 *==========================================================================*/

swadgeMode colorchordMode =
{
    .modeName = "colorchord",
    .fnEnterMode = colorchordEnterMode,
    .fnExitMode = NULL,
    .fnTimerCallback = NULL,
    .fnButtonCallback = NULL,
    .fnAudioCallback = colorchordSampleHandler,
    .shouldConnect = SOFTAP_MODE,
    .connectionColor = 0x00000000,
    .fnConnectionCallback = NULL,
    .fnPacketCallback = NULL,
    .next = NULL
};

static int samplesProcessed = 0;

/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * Initializer for colorchord
 */
void ICACHE_FLASH_ATTR colorchordEnterMode(void)
{
    InitColorChord();
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
    //printf("%s :: %d\r\n", __func__, __LINE__);
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
        setLeds( ledOut, USE_NUM_LIN_LEDS * 3 );

        // Reset the sample count
        samplesProcessed = 0;
    }
}
