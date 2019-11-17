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

/*============================================================================
 * Defines
 *==========================================================================*/

#define MENU_PAN_PERIOD_MS 20
#define MENU_PX_PER_PAN     4

/*============================================================================
 * Function prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR modeInit(void);
void ICACHE_FLASH_ATTR modeButtonCallback(uint8_t state, int button, int down);

static void ICACHE_FLASH_ATTR menuStartScreensaver(void* arg __attribute__((unused)));
static void ICACHE_FLASH_ATTR menuAnimateScreensaver(void* arg __attribute__((unused)));
void ICACHE_FLASH_ATTR stopScreensaver(void);

void ICACHE_FLASH_ATTR loadImg(swadgeMode* mode, uint8_t* decompressedImage);
void ICACHE_FLASH_ATTR drawImgAtOffset(uint8_t* img, int8_t hOffset);

void ICACHE_FLASH_ATTR startPanning(bool pLeft);
static void ICACHE_FLASH_ATTR menuPanImages(void* arg __attribute__((unused)));

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

#if SWADGE_VERSION != SWADGE_BBKIWI
os_timer_t timerScreensaverStart = {0};
os_timer_t timerScreensaverAnimation = {0};
uint8_t menuScreensaverIdx = 0;
#endif

uint8_t compressedStagingSpace[600] = {0};
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

    // Start at mode 0
    selectedMode = 0;

    // Set up memory for loading images into
    curImg = img1;
    nextImg = img2;

    // Load and draw the first image
    loadImg(modes[1 + selectedMode], curImg);
    drawImgAtOffset(curImg, 0);

#if SWADGE_VERSION != SWADGE_BBKIWI
    // Timer for starting a screensaver
    os_timer_disarm(&timerScreensaverStart);
    os_timer_setfn(&timerScreensaverStart, (os_timer_func_t*)menuStartScreensaver, NULL);

    // Timer for running a screensaver
    os_timer_disarm(&timerScreensaverAnimation);
    os_timer_setfn(&timerScreensaverAnimation, (os_timer_func_t*)menuAnimateScreensaver, NULL);

    // Timer for running a screensaver
    os_timer_disarm(&timerPanning);
    os_timer_setfn(&timerPanning, (os_timer_func_t*)menuPanImages, NULL);

    // This starts the screensaver timer
    stopScreensaver();
#endif

    // Draw to OLED at the same rate the image is panned
    setOledDrawTime(MENU_PAN_PERIOD_MS);
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
#if SWADGE_VERSION != SWADGE_BBKIWI
    // Stop the screensaver
    stopScreensaver();
#endif

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
                }
                else
                {
                    // Select the mode
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

#if SWADGE_VERSION != SWADGE_BBKIWI
/*==============================================================================
 * Screensaver functions
 *============================================================================*/

/**
 * @brief Called on a timer if there's no user input to start a screensaver
 *
 * @param arg unused
 */
static void ICACHE_FLASH_ATTR menuStartScreensaver(void* arg __attribute__((unused)))
{
    // Pick a random screensaver
    menuScreensaverIdx = os_random() % getNumDances();

    // Animate it at the given period
    os_timer_disarm(&timerScreensaverStart);
    os_timer_arm(&timerScreensaverAnimation, danceTimers[menuScreensaverIdx].period, true);
}

/**
 * @brief Called on a timer to animate a screensaver
 *
 * @param arg unused
 */
static void ICACHE_FLASH_ATTR menuAnimateScreensaver(void* arg __attribute__((unused)))
{
    // Animation!
    danceTimers[menuScreensaverIdx].timerFn(NULL);
}

/**
 * @brief Stop the screensaver and set it up to run again if idle
 *
 */
void ICACHE_FLASH_ATTR stopScreensaver(void)
{
    // Stop the current screensaver
    os_timer_disarm(&timerScreensaverAnimation);
    led_t leds[NUM_LIN_LEDS] = {{0}};
    setLeds(leds, sizeof(leds));

    // Start a timer to start the screensaver if there's no input
    os_timer_disarm(&timerScreensaverStart);
    // os_timer_arm(&timerScreensaverStart, 5000, false);
}
#endif
