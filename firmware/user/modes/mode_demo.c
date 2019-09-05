/*
 * mode_demo.c
 *
 *  Created on: Mar 27, 2019
 *      Author: adam
 */


/*
 * mode_demo.c
 *
 *  Created on: Oct 19, 2018
 *      Author: adam
 */

/*============================================================================
 * Includes
 *==========================================================================*/

#include <osapi.h>

#include "user_main.h"
#include "mode_demo.h"
#include "DFT32.h"
#include "embeddedout.h"
#include "oled.h"
#include "font.h"
#include "MMA8452Q.h"
#include "bresenham.h"
#include "buttons.h"
#include "hpatimer.h"

/*============================================================================
 * Defines
 *==========================================================================*/

#define BTN_CTR_X 96
#define BTN_CTR_Y 40
#define BTN_RAD    8
#define BTN_OFF   12

/*============================================================================
 * Prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR demoEnterMode(void);
void ICACHE_FLASH_ATTR demoExitMode(void);
void ICACHE_FLASH_ATTR demoButtonCallback(uint8_t state __attribute__((unused)),
        int button, int down);
void ICACHE_FLASH_ATTR demoAccelerometerHandler(accel_t* accel);

void ICACHE_FLASH_ATTR updateDisplay(void);
void ICACHE_FLASH_ATTR toggleBuzzer(void* arg __attribute__((unused)));

/*============================================================================
 * Variables
 *==========================================================================*/

swadgeMode demoMode =
{
    .modeName = "demo",
    .fnEnterMode = demoEnterMode,
    .fnExitMode = demoExitMode,
    .fnButtonCallback = demoButtonCallback,
    .wifiMode = SOFT_AP,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = demoAccelerometerHandler
};

static int samplesProcessed = 0;
accel_t demoAccel = {0};
uint8_t mButtonState = 0;

const song_t testSong =
{
    .shouldLoop = true,
    .numNotes = 8,
    .notes = {
        {.note = C_4, .timeMs = 250},
        {.note = D_4, .timeMs = 250},
        {.note = E_4, .timeMs = 250},
        {.note = F_4, .timeMs = 250},
        {.note = G_4, .timeMs = 250},
        {.note = F_4, .timeMs = 250},
        {.note = E_4, .timeMs = 250},
        {.note = D_4, .timeMs = 250},
    }
};

/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * Initializer for demo
 */
void ICACHE_FLASH_ATTR demoEnterMode(void)
{
    InitColorChord();
    samplesProcessed = 0;
    enableDebounce(false);

    startBuzzerSong(&testSong);
}

/**
 * Called when demo is exited
 */
void ICACHE_FLASH_ATTR demoExitMode(void)
{

}

/**
 * TODO
 */
void ICACHE_FLASH_ATTR updateDisplay(void)
{
    // Clear the display
    clearDisplay();

    // Draw a title
    plotText(0, 0, "DEMO MODE", RADIOSTARS);

    // Display the acceleration on the display
    char accelStr[32] = {0};

    ets_snprintf(accelStr, sizeof(accelStr), "X:%d", demoAccel.x);
    plotText(0, OLED_HEIGHT - (3 * (FONT_HEIGHT_IBMVGA8 + 1)), accelStr, IBM_VGA_8);

    ets_snprintf(accelStr, sizeof(accelStr), "Y:%d", demoAccel.y);
    plotText(0, OLED_HEIGHT - (2 * (FONT_HEIGHT_IBMVGA8 + 1)), accelStr, IBM_VGA_8);

    ets_snprintf(accelStr, sizeof(accelStr), "Z:%d", demoAccel.z);
    plotText(0, OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)), accelStr, IBM_VGA_8);

    if(mButtonState & DOWN)
    {
        // Down
        plotCircle(BTN_CTR_X, BTN_CTR_Y + BTN_OFF, BTN_RAD);
        plotCircle(BTN_CTR_X, BTN_CTR_Y - BTN_OFF, BTN_RAD);
    }
    if(mButtonState & LEFT)
    {
        // Left
        plotCircle(BTN_CTR_X - BTN_OFF, BTN_CTR_Y, BTN_RAD);
    }
    if(mButtonState & RIGHT)
    {
        // Right
        plotCircle(BTN_CTR_X + BTN_OFF, BTN_CTR_Y, BTN_RAD);
    }
}

/**
 * TODO
 *
 * @param state  A bitmask of all button states, unused
 * @param button The button which triggered this event
 * @param down   true if the button was pressed, false if it was released
 */
void ICACHE_FLASH_ATTR demoButtonCallback( uint8_t state,
        int button __attribute__((unused)), int down __attribute__((unused)))
{
    mButtonState = state;
    updateDisplay();
}

/**
 * Store the acceleration data to be displayed later
 *
 * @param x
 * @param x
 * @param z
 */
void ICACHE_FLASH_ATTR demoAccelerometerHandler(accel_t* accel)
{
    demoAccel.x = accel->x;
    demoAccel.y = accel->y;
    demoAccel.z = accel->z;
    updateDisplay();
}
