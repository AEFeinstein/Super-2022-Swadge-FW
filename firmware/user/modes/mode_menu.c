/*============================================================================
 * Includes
 *==========================================================================*/

#include <mem.h>
#include "osapi.h"
#include "user_main.h"
#include "assets.h"
#include "nvm_interface.h"
#include "oled.h"
#include "bresenham.h"
#include "cndraw.h"
#include "font.h"
#include "buttons.h"

#include "mode_menu.h"
#include "mode_dance.h"

#include "printControl.h"

/*============================================================================
 * Defines
 *==========================================================================*/

#define MENU_PAN_PERIOD_MS 20
#define MENU_PX_PER_PAN     8
#define SQ_WAVE_LINE_LEN   16

/*============================================================================
 * Enums
 *==========================================================================*/

typedef enum
{
    M_UP    = 0,
    M_RIGHT = 1,
    M_DOWN  = 2,
    M_RIGHT2 = 3,
    M_NUM_DIRS
} sqDir_t;

/*============================================================================
 * Function prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR menuInit(void);
void ICACHE_FLASH_ATTR menuExit(void);
void ICACHE_FLASH_ATTR menuButtonCallback(uint8_t state, int button, int down);

void ICACHE_FLASH_ATTR plotSquareWave(int16_t x, int16_t y);
static void ICACHE_FLASH_ATTR menuStartScreensaver(void* arg __attribute__((unused)));
static void ICACHE_FLASH_ATTR menuBrightScreensaver(void* arg __attribute__((unused)));
static void ICACHE_FLASH_ATTR menuAnimateScreensaverLEDs(void* arg __attribute__((unused)));
static void ICACHE_FLASH_ATTR menuAnimateScreensaverOLED(void* arg __attribute__((unused)));
void ICACHE_FLASH_ATTR stopScreensaver(void);

void ICACHE_FLASH_ATTR startPanning(bool pLeft);
static void ICACHE_FLASH_ATTR menuPanImages(void* arg __attribute__((unused)));
void ICACHE_FLASH_ATTR mnuDrawArrows(void);

/*============================================================================
 * Variables
 *==========================================================================*/

typedef struct
{
    uint8_t numModes;
    swadgeMode** modes;
    uint8_t selectedMode;

    timer_t timerScreensaverStart;
    timer_t timerScreensaverBright;
    timer_t timerScreensaverLEDAnimation;
    timer_t timerScreensaverOLEDAnimation;
    uint8_t menuScreensaverIdx;
    int16_t squareWaveScrollOffset;
    int16_t squareWaveScrollSpeed;
    uint8_t drawOLEDScreensaver;

    gifHandle img1;
    gifHandle img2;
    gifHandle* curImg;
    gifHandle* nextImg;

    timer_t timerPanning;
    bool menuIsPanning;
    bool panningLeft;
    int16_t panIdx;
} mnu_t;

/*============================================================================
 * Variables
 *==========================================================================*/

swadgeMode menuMode =
{
    .modeName = "menu",
    .fnEnterMode = menuInit,
    .fnExitMode = menuExit,
    .fnButtonCallback = menuButtonCallback,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
};

mnu_t* mnu;

/*============================================================================
 * Swadge Mode Functions
 *==========================================================================*/

/**
 * Initialize the menu by getting the list of mnu->modes from user_main.c
 */
