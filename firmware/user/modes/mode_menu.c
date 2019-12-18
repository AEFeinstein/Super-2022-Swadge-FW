/*============================================================================
 * Includes
 *==========================================================================*/

#include "osapi.h"
#include "user_main.h"
#include "mode_menu.h"
#include "display/oled.h"
#include "display/font.h"
#include "custom_commands.h"
#include "mode_dance.h"
#include "fastlz.h"
#include "mode_gallery.h"
#include "buttons.h"
#include "bresenham.h"
#include "font.h"

/*============================================================================
 * Defines
 *==========================================================================*/

#define MENU_PAN_PERIOD_MS 20
#define MENU_PX_PER_PAN     8
#define SQ_WAVE_LINE_LEN 16

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

void ICACHE_FLASH_ATTR modeInit(void);
void ICACHE_FLASH_ATTR modeButtonCallback(uint8_t state, int button, int down);

void ICACHE_FLASH_ATTR plotSquareWave(int16_t x, int16_t y);
static void ICACHE_FLASH_ATTR menuStartScreensaver(void* arg __attribute__((unused)));
static void ICACHE_FLASH_ATTR menuBrightScreensaver(void* arg __attribute__((unused)));
static void ICACHE_FLASH_ATTR menuAnimateScreensaverLEDs(void* arg __attribute__((unused)));
static void ICACHE_FLASH_ATTR menuAnimateScreensaverOLED(void* arg __attribute__((unused)));
void ICACHE_FLASH_ATTR stopScreensaver(void);

void ICACHE_FLASH_ATTR loadImg(swadgeMode* mode, uint8_t* decompressedImage);
void ICACHE_FLASH_ATTR drawImgAtOffset(uint8_t* img, int8_t hOffset);

void ICACHE_FLASH_ATTR startPanning(bool pLeft);
static void ICACHE_FLASH_ATTR menuPanImages(void* arg __attribute__((unused)));
void ICACHE_FLASH_ATTR mnuDrawArrows(void);

/*============================================================================
 * Variables
 *==========================================================================*/

swadgeMode menuMode =
{
    .modeName = "menu",
    .fnEnterMode = modeInit,
    .fnExitMode = NULL,
    .fnButtonCallback = modeButtonCallback,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
};

// Dummy mode for the mute option
swadgeMode muteOptionOn =
{
    .menuImageData = mnu_muteon_0,
    .menuImageLen = sizeof(mnu_muteon_0)
};
swadgeMode muteOptionOff =
{
    .menuImageData = mnu_muteoff_0,
    .menuImageLen = sizeof(mnu_muteoff_0)
};

uint8_t numModes = 0;
swadgeMode** modes = NULL;
uint8_t selectedMode = 0;

os_timer_t timerScreensaverStart = {0};
os_timer_t timerScreensaverBright = {0};
os_timer_t timerScreensaverLEDAnimation = {0}; // animation for the LEDs.
os_timer_t timerScreensaverOLEDAnimation = {0}; // animation for the OLED.
uint8_t menuScreensaverIdx = 0;
int16_t squareWaveScrollOffset = 0;
int16_t squareWaveScrollSpeed = -1; // expressed as pixels per frame.
uint8_t drawOLEDScreensaver = 0; // only draw after bright screensaver.

uint8_t compressedStagingSpace[1000] = {0};
uint8_t img1[((OLED_WIDTH * OLED_HEIGHT) / 8) + METADATA_LEN] = {0};
uint8_t img2[((OLED_WIDTH * OLED_HEIGHT) / 8) + METADATA_LEN] = {0};
uint8_t* curImg = img1;
uint8_t* nextImg = img2;

os_timer_t timerPanning = {0};
bool menuIsPanning = false;
bool panningLeft = false;
int16_t panIdx = 0;

/*============================================================================
 * Swadge Mode Functions
 *==========================================================================*/

/**
 * Initialize the menu by getting the list of modes from user_main.c
 */
