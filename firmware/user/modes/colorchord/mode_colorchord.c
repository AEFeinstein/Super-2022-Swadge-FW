/*
 * mode_colorchord.c
 *
 *  Created on: Oct 19, 2018
 *      Author: adam
 */

/*==============================================================================
 * Includes
 *============================================================================*/

#include <user_interface.h>
#include "user_main.h"
#include "mode_colorchord.h"
#include "DFT32.h"
#include "embeddedout.h"
#include "osapi.h"
#include "assets.h"
#include "oled.h"
#include "cndraw.h"
#include "embeddednf.h"
#include "embeddedout.h"
#include "font.h"
#include "buttons.h"
#include "bresenham.h"
#include "menu_strings.h"

/*==============================================================================
 * Defines
 *============================================================================*/

#define US_TO_QUIT 1048576 // 2^20, makes division easy

/*==============================================================================
 * Prototypes
 *============================================================================*/

void ICACHE_FLASH_ATTR colorchordEnterMode(void);
void ICACHE_FLASH_ATTR colorchordExitMode(void);
void ICACHE_FLASH_ATTR colorchordSampleHandler(int32_t samp);
void ICACHE_FLASH_ATTR colorchordButtonCallback(uint8_t state, int button, int down);
bool ICACHE_FLASH_ATTR ccRenderTask(void);
void ICACHE_FLASH_ATTR ccExitTimerFn(void* arg);

/*==============================================================================
 * Variables
 *============================================================================*/

swadgeMode colorchordMode =
{
    .modeName = "colorchord",
    .fnEnterMode = colorchordEnterMode,
    .fnExitMode = colorchordExitMode,
    .fnButtonCallback = colorchordButtonCallback,
    .fnAudioCallback = colorchordSampleHandler,
    .fnRenderTask = ccRenderTask,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .menuImg = "rainbow.gif"
};

struct
{
    int samplesProcessed;
    uint16_t maxValue;
    timer_t exitTimer;
    uint32_t exitTimeAccumulatedUs;
    uint32_t tLastCallUs;
    bool shouldExit;
    pngHandle upArrowPng;
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
    .gINITIAL_AMP          = AMP_OFFSET + (AMP_STEP_SIZE * (AMP_STEPS / 2))
};

/*==============================================================================
 * Functions
 *============================================================================*/

/**
 * Initializer for colorchord
 */
void ICACHE_FLASH_ATTR colorchordEnterMode(void)
{
    InitColorChord();

    ets_memset(&cc, 0, sizeof(cc));
    cc.samplesProcessed = 0;
    cc.maxValue = 1;

    cc.exitTimeAccumulatedUs = 0;
    cc.tLastCallUs = 0;
    cc.shouldExit = false;
    timerSetFn(&(cc.exitTimer), ccExitTimerFn, NULL);

    allocPngAsset("uparrow.png", &(cc.upArrowPng));

    enableDebounce(false);
}

/**
 * Called whenever it's time to draw to the OLED. This renders a spectrogram
 * and some text
 *
 * @return true If there is something to draw, false if there is no change
 */
