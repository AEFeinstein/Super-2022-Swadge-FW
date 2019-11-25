/*
 * mode_gallery.c
 *
 *  Created on: Oct 13, 2019
 *      Author: adam
 */

/*==============================================================================
 * Includes
 *============================================================================*/

#include <osapi.h>
#include <mem.h>

#include "user_main.h"
#include "oled.h"
#include "mode_gallery.h"
#include "galleryImages.h"
#include "fastlz.h"
#include "font.h"
#include "custom_commands.h"

/*==============================================================================
 * Defines
 *============================================================================*/

#define MAX_DECOMPRESSED_SIZE 0xA00

/*
#define DBG_GAL(...) do { \
        os_printf("%s::%d ", __func__, __LINE__); \
        os_printf(__VA_ARGS__); \
    } while(0)
*/
#define DBG_GAL(...)

/*==============================================================================
 * Enums
 *============================================================================*/

typedef enum
{
    RIGHT,
    LEFT
} panDir_t;

typedef enum
{
    NONE,
    ALWAYS_RIGHT,
    ALWAYS_LEFT
} panContDir_t;

/*==============================================================================
 * Structs
 *============================================================================*/

typedef struct
{
    const uint8_t* data;
    const uint16_t len;
} galFrame_t;

typedef struct
{
    const uint8_t nFrames;
    const panContDir_t continousPan;
    const galFrame_t frames[];
} galImage_t;

/*==============================================================================
 * Prototypes
 *============================================================================*/

void ICACHE_FLASH_ATTR galEnterMode(void);
void ICACHE_FLASH_ATTR galExitMode(void);
void ICACHE_FLASH_ATTR galButtonCallback(uint8_t state, int button, int down);
void ICACHE_FLASH_ATTR galLoadFirstFrame(void);
void ICACHE_FLASH_ATTR galClearImage(void);
void ICACHE_FLASH_ATTR galDrawFrame(void);
static void ICACHE_FLASH_ATTR galLoadNextFrame(void* arg);
static void ICACHE_FLASH_ATTR galTimerPan(void* arg);
const galImage_t* ICACHE_FLASH_ATTR galGetCurrentImage(void);
bool ICACHE_FLASH_ATTR galIsImageUnlocked(void);

/*==============================================================================
 * Variables
 *============================================================================*/

swadgeMode galleryMode =
{
    .modeName = "gallery",
    .fnEnterMode = galEnterMode,
    .fnExitMode = galExitMode,
    .fnButtonCallback = galButtonCallback,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = NULL,
    .menuImageData = mnu_gallery_0,
    .menuImageLen = sizeof(mnu_gallery_0)
};

struct
{
    uint8_t* compressedData;   ///< A pointer to compressed data in RAM
    uint8_t* decompressedData; ///< A pointer to decompressed data in RAM
    uint8_t* frameData;        ///< A pointer to the frame being displayed
    uint16_t width;            ///< The width of the current image
    uint16_t virtualWidth;     ///< The effective width = actual width or double that if continous panning
    uint16_t height;           ///< The height of the current image
    uint16_t nFrames;          ///< The number of frames in the image
    uint16_t cFrame;           ///< The current frame index being displyed
    uint16_t durationMs;       ///< The duration each frame should be displayed
    os_timer_t timerAnimate;   ///< A timer to animate the image
    os_timer_t timerPan;       ///< A timer to pan the image
    uint16_t cImage;           ///< The index of the current image being
    uint16_t panIdx;           ///< How much the image is currently panned
    panDir_t panDir;           ///< The direction the image is currently panning
    uint32_t unlockBitmask;    ///< A bitmask of the unlocked gallery images
} gal =
{
    .compressedData = NULL,
    .decompressedData = NULL,
    .frameData = NULL,
    .width = 0,
    .height = 0,
    .nFrames = 0,
    .cFrame = 0,
    .durationMs = 0,
    .timerAnimate = {0},
    .timerPan = {0},
    .cImage = 0,
    .panIdx = 0,
    .panDir = RIGHT
};

