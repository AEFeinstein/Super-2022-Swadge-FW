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
void ICACHE_FLASH_ATTR colorchordButtonCallback(uint8_t state __attribute__((unused)),
        int button, int down);

/*============================================================================
 * Variables
 *==========================================================================*/

swadgeMode colorchordMode =
{
    .modeName = "colorchord",
    .fnEnterMode = colorchordEnterMode,
    .fnExitMode = NULL,
    .fnTimerCallback = NULL,
    .fnButtonCallback = colorchordButtonCallback,
    .fnAudioCallback = colorchordSampleHandler,
    .wifiMode = SOFT_AP,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
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
        setLeds( ledOut, USE_NUM_LIN_LEDS * 3 );

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
        switch(button)
        {
            case 1:
            {
                // gCOLORCHORD_OUTPUT_DRIVER can be either 0 for multiple LED
                // colors or 1 for all the same LED color
                CCS.gCOLORCHORD_OUTPUT_DRIVER =
                    (CCS.gCOLORCHORD_OUTPUT_DRIVER + 1) % 2;
                break;
            }
            case 2:
            {
                // The initial value is 16, so this math gets the amps
                // [0, 8, 16, 24, 32]
                CCS.gINITIAL_AMP = (CCS.gINITIAL_AMP + 8) % 40;
                break;
            }
        }
    }
}
