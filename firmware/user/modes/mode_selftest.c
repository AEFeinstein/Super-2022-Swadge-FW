/*============================================================================
 * Includes
 *==========================================================================*/

#include <osapi.h>
#include <mem.h>
#include <user_interface.h>

#include "user_main.h"
#include "mode_selftest.h"
#include "embeddednf.h"
#include "nvm_interface.h"

#include "oled.h"
#include "cndraw.h"
#include "bresenham.h"
#include "font.h"
#include "assets.h"

#include "buttons.h"

/*============================================================================
 * Defines
 *==========================================================================*/

/*============================================================================
 * Enums
 *==========================================================================*/

typedef enum
{
    BS_INIT,
    BS_PRESSED,
    BS_RELEASED,
} buttonState_t;

/*============================================================================
 * Function prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR selfTestInit(void);
void ICACHE_FLASH_ATTR selfTestExit(void);
void ICACHE_FLASH_ATTR selfTestButtonCallback(uint8_t state, int button, int down);
void ICACHE_FLASH_ATTR selfTestAudioCallback(int32_t samp);
bool ICACHE_FLASH_ATTR selfTestRenderTask(void);
void ICACHE_FLASH_ATTR selfTestLedFunc(void*);

/*============================================================================
 * Variables
 *==========================================================================*/

typedef struct
{
    int16_t samplesProcessed;
    timer_t ledTimer;
    buttonState_t buttonStates[NUM_BUTTONS];
    pngSequenceHandle burger;
} selftest_t;

/*============================================================================
 * Variables
 *==========================================================================*/

swadgeMode selfTestMode =
{
    .modeName = "selfTest",
    .fnEnterMode = selfTestInit,
    .fnExitMode = selfTestExit,
    .fnButtonCallback = selfTestButtonCallback,
    .fnAudioCallback = selfTestAudioCallback,
    .fnRenderTask = selfTestRenderTask,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
};

selftest_t* st;

/*============================================================================
 * Swadge Mode Functions
 *==========================================================================*/

/**
 * Initialize the selfTest
 */
void ICACHE_FLASH_ATTR selfTestInit(void)
{
    // Allocate memory
    st = os_malloc(sizeof(selftest_t));

    // Initialize colorchord
    InitColorChord();

    // Set up LED timer
    selfTestLedFunc(NULL);
    timerSetFn(&(st->ledTimer), selfTestLedFunc, NULL);
    timerArm(&(st->ledTimer), 500, true);

    // Set up burger image sequence
    allocPngSequence(&(st->burger), 3,
                     "burger1.png",
                     "burger2.png",
                     "burger3.png");
}

/**
 * Free memory and disarm timers
 */
void ICACHE_FLASH_ATTR selfTestExit(void)
{
    timerDisarm(&(st->ledTimer));
    timerFlush();
    freePngSequence(&(st->burger));
    os_free(st);
}

/**
 * Handle the button. Each button must be pressed and released to pass the test
 *
 * @param state  A bitmask of all buttons, unused
 * @param button The button that was just pressed or released
 * @param down   true if the button was pressed, false if it was released
 */
void ICACHE_FLASH_ATTR selfTestButtonCallback(uint8_t state __attribute__((unused)),
        int button, int down)
{
    if(down)
    {
        if(st->buttonStates[button] == BS_INIT)
        {
            st->buttonStates[button] = BS_PRESSED;
        }
    }
    else
    {
        if(st->buttonStates[button] == BS_PRESSED)
        {
            st->buttonStates[button] = BS_RELEASED;
        }
    }
}

/**
 * Pass microphone samples to colorchord
 *
 * @param samp The sample read from the microphone
 */
void ICACHE_FLASH_ATTR selfTestAudioCallback(int32_t samp)
{
    PushSample32( samp );
    st->samplesProcessed++;

    // If at least 128 samples have been processed
    if( st->samplesProcessed >= 128 )
    {
        // Colorchord magic
        HandleFrameInfo();

        // Reset the sample count
        st->samplesProcessed = 0;
    }
}

/**
 * Draw everything to the screen including what buttons have been pressed,
 * the microphone's current FFT, an animated sprite, and some useful text
 */
