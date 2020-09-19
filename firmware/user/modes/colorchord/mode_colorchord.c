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
#include "assets.h"
#include "oled.h"
#include "embeddednf.h"
#include "embeddedout.h"
#include "font.h"
#include "buttons.h"

/*============================================================================
 * Defines
 *==========================================================================*/

#define AMP_OFFSET    8
#define AMP_STEPS     6
#define AMP_STEP_SIZE 6

/*============================================================================
 * Prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR colorchordEnterMode(void);
void ICACHE_FLASH_ATTR colorchordExitMode(void);
void ICACHE_FLASH_ATTR colorchordSampleHandler(int32_t samp);
void ICACHE_FLASH_ATTR colorchordButtonCallback(uint8_t state __attribute__((unused)),
        int button, int down);
void ICACHE_FLASH_ATTR ccLedOverrideReset(void* timer_arg __attribute__((unused)));
void ICACHE_FLASH_ATTR ccAnimation(void* arg __attribute__((unused)));

/*============================================================================
 * Variables
 *==========================================================================*/

swadgeMode colorchordMode =
{
    .modeName = "colorchord",
    .fnEnterMode = colorchordEnterMode,
    .fnExitMode = colorchordExitMode,
    .fnButtonCallback = colorchordButtonCallback,
    .fnAudioCallback = colorchordSampleHandler,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .menuImg = "rainbow.gif"
};

struct
{
    int samplesProcessed;
    timer_t ccLedOverrideTimer;
    bool ccOverrideLeds;
    timer_t ccAnimationTimer;
    pngHandle king;
} cc;

struct CCSettings CCS =
{
    .gSETTINGS_KEY         = 0,
    .gROOT_NOTE_OFFSET     = 0,
    .gDFTIIR               = 6,
    .gFUZZ_IIR_BITS        = 1,
    .gFILTER_BLUR_PASSES   = 2,
    .gSEMIBITSPERBIN       = 3,
    .gMAX_JUMP_DISTANCE    = 4,
    .gMAX_COMBINE_DISTANCE = 7,
    .gAMP_1_IIR_BITS       = 4,
    .gAMP_2_IIR_BITS       = 2,
    .gMIN_AMP_FOR_NOTE     = 80,
    .gMINIMUM_AMP_FOR_NOTE_TO_DISAPPEAR = 64,
    .gNOTE_FINAL_AMP       = 12,
    .gNERF_NOTE_PORP       = 15,
    .gUSE_NUM_LIN_LEDS     = NUM_LIN_LEDS,
    .gCOLORCHORD_ACTIVE    = 1,
    .gCOLORCHORD_OUTPUT_DRIVER = 1,
    .gINITIAL_AMP          = 80
};

/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * Initializer for colorchord
 */
void ICACHE_FLASH_ATTR colorchordEnterMode(void)
{
    InitColorChord();

    ets_memset(&cc, 0, sizeof(cc));

    cc.samplesProcessed = 0;

    cc.ccOverrideLeds = false;

    // Setup the LED override timer, but don't arm it
    ets_memset(&cc.ccLedOverrideTimer, 0, sizeof(timer_t));
    timerDisarm(&cc.ccLedOverrideTimer);
    timerSetFn(&cc.ccLedOverrideTimer, ccLedOverrideReset, NULL);

    // Set up an animation timer
    timerSetFn(&cc.ccAnimationTimer, ccAnimation, NULL);
    timerArm(&cc.ccAnimationTimer, 25, true); // 40fps updates

    allocPngAsset("king.png", &cc.king);
}

