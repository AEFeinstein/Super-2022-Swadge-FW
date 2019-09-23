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
#include "sprite.h"
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
static void ICACHE_FLASH_ATTR rotateBanana(void* arg __attribute__((unused)));

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

static os_timer_t timerHandleBanana = {0};
static uint8_t bananaIdx = 0;
const sprite_t rotating_banana[] ICACHE_RODATA_ATTR =
{
    // frame_0_delay-0.07s.png
    {
        .width = 16,
        .height = 16,
        .data =
        {
            0b0000000010000000,
            0b0000000110000000,
            0b0000000011000000,
            0b0000000010100000,
            0b0000000010010000,
            0b0000000100010000,
            0b0000000100001000,
            0b0000000100001000,
            0b0000000100001000,
            0b0000001000001000,
            0b0000010000001000,
            0b0001100000010000,
            0b0110000000100000,
            0b0100000011000000,
            0b0110011100000000,
            0b0001100000000000,
        }
    },
    // frame_1_delay-0.07s.png
    {
        .width = 16,
        .height = 16,
        .data =
        {
            0b0000000100000000,
            0b0000000110000000,
            0b0000000110000000,
            0b0000000110000000,
            0b0000001001000000,
            0b0000001001000000,
            0b0000001000100000,
            0b0000001000100000,
            0b0000010000100000,
            0b0000010000100000,
            0b0000010000100000,
            0b0000100000100000,
            0b0001000001000000,
            0b0001000011000000,
            0b0000111100000000,
            0b0000000000000000,
        }
    },
    // frame_2_delay-0.07s.png
    {
        .width = 16,
        .height = 16,
        .data =
        {
            0b0000000100000000,
            0b0000001100000000,
            0b0000001100000000,
            0b0000010100000000,
            0b0000010100000000,
            0b0000100100000000,
            0b0000100010000000,
            0b0001000010000000,
            0b0001000010000000,
            0b0001000010000000,
            0b0001000001000000,
            0b0000100001000000,
            0b0000100001000000,
            0b0000011001000000,
            0b0000000110000000,
            0b0000000000000000,
        }
    },
    // frame_3_delay-0.07s.png
    {
        .width = 16,
        .height = 16,
        .data =
        {
            0b0000001100000000,
            0b0000001100000000,
            0b0000011000000000,
            0b0000101000000000,
            0b0001001000000000,
            0b0010001000000000,
            0b0010000100000000,
            0b0010000100000000,
            0b0010000100000000,
            0b0010000010000000,
            0b0001000001000000,
            0b0001000000110000,
            0b0000100000001000,
            0b0000011000000100,
            0b0000000111111000,
            0b0000000000000000,
        }
    },
    // frame_4_delay-0.07s.png
    {
        .width = 16,
        .height = 16,
        .data =
        {
            0b0000001100000000,
            0b0000001100000000,
            0b0000011000000000,
            0b0000101000000000,
            0b0001001100000000,
            0b0010000100000000,
            0b0010000100000000,
            0b0010000100000000,
            0b0010000010000000,
            0b0010000010000000,
            0b0001000001000000,
            0b0001000000100000,
            0b0000100000011000,
            0b0000010000000100,
            0b0000001100000100,
            0b0000000011111100,
        }
    },
    // frame_5_delay-0.07s.png
    {
        .width = 16,
        .height = 16,
        .data =
        {
            0b0000000100000000,
            0b0000001100000000,
            0b0000001100000000,
            0b0000010010000000,
            0b0000010010000000,
            0b0000100010000000,
            0b0000100001000000,
            0b0000100001000000,
            0b0000100001000000,
            0b0000100001000000,
            0b0000100000100000,
            0b0000010000100000,
            0b0000010000100000,
            0b0000001000010000,
            0b0000001000010000,
            0b0000000111110000,
        }
    },
    // frame_6_delay-0.07s.png
    {
        .width = 16,
        .height = 16,
        .data =
        {
            0b0000000100000000,
            0b0000000110000000,
            0b0000000101000000,
            0b0000000100100000,
            0b0000000100100000,
            0b0000001000100000,
            0b0000001000010000,
            0b0000001000010000,
            0b0000001000010000,
            0b0000010000010000,
            0b0000010000100000,
            0b0000010000100000,
            0b0000010000100000,
            0b0000010000100000,
            0b0000100001000000,
            0b0000111110000000,
        }
    },
    // frame_7_delay-0.07s.png
    {
        .width = 16,
        .height = 16,
        .data =
        {
            0b0000000110000000,
            0b0000000011000000,
            0b0000000010100000,
            0b0000000010100000,
            0b0000000010010000,
            0b0000000100001000,
            0b0000000100001000,
            0b0000000100001000,
            0b0000000100001000,
            0b0000001000001000,
            0b0000010000010000,
            0b0000100000010000,
            0b0001000000100000,
            0b0010000001000000,
            0b0100000010000000,
            0b0111111100000000,
        }
    },
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

    // Start a software timer to rotate the banana every 100ms
    os_timer_disarm(&timerHandleBanana);
    os_timer_setfn(&timerHandleBanana, (os_timer_func_t*)rotateBanana, NULL);
    os_timer_arm(&timerHandleBanana, 100, 1);
}