/*==============================================================================
 * Const Variables
 *============================================================================*/

const galImage_t galBongo =
{
    .nFrames = 2,
    .continousPan = NONE,
    .frames = {
        {.data = gal_bongo_0, .len = sizeof(gal_bongo_0)},
        {.data = gal_bongo_1, .len = sizeof(gal_bongo_1)},
    }
};

const galImage_t galSnort =
{
    .nFrames = 1,
    .continousPan = ALWAYS_LEFT,
    .frames = {
        {.data = gal_snort_0, .len = sizeof(gal_snort_0)},
    }
};

const galImage_t galGaylord =
{
    .nFrames = 3,
    .continousPan = NONE,
    .frames = {
        {.data = gal_gaylord_0, .len = sizeof(gal_gaylord_0)},
        {.data = gal_gaylord_1, .len = sizeof(gal_gaylord_1)},
        {.data = gal_gaylord_2, .len = sizeof(gal_gaylord_2)},
    }
};

const galImage_t galFunkus =
{
    .nFrames = 30,
    .continousPan = NONE,
    .frames = {
        {.data = gal_funkus_0, .len = sizeof(gal_funkus_0)},
        {.data = gal_funkus_1, .len = sizeof(gal_funkus_1)},
        {.data = gal_funkus_2, .len = sizeof(gal_funkus_2)},
        {.data = gal_funkus_3, .len = sizeof(gal_funkus_3)},
        {.data = gal_funkus_4, .len = sizeof(gal_funkus_4)},
        {.data = gal_funkus_5, .len = sizeof(gal_funkus_5)},
        {.data = gal_funkus_6, .len = sizeof(gal_funkus_6)},
        {.data = gal_funkus_7, .len = sizeof(gal_funkus_7)},
        {.data = gal_funkus_8, .len = sizeof(gal_funkus_8)},
        {.data = gal_funkus_9, .len = sizeof(gal_funkus_9)},
        {.data = gal_funkus_10, .len = sizeof(gal_funkus_10)},
        {.data = gal_funkus_11, .len = sizeof(gal_funkus_11)},
        {.data = gal_funkus_12, .len = sizeof(gal_funkus_12)},
        {.data = gal_funkus_13, .len = sizeof(gal_funkus_13)},
        {.data = gal_funkus_14, .len = sizeof(gal_funkus_14)},
        {.data = gal_funkus_15, .len = sizeof(gal_funkus_15)},
        {.data = gal_funkus_16, .len = sizeof(gal_funkus_16)},
        {.data = gal_funkus_17, .len = sizeof(gal_funkus_17)},
        {.data = gal_funkus_18, .len = sizeof(gal_funkus_18)},
        {.data = gal_funkus_19, .len = sizeof(gal_funkus_19)},
        {.data = gal_funkus_20, .len = sizeof(gal_funkus_20)},
        {.data = gal_funkus_21, .len = sizeof(gal_funkus_21)},
        {.data = gal_funkus_22, .len = sizeof(gal_funkus_22)},
        {.data = gal_funkus_23, .len = sizeof(gal_funkus_23)},
        {.data = gal_funkus_24, .len = sizeof(gal_funkus_24)},
        {.data = gal_funkus_25, .len = sizeof(gal_funkus_25)},
        {.data = gal_funkus_26, .len = sizeof(gal_funkus_26)},
        {.data = gal_funkus_27, .len = sizeof(gal_funkus_27)},
        {.data = gal_funkus_28, .len = sizeof(gal_funkus_28)},
        {.data = gal_funkus_29, .len = sizeof(gal_funkus_29)},
    }
};