void ICACHE_FLASH_ATTR menuInit(void)
{
    mnu = os_malloc(sizeof(mnu_t));
    ets_memset(mnu, 0, sizeof(mnu_t));

    // expressed as pixels per frame.
    mnu->squareWaveScrollSpeed = -1;

    // Assign pointers
    mnu->curImg = &mnu->img1;
    mnu->nextImg = &mnu->img2;

    // Get the list of mnu->modes
    mnu->numModes = getSwadgeModes(&mnu->modes);
    // Don't count the menu as a mode
    mnu->numModes--;

    // Start where we left off
    mnu->selectedMode = getMenuPos();

    // Load and draw the first image
    loadGifFromAsset(mnu->modes[1 + mnu->selectedMode]->menuImg, mnu->curImg);
    drawGifFromAsset(mnu->curImg, 0, 0, false, false, 0, false);
    mnuDrawArrows();

    // Timer for starting a screensaver
    timerDisarm(&mnu->timerScreensaverStart);
    timerSetFn(&mnu->timerScreensaverStart, (os_timer_func_t*)menuStartScreensaver, NULL);

    // Timer for starting a screensaver
    timerDisarm(&mnu->timerScreensaverBright);
    timerSetFn(&mnu->timerScreensaverBright, (os_timer_func_t*)menuBrightScreensaver, NULL);

    // Timer for running a screensaver
    timerDisarm(&mnu->timerScreensaverLEDAnimation);
    timerSetFn(&mnu->timerScreensaverLEDAnimation, (os_timer_func_t*)menuAnimateScreensaverLEDs, NULL);

    // Timer for running a screensaver
    timerDisarm(&mnu->timerScreensaverOLEDAnimation);
    timerSetFn(&mnu->timerScreensaverOLEDAnimation, (os_timer_func_t*)menuAnimateScreensaverOLED, NULL);

    // Timer for running a screensaver
    timerDisarm(&mnu->timerPanning);
    timerSetFn(&mnu->timerPanning, (os_timer_func_t*)menuPanImages, NULL);

    // This starts the screensaver timer
    stopScreensaver();

    // Make buttons sensitive, they're ignored during animation anyway
    enableDebounce(false);
}

/**
 * @brief Free memory and disarm timers
 *
 */
void ICACHE_FLASH_ATTR menuExit(void)
{
    timerDisarm(&mnu->timerScreensaverStart);
    timerDisarm(&mnu->timerScreensaverBright);
    timerDisarm(&mnu->timerScreensaverLEDAnimation);
    timerDisarm(&mnu->timerScreensaverOLEDAnimation);
    timerDisarm(&mnu->timerPanning);
    timerFlush();
    freeGifAsset(mnu->curImg);
    freeGifAsset(mnu->nextImg);
    os_free(mnu);
}

/**
 * Handle the button. Left press selects the mode, right press starts it
 *
 * @param state  A bitmask of all buttons, unused
 * @param button The button that was just pressed or released
 * @param down   true if the button was pressed, false if it was released
 */
void ICACHE_FLASH_ATTR menuButtonCallback(uint8_t state __attribute__((unused)),
        int button, int down)
{
    // Stop the screensaver
    stopScreensaver();

    // Don't accept button input if the menu is panning
    if(mnu->menuIsPanning)
    {
        return;
    }

    // Menu is not panning
    if(down)
    {
        switch(button)
        {
            case 4:
            {
                // Select the mode
                setMenuPos(mnu->selectedMode);
                switchToSwadgeMode(1 + mnu->selectedMode);
                break;
            }
            case 2:
            {
                // Cycle the currently selected mode
                mnu->selectedMode = (mnu->selectedMode + 1) % mnu->numModes;
                startPanning(true);
                break;
            }
            case 0:
            {
                // Cycle the currently selected mode
                if(0 == mnu->selectedMode)
                {
                    mnu->selectedMode = mnu->numModes - 1;
                }
                else
                {
                    mnu->selectedMode--;
                }

                startPanning(false);
                break;
            }
            default:
            {
                // Do nothing
                break;
            }
        }
    }
}

/*==============================================================================
 * Menu panning functions
 *============================================================================*/

/**
 * Start panning the menu
 *
 * @param pLeft true to pan to the left, false to pan to the right
 */
void ICACHE_FLASH_ATTR startPanning(bool pLeft)
{
    // Block button input until it's done
    mnu->menuIsPanning = true;

    // Load the next image
    freeGifAsset(mnu->nextImg);
    loadGifFromAsset(mnu->modes[1 + mnu->selectedMode]->menuImg, mnu->nextImg);

    // Start the timer to pan
    mnu->panningLeft = pLeft;
    mnu->panIdx = 0;
    timerArm(&mnu->timerPanning, MENU_PAN_PERIOD_MS, true);
}

/**
 * Timer function called periodically while the menu is panning
 *
 * @param arg unused
 */
