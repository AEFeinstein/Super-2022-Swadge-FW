/*
 * mode_guitartuner.c
 *
 *  Created on: Oct 19, 2018
 *      Author: adam
 */

/*============================================================================
 * Includes
 *==========================================================================*/

#include "user_main.h"
#include "mode_guitar_tuner.h"
#include "DFT32.h"
#include "embeddedout.h"
#include "osapi.h"

/*============================================================================
 * Prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR guitarTunerEnterMode(void);
void ICACHE_FLASH_ATTR guitarTunerExitMode(void);
void ICACHE_FLASH_ATTR guitarTunerSampleHandler(int32_t samp);
void ICACHE_FLASH_ATTR guitarTunerButtonCallback(uint8_t state __attribute__((unused)),
        int button, int down);
void ICACHE_FLASH_ATTR guitarLedOverrideReset(void* timer_arg __attribute__((unused)));

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

static int samplesProcessed = 0;

os_timer_t guitarLedOverrideTimer = {0};
bool guitarTunerOverrideLeds = false;

/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * Initializer for guitar tuner
 */
void ICACHE_FLASH_ATTR guitarTunerEnterMode(void)
{
    InitColorChord();

    guitarTunerOverrideLeds = false;

    // Setup the LED override timer, but don't arm it
    ets_memset(&guitarLedOverrideTimer, 0, sizeof(os_timer_t));
    os_timer_disarm(&guitarLedOverrideTimer);
    os_timer_setfn(&guitarLedOverrideTimer, guitarLedOverrideReset, NULL);
}

/**
 * Called when colorchord is exited, it disarms the timer
 */
void ICACHE_FLASH_ATTR guitarTunerExitMode(void)
{
    // Disarm the timer
    os_timer_disarm(&guitarLedOverrideTimer);
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

#if 0
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
        if(!guitarOverrideLeds)
        {
            setLeds( (led_t*)ledOut, USE_NUM_LIN_LEDS * 3 );
        }
#endif

#define OFS 0

		//fuzzed_bins[0] = A ... 1/2 steps are every 2.
		#define ES0 fb[37+OFS]
		#define ES1 fb[39+OFS]
		#define ESI fb[38+OFS]	//E string needs to skip an octave... Can't read sounds this low.

		#define AS0 fb[23+OFS]
		#define AS1 fb[25+OFS]
		#define ASI fb[24+OFS]	//A string is exactly at note #24

		#define DS0 fb[33+OFS]
		#define DS1 fb[35+OFS] 
		#define DSI fb[34+OFS]	//D = A + 5 half steps = 35

		#define GS0 fb[43+OFS]
		#define GS1 fb[45+OFS]
		#define GSI fb[44+OFS]

		#define BS0 fb[51+OFS]
		#define BS1 fb[53+OFS]
		#define BSI fb[52+OFS]

		#define eS0 fb[61+OFS]
		#define eS1 fb[63+OFS]
		#define eSI fb[62+OFS]

		int16_t * fb = (int16_t*)fuzzed_bins; //Forced typecasting to allow negative results.

		uint16_t intensities_in[6] = {  eSI, BSI, GSI, DSI, ASI, ESI };
		int16_t  diffs_in[6] =       {  eS1 - eS0, BS1 - BS0, GS1 - GS0, DS1 - DS0, AS1 - AS0, ES1 - ES0 };

		static uint32_t intensities_filt[6];
		static int32_t diffs_filt[6];

		uint16_t intensities[6];
		int i;
		int16_t diffs[6];
		for( i = 0; i < 6; i++ )
		{
			intensities_filt[i] = (intensities_in[i] + intensities_filt[i]) - (intensities_filt[i]>>5);
			diffs_filt[i] =       (diffs_in[i] + diffs_filt[i]) - (diffs_filt[i]>>5);

			//Change sensitivity here, Adam.
			intensities[i] = intensities_filt[i] >> 5;
			diffs[i] = diffs_filt[i] >> 5;
		}


		uint8_t colors[18];
		uint8_t * colorptr = colors;
		for( i = 0; i < 6; i++ )
		{
			int16_t intensity = intensities[i]-40; // drop a baseline.
			int16_t  diff = diffs[i];

			int16_t id = intensity;
			if( intensity < 0 ) intensity = 0;
			if( intensity > 255 ) intensity = 255;

			//This is the tonal difference.  You "calibrate" out the intensity.
			int16_t cdiff = diff * 200 / (intensity + 1);
			int16_t abscdiff = (cdiff>0)?cdiff:-cdiff;

			int intune = (abscdiff<10)?1:0;
			int red, grn, blu;

			if( intune )
			{
				red = 255;
				grn = 255;
				blu = 255;
			}
			else
			{
				if( cdiff > 0 )
				{
					red = 255;
					grn = blu = 255-(cdiff)*15;
				}
				else
				{
					blu = 255;
					grn = red = 255-(-cdiff)*15;
				}
				if( red > 255 ) red = 255;
				if( blu > 255 ) blu = 255;
				if( grn > 255 ) grn = 255;
				red >> 3;
				grn >> 3;
				blu >> 3;
			}

			red = (red >> 3 ) * ( intensity >> 3);
			grn = (grn >> 3 ) * ( intensity >> 3);
			blu = (blu >> 3 ) * ( intensity >> 3);
			
			if( red > 255 ) red = 255;
			if( blu > 255 ) blu = 255;
			if( grn > 255 ) grn = 255;
		
			if( red < 0 ) red = 0;
			if( blu < 0 ) blu = 0;
			if( grn < 0 ) grn = 0;
		
			(*(colorptr++)) = grn;
			(*(colorptr++)) = red;  //red
			(*(colorptr++)) = blu; //blu
			

			//For hue, 0 is red.
//			uint32_t rgb = EHSVtoHEX( , 255, intensity );
//			(*(colorptr++)) = rgb;
//			(*(colorptr++)) = rgb>>8;
//			(*(colorptr++)) = rgb>>16;
		}

		if( !guitarTunerOverrideLeds )
		{
	        setLeds( (led_t*)colors, 18 );
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
void ICACHE_FLASH_ATTR guitarTunerButtonCallback(
    uint8_t state __attribute__((unused)), int button, int down)
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