const galImage_t galLogo =
{
    .nFrames = 21,
    .continousPan = NONE,
    .frames = {
        {.data = gal_logo_0, .len = sizeof(gal_logo_0)},
        {.data = gal_logo_1, .len = sizeof(gal_logo_1)},
        {.data = gal_logo_2, .len = sizeof(gal_logo_2)},
        {.data = gal_logo_3, .len = sizeof(gal_logo_3)},
        {.data = gal_logo_4, .len = sizeof(gal_logo_4)},
        {.data = gal_logo_5, .len = sizeof(gal_logo_5)},
        {.data = gal_logo_6, .len = sizeof(gal_logo_6)},
        {.data = gal_logo_7, .len = sizeof(gal_logo_7)},
        {.data = gal_logo_8, .len = sizeof(gal_logo_8)},
        {.data = gal_logo_9, .len = sizeof(gal_logo_9)},
        {.data = gal_logo_10, .len = sizeof(gal_logo_10)},
        {.data = gal_logo_11, .len = sizeof(gal_logo_11)},
        {.data = gal_logo_12, .len = sizeof(gal_logo_12)},
        {.data = gal_logo_13, .len = sizeof(gal_logo_13)},
        {.data = gal_logo_14, .len = sizeof(gal_logo_14)},
        {.data = gal_logo_15, .len = sizeof(gal_logo_15)},
        {.data = gal_logo_16, .len = sizeof(gal_logo_16)},
        {.data = gal_logo_17, .len = sizeof(gal_logo_17)},
        {.data = gal_logo_18, .len = sizeof(gal_logo_18)},
        {.data = gal_logo_19, .len = sizeof(gal_logo_19)},
        {.data = gal_logo_20, .len = sizeof(gal_logo_20)},
    }
};

const galImage_t galUnlockJoust =
{
    .nFrames = 1,
    .frames = {
        {.data = gal_unlock_joust_0, .len = sizeof(gal_unlock_joust_0)},
    }
};

const galImage_t galUnlockMaze =
{
    .nFrames = 1,
    .frames = {
        {.data = gal_unlock_maze_0, .len = sizeof(gal_unlock_maze_0)},
    }
};

const galImage_t galUnlockTiltrads =
{
    .nFrames = 1,
    .frames = {
        {.data = gal_unlock_tiltrads_0, .len = sizeof(gal_unlock_tiltrads_0)},
    }
};

const galImage_t galUnlockSnake =
{
    .nFrames = 1,
    .frames = {
        {.data = gal_unlock_snake_0, .len = sizeof(gal_unlock_snake_0)},
    }
};

// Order matters, must match galUnlockPlaceholders
const galImage_t* galImages[5] =
{
    &galLogo,    // Already unlocked
    &galBongo,   // Joust
    &galFunkus,  // Snake
    &galGaylord, // Tiltrads
    &galSnort    // Maze
};

const galImage_t* galUnlockPlaceholders[4] =
{
    &galUnlockJoust,
    &galUnlockSnake,
    &galUnlockTiltrads,
    &galUnlockMaze
};

/*==============================================================================
 * Functions
 *============================================================================*/

/**
 * Initializer for gallery. Allocate memory, set up timers, and load the first
 * frame of the first image
 */
void ICACHE_FLASH_ATTR galEnterMode(void)
{
    // Clear everything out, for safety
    memset(&gal, 0, sizeof(gal));

    // Allocate a bunch of memory for decompresed images
    gal.compressedData = os_malloc(MAX_DECOMPRESSED_SIZE);
    gal.decompressedData = os_malloc(MAX_DECOMPRESSED_SIZE);
    gal.frameData = os_malloc(MAX_DECOMPRESSED_SIZE);

    //Set up software timers for animation and panning
    os_timer_disarm(&gal.timerAnimate);
    os_timer_setfn(&gal.timerAnimate, (os_timer_func_t*)galLoadNextFrame, NULL);
    os_timer_disarm(&gal.timerPan);
    os_timer_setfn(&gal.timerPan, (os_timer_func_t*)galTimerPan, NULL);

    // Unlock one image by default
    gal.unlockBitmask = getGalleryUnlocks();

    // Draw the OLED as fast as the pan timer
    setOledDrawTime(25);

    // Load the image
    galLoadFirstFrame();
}

/**
 * Called when gallery is exited. Stop timers and free memory
 */