bool ICACHE_FLASH_ATTR ccRenderTask(void)
{
    // If a timer says to quit, quit
    if(cc.shouldExit)
    {
        switchToSwadgeMode(0);
        return false;
    }

    // Clear the display first
    clearDisplay();

    // Draw the spectrum as a bar graph
    uint16_t mv = cc.maxValue;
    for(uint16_t i = 0; i < FIXBINS; i++)
    {
        if(fuzzed_bins[i] > cc.maxValue)
        {
            cc.maxValue = fuzzed_bins[i];
        }
        uint8_t height = (OLED_HEIGHT * fuzzed_bins[i]) / mv;
        fillDisplayArea(i, OLED_HEIGHT - height, (i + 1), OLED_HEIGHT - 2, WHITE);
    }

    // Plot sensitivity
    char text[16] = {0};
    int16_t ampLevel = 1 + ((CCS.gINITIAL_AMP - AMP_OFFSET) / AMP_STEP_SIZE);
    ets_snprintf(text, sizeof(text) - 1, "GAIN:%d", ampLevel);
    fillDisplayArea(0, 0, textWidth(text, IBM_VGA_8), FONT_HEIGHT_IBMVGA8, BLACK);
    plotText(0, 0, text, IBM_VGA_8, WHITE);
    drawPng(&cc.upArrowPng, 0, FONT_HEIGHT_IBMVGA8 + 1, false, false, 270);

    // Plot output mode
    ets_memset(text, sizeof(text), 0);
    switch(CCS.gCOLORCHORD_OUTPUT_DRIVER)
    {
        default:
        case 0:
        {
            ets_strncpy(text, "Rainbow", sizeof(text));
            break;
        }
        case 1:
        {
            ets_strncpy(text, "Solid", sizeof(text));
            break;
        }
    }
    uint16_t width = textWidth(text, IBM_VGA_8);
    fillDisplayArea(OLED_WIDTH - width - 1, 0, OLED_WIDTH, FONT_HEIGHT_IBMVGA8, BLACK);
    plotText(OLED_WIDTH - width, 0, text, IBM_VGA_8, WHITE);
    drawPng(&cc.upArrowPng, OLED_WIDTH - cc.upArrowPng.width, FONT_HEIGHT_IBMVGA8 + 1, false, false, 90);

    // Plot exit label
    width = textWidth(str_quit, IBM_VGA_8);
    fillDisplayArea((OLED_WIDTH - width) / 2 - 1,
                    OLED_HEIGHT - FONT_HEIGHT_IBMVGA8 - 2,
                    (OLED_WIDTH - width) / 2 + width - 1,
                    OLED_HEIGHT - 1,
                    BLACK);
    plotText((OLED_WIDTH - width) / 2, OLED_HEIGHT - FONT_HEIGHT_IBMVGA8 - 1, str_quit, IBM_VGA_8, WHITE);
    drawPng(&cc.upArrowPng, (OLED_WIDTH - cc.upArrowPng.width) / 2,
            OLED_HEIGHT - FONT_HEIGHT_IBMVGA8 - 1 - cc.upArrowPng.height, false, false, 180);

    // If the quit button is being held
    if(cc.exitTimeAccumulatedUs > 0)
    {
        // Draw a bar
        plotLine(0, OLED_HEIGHT - 1, (OLED_WIDTH * cc.exitTimeAccumulatedUs) / US_TO_QUIT, OLED_HEIGHT - 1, WHITE);
    }
    return true;
}

/**
 * Called when colorchord is exited, cleanup
 */
void ICACHE_FLASH_ATTR colorchordExitMode(void)
{
    freePngAsset(&(cc.upArrowPng));

    timerDisarm(&(cc.exitTimer));
    timerFlush();
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
            default:
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
        setLeds( (led_t*)ledOut, NUM_LIN_LEDS * 3 );

        // Reset the sample count
        cc.samplesProcessed = 0;
    }
}

/**
 * Button callback for colorchord. Cycle through the options or exit
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
            case DOWN:
            {
                // Start the timer to exit
                timerArm(&(cc.exitTimer), 1, true);
                break;
            }
            case RIGHT:
            {
                cycleColorchordOutput();
                break;
            }
            case LEFT:
            {
                cycleColorchordSensitivity();
                break;
            }
            case UP:
            case ACTION:
            default:
            {
                // The buttons, they do nothing!
                break;
            }
        }
    }
    else
    {
        switch(button)
        {
            case DOWN:
            {
                // Stop the timer to exit
                cc.exitTimeAccumulatedUs = 0;
                cc.tLastCallUs = 0;
                timerDisarm(&(cc.exitTimer));
                break;
            }
            case RIGHT:
            case LEFT:
            case UP:
            case ACTION:
            default:
            {
                break;
            }
        }
    }
}

/**
 * This timer function is called periodically when the button to exit is held down
 * When the accumulated time hits US_TO_QUIT, the mode is quit
 *
 * @param arg unused
 */
void ICACHE_FLASH_ATTR ccExitTimerFn(void* arg __attribute__((unused)))
{
    if(0 == cc.tLastCallUs)
    {
        cc.tLastCallUs = system_get_time();
    }
    else
    {
        uint32_t tNowUs = system_get_time();
        cc.exitTimeAccumulatedUs += (tNowUs - cc.tLastCallUs);
        cc.tLastCallUs = tNowUs;

        if(cc.exitTimeAccumulatedUs > US_TO_QUIT)
        {
            cc.shouldExit = true;
        }
    }
}

/**
 * Cycles through the colorchord output options
 */
void ICACHE_FLASH_ATTR cycleColorchordOutput(void)
{
    // gCOLORCHORD_OUTPUT_DRIVER can be either 0 for multiple LED
    // colors or 1 for all the same LED color
    CCS.gCOLORCHORD_OUTPUT_DRIVER = (CCS.gCOLORCHORD_OUTPUT_DRIVER + 1) % 2;
}

/**
 * Cycles the colorchord sensitivity
 */
void ICACHE_FLASH_ATTR cycleColorchordSensitivity(void)
{
    // Cycle to the next amplification
    CCS.gINITIAL_AMP -= AMP_OFFSET;
    CCS.gINITIAL_AMP = (CCS.gINITIAL_AMP + AMP_STEP_SIZE) % (AMP_STEPS * AMP_STEP_SIZE);
    CCS.gINITIAL_AMP += AMP_OFFSET;

    // Reset the spectrogram max value
    cc.maxValue = 1;
}