static void ICACHE_FLASH_ATTR menuPanImages(void* arg __attribute__((unused)))
{
    // Every MENU_PAN_PERIOD_MS, pan the menu MENU_PX_PER_PAN pixels
    // With 4px every 20ms, a transition takes 640ms
    if(mnu->panningLeft)
    {
        mnu->panIdx -= MENU_PX_PER_PAN;
        if(mnu->panIdx < -OLED_WIDTH)
        {
            mnu->panIdx = -OLED_WIDTH;
        }
        drawGifFromAsset(mnu->curImg, mnu->panIdx, 0, false, false, 0, false);
        drawGifFromAsset(mnu->nextImg, mnu->panIdx + OLED_WIDTH, 0, false, false, 0, false);
    }
    else
    {
        mnu->panIdx += MENU_PX_PER_PAN;
        if(mnu->panIdx > OLED_WIDTH)
        {
            mnu->panIdx = OLED_WIDTH;
        }
        drawGifFromAsset(mnu->curImg, mnu->panIdx, 0, false, false, 0, false);
        drawGifFromAsset(mnu->nextImg, mnu->panIdx - OLED_WIDTH, 0, false, false, 0, false);
    }
    mnuDrawArrows();

    // Check if it's all done
    if(mnu->panIdx == -OLED_WIDTH || mnu->panIdx == OLED_WIDTH)
    {
        // Swap mnu->curImg and mnu->nextImg
        gifHandle* tmp;
        tmp = mnu->curImg;
        mnu->curImg = mnu->nextImg;
        mnu->nextImg = tmp;

        // stop the timer
        timerDisarm(&mnu->timerPanning);
        mnu->menuIsPanning = false;
    }
}

/*==============================================================================
 * Screensaver functions
 *============================================================================*/

void ICACHE_FLASH_ATTR plotSquareWave (int16_t x, int16_t y)
{
    // Starting point for the line
    int16_t pt1x = x;
    int16_t pt1y = y + (OLED_HEIGHT / 2) + (SQ_WAVE_LINE_LEN / 2);

    // Ending point for the line
    int16_t pt2x = x;
    int16_t pt2y = y + (OLED_HEIGHT / 2) - (SQ_WAVE_LINE_LEN / 2);

    // Direction the square wave is traveling
    sqDir_t sqDir = M_RIGHT;

    // Draw a square wave, one line at a time
    //uint8_t segments;
    //for(segments = 0; segments < ((2 * OLED_WIDTH) / SQ_WAVE_LINE_LEN) - 1; segments++)
    while (pt1x <= OLED_WIDTH)
    {
        // Draw the line
        plotLine(pt1x, pt1y, pt2x, pt2y, WHITE);

        // Move the starting point of the line
        pt1x = pt2x;
        pt1y = pt2y;

        // Move the ending point of the line
        switch (sqDir)
        {
            case M_UP:
            {
                pt2y -= SQ_WAVE_LINE_LEN;
                break;
            }
            case M_DOWN:
            {
                pt2y += SQ_WAVE_LINE_LEN;
                break;
            }
            case M_RIGHT:
            case M_RIGHT2:
            {
                pt2x += SQ_WAVE_LINE_LEN;
                break;
            }
            case M_NUM_DIRS:
            default:
            {
                break;
            }
        }

        // Move to the next part of the square wave
        sqDir = (sqDir + 1) % M_NUM_DIRS;
    }
}

/**
 * @brief Called on a timer if there's no user input to start a screensaver
 *
 * @param arg unused
 */
static void ICACHE_FLASH_ATTR menuStartScreensaver(void* arg __attribute__((unused)))
{
    // Pick a random screensaver from a reduced list of dances (missing 12, 13, 18, 19)
    // static const uint8_t acceptableDances[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 14, 15, 16, 17};
    static const uint8_t acceptableDances[] = {17};
    mnu->menuScreensaverIdx = acceptableDances[os_random() % sizeof(acceptableDances)];

    // Set the brightness to low
    setDanceBrightness(1);

    // Animate it at the given period
    timerArm(&mnu->timerScreensaverLEDAnimation, danceTimers[mnu->menuScreensaverIdx].period, true);

    // Animate the OLED at the given period
    timerArm(&mnu->timerScreensaverOLEDAnimation, MENU_PAN_PERIOD_MS, true);

    mnu->drawOLEDScreensaver = 0;

    // Start a timer to turn the screensaver brighter
    timerArm(&mnu->timerScreensaverBright, 1000, false);
}