void ICACHE_FLASH_ATTR galExitMode(void)
{
    // Stop the timers
    os_timer_disarm(&gal.timerAnimate);
    os_timer_disarm(&gal.timerPan);

    // Free the memory
    os_free(gal.compressedData);
    os_free(gal.decompressedData);
    os_free(gal.frameData);
}

/**
 * Cycle between the images to display. The flow is to clear the current image,
 * increment to the next image, and load that iamge
 *
 * @param state  A bitmask of all button states, unused
 * @param button The button which triggered this event
 * @param down   true if the button was pressed, false if it was released
 */
void ICACHE_FLASH_ATTR galButtonCallback(uint8_t state __attribute__((unused)),
        int button __attribute__((unused)), int down __attribute__((unused)))
{
    if(down)
    {
        switch (button)
        {
            case 2:
            {
                // Right button
                galClearImage();

                // Iterate through the images
                gal.cImage = (gal.cImage + 1) %
                             (sizeof(galImages) / sizeof(galImages[0]));

                // Load it
                galLoadFirstFrame();
                break;
            }
            case 1:
            {
                // Left Button
                galClearImage();

                // Iterate through the images
                if(0 == gal.cImage)
                {
                    gal.cImage = (sizeof(galImages) / sizeof(galImages[0])) - 1;
                }
                else
                {
                    gal.cImage--;
                }

                // Load it
                galLoadFirstFrame();
                break;
            }
            default:
            {
                break;
            }
        }
    }
}

/**
 * @return true  if the image is unlocked
 * @return false if the image is locked
 */
bool ICACHE_FLASH_ATTR galIsImageUnlocked(void)
{
    if(gal.cImage > 0)
    {
        // Check to see if it's unlocked
        if(getGalleryUnlocks() & 1 << (gal.cImage - 1))
        {
            // unlocked
            return true;
        }
        else
        {
            // Not unlocked
            return false;
        }
    }
    else
    {
        // First image always unlocked
        return true;
    }
}

/**
 * @return a pointer to the current image to draw, takes into account unlocks
 */
const galImage_t* ICACHE_FLASH_ATTR galGetCurrentImage(void)
{
    const galImage_t* imageToLoad;
    // If we're not on the first image
    if(gal.cImage > 0)
    {
        // Check to see if it's unlocked
        if(getGalleryUnlocks() & 1 << (gal.cImage - 1))
        {
            // unlocked
            imageToLoad = galImages[gal.cImage];
        }
        else
        {
            // Not unlocked
            imageToLoad = galUnlockPlaceholders[gal.cImage - 1];
        }
    }
    else
    {
        // First image always unlocked
        imageToLoad = galImages[gal.cImage];
    }
    return imageToLoad;
}

/**
 * For the first frame of an image, load the compressed data from ROM to RAM,
 * decompress the data in RAM, save the metadata, then draw the frame to the
 * OLED
 */