/**
 * Called when demo is exited
 */
void ICACHE_FLASH_ATTR demoExitMode(void)
{
    os_timer_disarm(&timerHandleBanana);
}

/**
 * @brief Called on a timer, this rotates the banana by picking the next sprite
 *
 * @param arg unused
 */
static void ICACHE_FLASH_ATTR rotateBanana(void* arg __attribute__((unused)))
{
    bananaIdx = (bananaIdx + 1) % (sizeof(rotating_banana) / sizeof(rotating_banana[0]));
}

/**
 * TODO
 */
void ICACHE_FLASH_ATTR updateDisplay(void)
{
    // Clear the display
    clearDisplay();

    // Draw a title
    plotText(0, 0, "DEMO MODE", RADIOSTARS, WHITE);
    plotEllipseRect(0, 0, 120, 20, INVERSE);

    // Display the acceleration on the display
    char accelStr[32] = {0};

    ets_snprintf(accelStr, sizeof(accelStr), "X:%d", demoAccel.x);
    plotText(0, OLED_HEIGHT - (3 * (FONT_HEIGHT_IBMVGA8 + 1)), accelStr, IBM_VGA_8, WHITE);

    ets_snprintf(accelStr, sizeof(accelStr), "Y:%d", demoAccel.y);
    plotText(0, OLED_HEIGHT - (2 * (FONT_HEIGHT_IBMVGA8 + 1)), accelStr, IBM_VGA_8, WHITE);

    ets_snprintf(accelStr, sizeof(accelStr), "Z:%d", demoAccel.z);
    plotText(0, OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)), accelStr, IBM_VGA_8, WHITE);

    if(mButtonState & DOWN)
    {
        // Down
        plotCircle(BTN_CTR_X, BTN_CTR_Y + BTN_OFF, BTN_RAD, WHITE);
        plotCircle(BTN_CTR_X, BTN_CTR_Y - BTN_OFF, BTN_RAD, WHITE);
        plotText(0, 0, "DEMO MODE", RADIOSTARS, INVERSE);

    }
    if(mButtonState & LEFT)
    {
        // Left
        plotCircle(BTN_CTR_X - BTN_OFF, BTN_CTR_Y, BTN_RAD, WHITE);
    }
    if(mButtonState & RIGHT)
    {
        // Right
        plotCircle(BTN_CTR_X + BTN_OFF, BTN_CTR_Y, BTN_RAD, WHITE);
    }

    // Draw the banana
    plotSprite(54, 32, &rotating_banana[bananaIdx], WHITE);
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