/**
 * @brief Five seconds after starting the screensaver, clear the OLED and
 *        make the LEDs one step brighter
 *
 * @param arg unused
 */
static void ICACHE_FLASH_ATTR menuBrightScreensaver(void* arg __attribute__((unused)))
{
    // Clear the display
    clearDisplay();

    mnu->squareWaveScrollOffset = 0;
    plotSquareWave(mnu->squareWaveScrollOffset, 0);

    // Plot some tiny corner text
    plotText(0, OLED_HEIGHT - FONT_HEIGHT_TOMTHUMB, "Swadge 2021", TOM_THUMB, WHITE);

    mnu->drawOLEDScreensaver = 1;

    // Set the brightness to medium
    setDanceBrightness(0);
}

/**
 * @brief Called on a timer to animate a screensaver (the LEDs portion)
 *
 * @param arg unused
 */
static void ICACHE_FLASH_ATTR menuAnimateScreensaverLEDs(void* arg __attribute__((unused)))
{
    // Animation!
    danceTimers[mnu->menuScreensaverIdx].timerFn(NULL);
}

/**
 * @brief Called on a timer to animate a screensaver (the OLED portion)
 *
 * @param arg unused
 */
static void ICACHE_FLASH_ATTR menuAnimateScreensaverOLED(void* arg __attribute__((unused)))
{
    if (mnu->drawOLEDScreensaver)
    {
        // Clear the display
        clearDisplay();

        // Plot scrolling square wave
        mnu->squareWaveScrollOffset += mnu->squareWaveScrollSpeed;
        mnu->squareWaveScrollOffset = mnu->squareWaveScrollOffset % (SQ_WAVE_LINE_LEN * 2);
        plotSquareWave(mnu->squareWaveScrollOffset, 0);

        // Plot some tiny corner text
        plotText(0, OLED_HEIGHT - FONT_HEIGHT_TOMTHUMB, "Swadge 2021", TOM_THUMB, WHITE);
    }
}

/**
 * @brief Stop the screensaver and set it up to run again if idle
 *
 */
void ICACHE_FLASH_ATTR stopScreensaver(void)
{
    // Stop the current screensaver
    timerDisarm(&mnu->timerScreensaverLEDAnimation);
    timerDisarm(&mnu->timerScreensaverOLEDAnimation);
    led_t leds[NUM_LIN_LEDS] = {{0}};
    setLeds(leds, sizeof(leds));
    mnu->drawOLEDScreensaver = 0;

#if SWADGE_VERSION != SWADGE_BBKIWI
    // Start a timer to start the screensaver if there's no input
    timerDisarm(&mnu->timerScreensaverStart);
    timerArm(&mnu->timerScreensaverStart, 1000, false);
#endif
    // Stop this timer too
    timerDisarm(&mnu->timerScreensaverBright);
}

/**
 * @brief Draw some button function arrows
 */
void ICACHE_FLASH_ATTR mnuDrawArrows(void)
{
    // Draw left and right arrows to indicate button functions
    fillDisplayArea(0, OLED_HEIGHT - FONT_HEIGHT_TOMTHUMB - 1, 3, OLED_HEIGHT, BLACK);
    plotText(0, OLED_HEIGHT - FONT_HEIGHT_TOMTHUMB, "<", TOM_THUMB, WHITE);
    fillDisplayArea(OLED_WIDTH - 4, OLED_HEIGHT - FONT_HEIGHT_TOMTHUMB - 1, OLED_WIDTH, OLED_HEIGHT, BLACK);
    plotText(OLED_WIDTH - 3, OLED_HEIGHT - FONT_HEIGHT_TOMTHUMB, ">", TOM_THUMB, WHITE);
}