void ICACHE_FLASH_ATTR ccAnimation(void* arg __attribute__((unused)))
{
    clearDisplay();
    // plotText(0, 0, "COLORCHORD", RADIOSTARS, WHITE);

    // static uint16_t rotation = 0;
    // rotation = (rotation + 4) % 360;
    // drawPng(&cc.king, (128 - 37) / 2, 0, false, false, rotation);

    uint32_t locMasses = 0;
    uint32_t totalMass = 0;
    uint32_t highestPeak = 0;
    uint32_t peakLoc = 0;
    uint32_t avg = 0;
    uint32_t avgCnt = 0;

#define THRESHOLD 100

    uint8_t i;
    for(i = 0; i < FIXBINS; i++)
    {
        // Anything below the threshold is noise
        if(fuzzed_bins[i] > THRESHOLD)
        {
            // Run up the average
            avg += fuzzed_bins[i];
            avgCnt++;

            // Also find the highest peak
            if(fuzzed_bins[i] > highestPeak)
            {
                highestPeak = fuzzed_bins[i];
                peakLoc = i;
            }
        }
    }
    int32_t delta = 0;
    // Divide the average
    if(avgCnt)
    {
        avg /= avgCnt;
        for(i = 0; i < FIXBINS; i++)
        {
            // If this point beats the average
            if(fuzzed_bins[i] > avg)
            {
                // Use it to calculate the center of mass
                locMasses += ((i + 1) * fuzzed_bins[i]);
                totalMass += fuzzed_bins[i];
            }
        }

        // As long as there was something
        if(totalMass > 0)
        {
            int32_t centerOfMass = (locMasses / totalMass) - 1;

            static int32_t movAvgMass = 0;
            int32_t lastMovAvgMass = movAvgMass;
#define MOV_AVG_NUM 0
#define MOV_AVG_DEN 1
            movAvgMass = (MOV_AVG_NUM * (movAvgMass / MOV_AVG_DEN)) + ((MOV_AVG_DEN - MOV_AVG_NUM) * (centerOfMass / MOV_AVG_DEN));
            delta = movAvgMass - lastMovAvgMass;


            char dbg[64] = {0};
            os_sprintf(dbg, "%d  %d  %d  %d", delta, peakLoc, centerOfMass, (peakLoc + centerOfMass) / 4);
            plotText(0, 0, dbg, IBM_VGA_8, WHITE);
        }
    }

    static int32_t deltaHist[20] = {0};
    ets_memmove(&deltaHist[1], &deltaHist[0], sizeof(deltaHist) - sizeof(int32_t));
    deltaHist[0] = delta;
    uint8_t posCnt = 0;
    uint8_t negCnt = 0;
    for(i = 0; i < 20; i++)
    {
        if(deltaHist[i] > 0)
        {
            posCnt++;
        }
        else if(deltaHist[i] < 0)
        {
            negCnt++;
        }
    }

    os_printf("%2d %2d\n", posCnt, negCnt);

    if(posCnt - negCnt > 2 && posCnt > 5)
    {
        plotText(0, FONT_HEIGHT_IBMVGA8 + 2, "UP", IBM_VGA_8, WHITE);
    }
    else if(negCnt - posCnt > 2 && negCnt > 5)
    {
        plotText(0, FONT_HEIGHT_IBMVGA8 + 2, "DOWN", IBM_VGA_8, WHITE);
    }

    static uint16_t maxValue = 1;
    uint16_t mv = maxValue;
    for(i = 0; i < FIXBINS; i++)
    {
        if(fuzzed_bins[i] > maxValue)
        {
            maxValue = fuzzed_bins[i];
            os_printf("%d\n", maxValue);
        }
        uint8_t height = (OLED_HEIGHT * fuzzed_bins[i]) / mv;
        fillDisplayArea(i, OLED_HEIGHT - height, (i + 1), OLED_HEIGHT, WHITE);
    }
}

/**
 * Called when colorchord is exited, it disarms the timer
 */
void ICACHE_FLASH_ATTR colorchordExitMode(void)
{
    // Disarm the timer
    timerDisarm(&cc.ccLedOverrideTimer);

    freePngAsset(&cc.king);
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
    // os_printf("%s %d\n", __func__, samp);
    PushSample32( samp );
    cc.samplesProcessed++;

    // If at least 128 samples have been processed
    if( cc.samplesProcessed >= 128 )
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
            default:
            {
                break;
            }
        };

        // Push out the LED data
        if(!cc.ccOverrideLeds)
        {
            setLeds( (led_t*)ledOut, NUM_LIN_LEDS * 3 );
        }

        // Reset the sample count
        cc.samplesProcessed = 0;
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
        // Only freeze LEDs if the button pressed displays status change via LEDs
        if(button == RIGHT || button == DOWN)
        {
            // Start a timer to restore LED functionality to colorchord
            cc.ccOverrideLeds = true;
            timerDisarm(&cc.ccLedOverrideTimer);
            timerArm(&cc.ccLedOverrideTimer, 1000, false);
        }

        switch(button)
        {
            case DOWN:
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
                        uint32_t ledColor = getLedColorPerNumber(i, 0xFF);
                        leds[i].r = (ledColor >>  0) & 0xFF;
                        leds[i].g = (ledColor >>  8) & 0xFF;
                        leds[i].b = (ledColor >> 16) & 0xFF;
                    }
                }
                setLeds(leds, sizeof(leds));
                break;
            }
            case RIGHT:
            {
                cycleColorchordSensitivity();
                break;
            }
            case LEFT:
            {
                switchToSwadgeMode(0);
            }
            default:
            {
                break;
            }
        }
    }
}

/**
 * Cycles the colorchord sensitivity
 */
void ICACHE_FLASH_ATTR cycleColorchordSensitivity(void)
{
    // The initial value is 16, so this math gets the amps
    // [8, 14, 20, 26, 32, 38]
    CCS.gINITIAL_AMP -= AMP_OFFSET;
    CCS.gINITIAL_AMP = (CCS.gINITIAL_AMP + AMP_STEP_SIZE) % (AMP_STEPS * AMP_STEP_SIZE);
    CCS.gINITIAL_AMP += AMP_OFFSET;

    // Override the LEDs to show the sensitivity, 1-6
    led_t leds[6] = {{0}};
    int i;
    for(i = 0; i < ((CCS.gINITIAL_AMP - AMP_OFFSET) / AMP_STEP_SIZE) + 1; i++)
    {
        leds[(6 - i) % 6].b = 0xFF;
    }
    setLeds(leds, sizeof(leds));
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
    cc.ccOverrideLeds = false;
}