void ICACHE_FLASH_ATTR galLoadFirstFrame(void)
{
    const galImage_t* imageToLoad = galGetCurrentImage();

    /* Read the compressed image from ROM into RAM, and make sure to do a
     * 32 bit aligned read. The arrays are all __attribute__((aligned(4)))
     * so this is safe, not out of bounds
     */
    uint32_t alignedSize = imageToLoad->frames[0].len;
    while(alignedSize % 4 != 0)
    {
        alignedSize++;
    }
    memcpy(gal.compressedData, imageToLoad->frames[0].data, alignedSize);

    // Decompress the image from one RAM area to another
    uint32_t dLen = fastlz_decompress(gal.compressedData,
                                      imageToLoad->frames[0].len,
                                      gal.decompressedData,
                                      MAX_DECOMPRESSED_SIZE);
    DBG_GAL("dLen=%d\n", dLen);

    // Save the metadata
    gal.width      = (gal.decompressedData[0] << 8) | gal.decompressedData[1];
    gal.height     = (gal.decompressedData[2] << 8) | gal.decompressedData[3];
    gal.nFrames    = (gal.decompressedData[4] << 8) | gal.decompressedData[5];
    gal.durationMs = (gal.decompressedData[6] << 8) | gal.decompressedData[7];

    // Save the pan direction
    switch(galImages[gal.cImage]->continousPan)
    {
        case ALWAYS_LEFT:
        {
            gal.virtualWidth = 2 * gal.width;
            gal.panDir = LEFT;
            break;
        }
        case ALWAYS_RIGHT:
        {
            gal.virtualWidth = 2 * gal.width;
            gal.panDir = RIGHT;
            break;
        }
        default:
        case NONE:
        {
            gal.virtualWidth = gal.width;
            break;
        }
    }

    // But never pan the placeholder images
    if(!galIsImageUnlocked())
    {
        gal.virtualWidth = gal.width;
        gal.panDir = NONE;
    }

    DBG_GAL("w=%d, h=%d, nfr=%d, dur=%d repeatw=%d\n", gal.width, gal.height, gal.nFrames,
            gal.durationMs, gal.virtualWidth);

    // Clear gal.frameData, then save the first actual frame
    memset(gal.frameData, 0, MAX_DECOMPRESSED_SIZE);
    memcpy(gal.frameData, &gal.decompressedData[METADATA_LEN], dLen - METADATA_LEN);

    // Set the current frame to 0
    gal.cFrame = 0;

    // Adjust the animation timer to this image's speed
    os_timer_disarm(&gal.timerAnimate);
    if(gal.nFrames > 1)
    {
        os_timer_arm(&gal.timerAnimate, gal.durationMs, true);
    }

    // Set up the panning timer if the image is wider that the OLED
    os_timer_disarm(&gal.timerPan);
    if(gal.virtualWidth > OLED_WIDTH)
    {
        // Pan one pixel every 25 ms for a faster pan
        os_timer_arm(&gal.timerPan, 25, true);
    }

    // Draw the first frame in it's entirety to the OLED
    galDrawFrame();
}

/**
 * For any frame besides the first frame of an image, load the compressed data
 * from ROM to RAM, decompress the data in RAM, modify the current frame with
 * the differences in the decompressed data, then draw the frame to the OLED
 *
 * @param arg Unused
 */
static void ICACHE_FLASH_ATTR galLoadNextFrame(void* arg __attribute__((unused)))
{
    // Increment the current frame
    gal.cFrame = (gal.cFrame + 1) % gal.nFrames;

    // If we're back to the first frame
    if(0 == gal.cFrame)
    {
        // Load and draw the whole frame
        galLoadFirstFrame();
    }
    else
    {
        const galImage_t* imageToLoad = galGetCurrentImage();
        /* Read the compressed image from ROM into RAM, and make sure to do a
        * 32 bit aligned read. The arrays are all __attribute__((aligned(4)))
        * so this is safe, not out of bounds
        */
        uint32_t alignedSize = imageToLoad->frames[gal.cFrame].len;
        while(alignedSize % 4 != 0)
        {
            alignedSize++;
        }
        memcpy(gal.compressedData, imageToLoad->frames[gal.cFrame].data, alignedSize);

        // Decompress the image
        uint32_t dLen = fastlz_decompress(gal.compressedData,
                                          imageToLoad->frames[gal.cFrame].len,
                                          gal.decompressedData,
                                          MAX_DECOMPRESSED_SIZE);
        DBG_GAL("dLen=%d\n", dLen);

        // Adjust only the differences
        for(uint32_t idx = 0; idx < dLen; idx++)
        {
            gal.frameData[idx] ^= gal.decompressedData[idx];
        }

        // Draw the frame
        galDrawFrame();
    }
}

/**
 * Clear all data in RAM from the current image, including the metadata
 * Also stop the timers
 */