void ICACHE_FLASH_ATTR modeInit(void)
{
    // Get the list of modes
    numModes = getSwadgeModes(&modes);
    // Don't count the menu as a mode
    numModes--;

    // Start where we left off
    selectedMode = getMenuPos();

    // Set up memory for loading images into
    curImg = img1;
    nextImg = img2;

    // Load and draw the first image
    loadImg(modes[1 + selectedMode], curImg);
    drawImgAtOffset(curImg, 0);
    mnuDrawArrows();

    // Timer for starting a screensaver
    os_timer_disarm(&timerScreensaverStart);
    os_timer_setfn(&timerScreensaverStart, (os_timer_func_t*)menuStartScreensaver, NULL);

    // Timer for starting a screensaver
    os_timer_disarm(&timerScreensaverBright);
    os_timer_setfn(&timerScreensaverBright, (os_timer_func_t*)menuBrightScreensaver, NULL);

    // Timer for running a screensaver
    os_timer_disarm(&timerScreensaverLEDAnimation);
    os_timer_setfn(&timerScreensaverLEDAnimation, (os_timer_func_t*)menuAnimateScreensaverLEDs, NULL);

    // Timer for running a screensaver
    os_timer_disarm(&timerScreensaverOLEDAnimation);
    os_timer_setfn(&timerScreensaverOLEDAnimation, (os_timer_func_t*)menuAnimateScreensaverOLED, NULL);

    // Timer for running a screensaver
    os_timer_disarm(&timerPanning);
    os_timer_setfn(&timerPanning, (os_timer_func_t*)menuPanImages, NULL);

    // This starts the screensaver timer
    stopScreensaver();

    // Make buttons sensitive, they're ignored during animation anyway
    enableDebounce(false);
}

/**
 * Handle the button. Left press selects the mode, right press starts it
 *
 * @param state  A bitmask of all buttons, unused
 * @param button The button that was just pressed or released
 * @param down   true if the button was pressed, false if it was released
 */
