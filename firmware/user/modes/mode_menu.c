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
 * Function prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR modeInit(void);
void ICACHE_FLASH_ATTR modeButtonCallback(uint8_t state, int button, int down);
void ICACHE_FLASH_ATTR drawMenu(void);

static void ICACHE_FLASH_ATTR menuStartScreensaver(void* arg __attribute__((unused)));
static void ICACHE_FLASH_ATTR menuAnimateScreensaver(void* arg __attribute__((unused)));
void ICACHE_FLASH_ATTR stopScreensaver(void);
void ICACHE_FLASH_ATTR loadImg(swadgeMode* mode, uint8_t* decompressedImage);

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
os_timer_t timerScreensaverAnimation = {0};
uint8_t menuScreensaverIdx = 0;

uint8_t compressedStagingSpace[600];
uint8_t img1[((OLED_WIDTH * OLED_HEIGHT) / 8) + METADATA_LEN];
uint8_t img2[((OLED_WIDTH * OLED_HEIGHT) / 8) + METADATA_LEN];

/*============================================================================
 * Variables
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

    selectedMode = 0;

    // Timer for starting a screensaver
    os_timer_disarm(&timerScreensaverStart);
    os_timer_setfn(&timerScreensaverStart, (os_timer_func_t*)menuStartScreensaver, NULL);

    // Timer for running a screensaver
    os_timer_disarm(&timerScreensaverAnimation);
    os_timer_setfn(&timerScreensaverAnimation, (os_timer_func_t*)menuAnimateScreensaver, NULL);

    stopScreensaver();

    drawMenu();
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

    if(down)
    {
        switch(button)
        {
            case 0:
            {
                if(modes[1 + selectedMode] == &muteOptionOff)
                {
                    // Toggle the mute and redraw the menu
                    setIsMutedOption(!getIsMutedOption());
                    drawMenu();
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

                // Draw the menu
                drawMenu();
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

                // Draw the menu
                drawMenu();
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

/**
 * Draw a cursor and all the mode names to the display
 */
void ICACHE_FLASH_ATTR drawMenu(void)
{
    swadgeMode* modeToDraw;
    if(&muteOptionOff == modes[1 + selectedMode])
    {
        if(getIsMutedOption())
        {
            modeToDraw = &muteOptionOn;
        }
        else
        {
            modeToDraw = &muteOptionOff;
        }
    }
    else
    {
        modeToDraw = modes[1 + selectedMode];
    }

    loadImg(modeToDraw, img1);

    // Draw the frame to the OLED, one pixel at a time
    for (int w = 0; w < OLED_WIDTH; w++)
    {
        for (int h = 0; h < OLED_HEIGHT; h++)
        {
            uint16_t linearIdx = (OLED_HEIGHT * (w)) + h;
            uint16_t byteIdx = METADATA_LEN + linearIdx / 8;
            uint8_t bitIdx = linearIdx % 8;

            if (img1[byteIdx] & (0x80 >> bitIdx))
            {
                drawPixel(w, h, WHITE);
            }
            else
            {
                drawPixel(w, h, BLACK);
            }
        }
    }
}

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

/**
 * @brief Helper function to load an image from ROM
 *
 * @param mode The mode who's menu image t load
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
    uint32_t dLen = fastlz_decompress(compressedStagingSpace,
                                      mode->menuImageLen,
                                      decompressedImage,
                                      1024 + 8);
    uint8_t width      = (decompressedImage[0] << 8) | decompressedImage[1];
    uint8_t height     = (decompressedImage[2] << 8) | decompressedImage[3];
    uint8_t nFrames    = (decompressedImage[4] << 8) | decompressedImage[5];
    uint8_t durationMs = (decompressedImage[6] << 8) | decompressedImage[7];
    os_printf("2=%d\nh=%d\nf=%d\nd=%d\ndLen=%d\n", width, height, nFrames, durationMs, dLen);
}