void ICACHE_FLASH_ATTR galClearImage(void)
{
    // Stop timers
    os_timer_disarm(&gal.timerAnimate);
    os_timer_disarm(&gal.timerPan);

    // Zero memory, don't free it
    memset(gal.compressedData, 0, MAX_DECOMPRESSED_SIZE);
    memset(gal.decompressedData, 0, MAX_DECOMPRESSED_SIZE);
    memset(gal.frameData, 0, MAX_DECOMPRESSED_SIZE);

    // Clear variables
    gal.width = 0;
    gal.height = 0;
    gal.nFrames = 0;
    gal.cFrame = 0;
    gal.durationMs = 0;
    // Don't reset cImage
    gal.panIdx = 0;
    gal.panDir = RIGHT;
}

/**
 * Draw the current gal.frameData to the OLED, taking into account panning
 */
void ICACHE_FLASH_ATTR galDrawFrame(void)
{
    // Draw the frame to the OLED, one pixel at a time
    int wmod;
    for (int w = 0; w < OLED_WIDTH; w++)
    {
        for (int h = 0; h < OLED_HEIGHT; h++)
        {
            uint16_t linearIdx;
            if(galImages[gal.cImage]->continousPan == NONE)
            {

                linearIdx = (OLED_HEIGHT * (w + gal.panIdx)) + h;
            }
            else
            {
                //At first had this code which blows up if try and pan fast 10ms
                //linearIdx = (OLED_HEIGHT * ((w + gal.panIdx) % gal.width)) + h;
                wmod = w + gal.panIdx;
                while (wmod >= gal.width)
                {
                    wmod -= gal.width;
                }
                linearIdx = OLED_HEIGHT * wmod + h;
            }
            uint16_t byteIdx = linearIdx / 8;
            uint8_t bitIdx = linearIdx % 8;

            if (gal.frameData[byteIdx] & (0x80 >> bitIdx))
            {
                drawPixel(w, h, WHITE);
            }
            else
            {
                drawPixel(w, h, BLACK);
            }
        }
    }

    // Draw left and right arrows to indicate button functions
    fillDisplayArea(0, OLED_HEIGHT - FONT_HEIGHT_IBMVGA8 - 1, 7, OLED_HEIGHT, BLACK);
    plotText(0, OLED_HEIGHT - FONT_HEIGHT_IBMVGA8, "<", IBM_VGA_8, WHITE);
    fillDisplayArea(OLED_WIDTH - 7, OLED_HEIGHT - FONT_HEIGHT_IBMVGA8 - 1, OLED_WIDTH, OLED_HEIGHT, BLACK);
    plotText(OLED_WIDTH - 6, OLED_HEIGHT - FONT_HEIGHT_IBMVGA8, ">", IBM_VGA_8, WHITE);
}

/**
 * Timer function to pan the image left and right, if it is wider than the OLED
 *
 * @param arg Unused
 */
static void ICACHE_FLASH_ATTR galTimerPan(void* arg __attribute__((unused)))
{
    if(gal.virtualWidth > OLED_WIDTH)
    {
        switch(gal.panDir)
        {
            case RIGHT:
            {
                // If we're at the end
                if((gal.virtualWidth - OLED_WIDTH) == gal.panIdx)
                {
                    if (galImages[gal.cImage]->continousPan == ALWAYS_RIGHT)
                    {
                        // reset for pan
                        gal.panIdx = (gal.virtualWidth - OLED_WIDTH) - gal.width;
                    }
                    else
                    {
                        // Start going to the left
                        gal.panDir = LEFT;
                    }
                }
                else
                {
                    // Pan to the right
                    gal.panIdx++;
                }
                break;
            }
            case LEFT:
            {
                // If we're at the beginning
                if(0 == gal.panIdx)
                {
                    if (galImages[gal.cImage]->continousPan == ALWAYS_LEFT)
                    {
                        // reset for pan
                        gal.panIdx = gal.width;
                    }
                    else
                    {
                        // Start going to the right
                        gal.panDir = RIGHT;
                    }
                }
                else
                {
                    // Pan to the left (objects seem to move from L to R)
                    gal.panIdx--;
                }
                break;
            }
            default:
            {
                break;
            }
        }
        // Draw the panned frame
        galDrawFrame();
    }
}