void ICACHE_FLASH_ATTR modeButtonCallback(uint8_t state __attribute__((unused)),
        int button, int down)
{
    // Stop the screensaver
    stopScreensaver();

    // Don't accept button input if the menu is panning
    if(menuIsPanning)
    {
        return;
    }

    // Menu is not panning
    if(down)
    {
        switch(button)
        {
            case 0:
            {
                // Special handling for the mute mode
                if(modes[1 + selectedMode] == &muteOptionOff)
                {
                    // Toggle the mute and redraw the menu
                    setIsMutedOption(!getIsMutedOption());

                    // Pick one of the mute images to draw
                    swadgeMode* modeToDraw;
                    if(getIsMutedOption())
                    {
                        modeToDraw = &muteOptionOn;
                    }
                    else
                    {
                        modeToDraw = &muteOptionOff;
                    }
                    // Load and draw the mute image
                    loadImg(modeToDraw, curImg);
                    drawImgAtOffset(curImg, 0);
                    mnuDrawArrows();
                }
                else
                {
                    // Select the mode
                    setMenuPos(selectedMode);
                    switchToSwadgeMode(1 + selectedMode);
                }
                break;
            }
            case 2:
            {
                // Cycle the currently selected mode
                selectedMode = (selectedMode + 1) % numModes;

                startPanning(true);
                break;
            }
            case 1:
            {
                // Cycle the currently selected mode
                if(0 == selectedMode)
                {
                    selectedMode = numModes - 1;
                }
                else
                {
                    selectedMode--;
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
 * Image utility functions
 *============================================================================*/

/**
 * @brief Helper function to load an image from ROM
 *
 * @param mode The mode who's menu image to load
 * @param decompressedImage The memory to load the image to
 */
void ICACHE_FLASH_ATTR loadImg(swadgeMode* mode, uint8_t* decompressedImage)
{
    /* Read the compressed image from ROM into RAM, and make sure to do a
     * 32 bit aligned read. The arrays are all __attribute__((aligned(4)))
     * so this is safe, not out of bounds
     */
    uint32_t alignedSize = mode->menuImageLen;
    while(alignedSize % 4 != 0)
    {
        alignedSize++;
    }
    memcpy(compressedStagingSpace, mode->menuImageData, alignedSize);

    // Decompress the image from one RAM area to another
    fastlz_decompress(compressedStagingSpace,
                      mode->menuImageLen,
                      decompressedImage,
                      1024 + 8);
}

/**
 * @brief Draw a menu image at a given horizontal offset
 *
 * @param img The image to draw
 * @param hOffset The horizontal offset to draw it at
 */
void ICACHE_FLASH_ATTR drawImgAtOffset(uint8_t* img, int8_t wOffset)
{
    for (int w = 0; w < OLED_WIDTH; w++)
    {
        for (int h = 0; h < OLED_HEIGHT; h++)
        {
            uint16_t linearIdx = (OLED_HEIGHT * (w)) + h;
            uint16_t byteIdx = METADATA_LEN + linearIdx / 8;
            uint8_t bitIdx = linearIdx % 8;

            if (img[byteIdx] & (0x80 >> bitIdx))
            {
                drawPixel(w + wOffset, h, WHITE);
            }
            else
            {
                drawPixel(w + wOffset, h, BLACK);
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
    menuIsPanning = true;

    // Special handling for mute, pick the actual mode to draw an image for
    swadgeMode* newMode;
    if(&muteOptionOff == modes[1 + selectedMode])
    {
        if(getIsMutedOption())
        {
            newMode = &muteOptionOn;
        }
        else
        {
            newMode = &muteOptionOff;
        }
    }
    else
    {
        newMode = modes[1 + selectedMode];
    }

    // Load the next image
    loadImg(newMode, nextImg);

    // Start the timer to pan
    panningLeft = pLeft;
    panIdx = 0;
    os_timer_arm(&timerPanning, MENU_PAN_PERIOD_MS, true);
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
    if(panningLeft)
    {
        panIdx -= MENU_PX_PER_PAN;
        if(panIdx < -OLED_WIDTH)
        {
            panIdx = -OLED_WIDTH;
        }
        drawImgAtOffset(curImg, panIdx);
        drawImgAtOffset(nextImg, panIdx + OLED_WIDTH);
    }
    else
    {
        panIdx += MENU_PX_PER_PAN;
        if(panIdx > OLED_WIDTH)
        {
            panIdx = OLED_WIDTH;
        }
        drawImgAtOffset(curImg, panIdx);
        drawImgAtOffset(nextImg, panIdx - OLED_WIDTH);
    }
    mnuDrawArrows();

    // Check if it's all done
    if(panIdx == -OLED_WIDTH || panIdx == OLED_WIDTH)
    {
        // Swap curImg and nextImg
        uint8_t* tmp;
        tmp = curImg;
        curImg = nextImg;
        nextImg = tmp;

        // stop the timer
        os_timer_disarm(&timerPanning);
        menuIsPanning = false;
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
    static const uint8_t acceptableDances[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 14, 15, 16, 17};
    menuScreensaverIdx = acceptableDances[os_random() % sizeof(acceptableDances)];

    // Set the brightness to low
    setDanceBrightness(2);

    // Animate it at the given period
    os_timer_arm(&timerScreensaverLEDAnimation, danceTimers[menuScreensaverIdx].period, true);

    // Animate the OLED at the given period
    os_timer_arm(&timerScreensaverOLEDAnimation, MENU_PAN_PERIOD_MS, true);

    drawOLEDScreensaver = 0;

    // Start a timer to turn the screensaver brighter
    os_timer_arm(&timerScreensaverBright, 5000, false);
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

    squareWaveScrollOffset = 0;
    plotSquareWave(squareWaveScrollOffset, 0);

    // Plot some tiny corner text
    plotText(0, OLED_HEIGHT - FONT_HEIGHT_TOMTHUMB, "Swadge 2020", TOM_THUMB, WHITE);

    drawOLEDScreensaver = 1;

    // Set the brightness to medium
    setDanceBrightness(1);
}

/**
 * @brief Called on a timer to animate a screensaver (the LEDs portion)
 *
 * @param arg unused
 */
static void ICACHE_FLASH_ATTR menuAnimateScreensaverLEDs(void* arg __attribute__((unused)))
{
    // Animation!
    danceTimers[menuScreensaverIdx].timerFn(NULL);
}

/**
 * @brief Called on a timer to animate a screensaver (the OLED portion)
 *
 * @param arg unused
 */
static void ICACHE_FLASH_ATTR menuAnimateScreensaverOLED(void* arg __attribute__((unused)))
{
    if (drawOLEDScreensaver)
    {
        // Clear the display
        clearDisplay();

        // Plot scrolling square wave
        squareWaveScrollOffset += squareWaveScrollSpeed;
        squareWaveScrollOffset = squareWaveScrollOffset % (SQ_WAVE_LINE_LEN * 2);
        plotSquareWave(squareWaveScrollOffset, 0);

        // Plot some tiny corner text
        plotText(0, OLED_HEIGHT - FONT_HEIGHT_TOMTHUMB, "Swadge 2020", TOM_THUMB, WHITE);
    }
}

/**
 * @brief Stop the screensaver and set it up to run again if idle
 *
 */
void ICACHE_FLASH_ATTR stopScreensaver(void)
{
    // Stop the current screensaver
    os_timer_disarm(&timerScreensaverLEDAnimation);
    os_timer_disarm(&timerScreensaverOLEDAnimation);
    led_t leds[NUM_LIN_LEDS] = {{0}};
    setLeds(leds, sizeof(leds));
    drawOLEDScreensaver = 0;

#if SWADGE_VERSION != SWADGE_BBKIWI
    // Start a timer to start the screensaver if there's no input
    os_timer_disarm(&timerScreensaverStart);
    os_timer_arm(&timerScreensaverStart, 5000, false);
#endif
    // Stop this timer too
    os_timer_disarm(&timerScreensaverBright);
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