bool ICACHE_FLASH_ATTR selfTestRenderTask(void)
{
    // Clear everything first
    clearDisplay();

    // Track 'energy' to validate the mic is working
    static uint32_t maxEnergy = 0;
    uint32_t energy = 0;
    // Track the max value to scale the FFT to fit the OLED
    static uint16_t maxValue = 1;
    uint16_t mv = maxValue;
    // Plot audio FFT
    for(int16_t i = 0; i < FIXBINS; i++)
    {
        if(fuzzed_bins[i] > maxValue)
        {
            maxValue = fuzzed_bins[i];
        }
        uint8_t height = (OLED_HEIGHT * fuzzed_bins[i]) / mv;
        fillDisplayArea(i, OLED_HEIGHT - height, (i + 1), OLED_HEIGHT, WHITE);
        energy += fuzzed_bins[i];
    }
    if(energy > maxEnergy)
    {
        maxEnergy = energy;
    }

    // Then plot the animated burger.
    // Keep track of the time elapsed between the last call and now
    static uint32_t tLast = 0;
    static uint32_t tAccumulated = 0;
    uint32_t tNow = system_get_time();
    tAccumulated += (tNow - tLast);
    tLast = tNow;
    // Every 0.5s switch the burger's frame
    static int8_t burgerIdx = 0;
    while(tAccumulated > 500000)
    {
        tAccumulated -= 500000;
        burgerIdx = (burgerIdx + 1) % (st->burger.count + 1);
    }
    // Draw the burger
    if(burgerIdx < st->burger.count)
    {
        drawPngSequence(&(st->burger),
                        OLED_WIDTH - st->burger.handles->width,
                        OLED_HEIGHT - st->burger.handles->height,
                        false, false, 0, burgerIdx);
    }

    // Then plot buttons, keeping track of how many were pressed and released
    int16_t numOkButtons = 0;
    for(int16_t i = 0; i < NUM_BUTTONS; i++)
    {
        switch (st->buttonStates[i])
        {
            default:
            case BS_INIT:
            {
                break;
            }
            case BS_RELEASED:
            {
                // After a button is released, 'fill in' the circle
                numOkButtons++;
                plotCircle(69 + i * 12, FONT_HEIGHT_TOMTHUMB + 7, 4, WHITE);
                plotCircle(69 + i * 12, FONT_HEIGHT_TOMTHUMB + 7, 3, WHITE);
                plotCircle(69 + i * 12, FONT_HEIGHT_TOMTHUMB + 7, 2, WHITE);
                plotCircle(69 + i * 12, FONT_HEIGHT_TOMTHUMB + 7, 1, WHITE);
                // No break
            }
            case BS_PRESSED:
            {
                // When a button is pressed, just draw the outline
                plotCircle(69 + i * 12, FONT_HEIGHT_TOMTHUMB + 7, 5, WHITE);
                break;
            }
        }
    }

    // Then plot text, starting with the git hash
    char githash[32] = {0};
    getGitHash(githash);
    plotText(0, 0, githash, TOM_THUMB, WHITE);

    uint8_t passedTests = 0;
    // If all the buttons are OK, plot that
    if(NUM_BUTTONS == numOkButtons)
    {
        plotText(0, FONT_HEIGHT_TOMTHUMB + 2, "BTN OK", IBM_VGA_8, WHITE);
        passedTests++;
    }

    // If the mic heard something loud, plot that
    if(maxEnergy > 40000)
    {
        plotText(0, FONT_HEIGHT_TOMTHUMB + FONT_HEIGHT_IBMVGA8 + 4, "MIC OK", IBM_VGA_8, WHITE);
        passedTests++;
    }

    // If the mic and buttons are good, plot that
    if(2 == passedTests)
    {
        plotText(OLED_WIDTH / 2, FONT_HEIGHT_TOMTHUMB + FONT_HEIGHT_IBMVGA8 + 4, "PASSED", IBM_VGA_8, WHITE);

        // If we haven't written to NVM yet
        if(false == getSelfTestPass())
        {
            // Write that the self test passed
            setSelfTestPass(true);
        }
    }

    // Always return a scene to draw
    return true;
}

/**
 * LED timer function, called every 0.5s.
 * Rotate R, G, and B around the chainsaw
 *
 * @param arg unused
 */
void ICACHE_FLASH_ATTR selfTestLedFunc(void* arg __attribute__((unused)))
{
    led_t leds[NUM_LIN_LEDS] = {{0}};
    static int8_t idx = 0;

    // Set the R, G, and B LEDs
    leds[(idx + 0) % NUM_LIN_LEDS].b = 0x20;
    leds[(idx + 1) % NUM_LIN_LEDS].g = 0x20;
    leds[(idx + 2) % NUM_LIN_LEDS].r = 0x20;

    // Every call increment this by one
    idx = (idx + 1) % NUM_LIN_LEDS;

    // Push out the LEDs
    setLeds(leds, sizeof(leds));
}
