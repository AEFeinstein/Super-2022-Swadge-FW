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

/*==============================================================================
 * Defines
 *============================================================================*/

#define MAX_DECOMPRESSED_SIZE 0x8440
#define METADATA_LEN 8

#define DBG_GAL(...) do { \
        os_printf("%s::%d ", __func__, __LINE__); \
        os_printf(__VA_ARGS__); \
    } while(0)

/*==============================================================================
 * Prototypes
 *============================================================================*/

void ICACHE_FLASH_ATTR galEnterMode(void);
void ICACHE_FLASH_ATTR galExitMode(void);
void ICACHE_FLASH_ATTR galButtonCallback(uint8_t state, int button, int down);
void ICACHE_FLASH_ATTR galLoadFirstImage(void);
void ICACHE_FLASH_ATTR galClearImage(void);
void ICACHE_FLASH_ATTR galDrawFirstFrame(void);
static void ICACHE_FLASH_ATTR galDrawNextFrame(void* arg);
static void ICACHE_FLASH_ATTR galTimerPan(void* arg);

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
    .fnAccelerometerCallback = NULL
};

struct
{
    uint8_t* imageData;
    uint16_t width;
    uint16_t height;
    uint16_t nFrames;
    uint16_t cFrame;
    uint16_t duration;
    os_timer_t timerAnimate;
    os_timer_t timerPan;
    uint16_t cImage;
} gal =
{
    .imageData = NULL,
    .width = 0,
    .height = 0,
    .nFrames = 0,
    .cFrame = 0,
    .timerAnimate = {0},
    .timerPan = {0},
    .cImage = 0,
};

const struct
{
    uint8_t* data;
    uint16_t len;
} galImages[] =
{
    {gal_bongo, sizeof(gal_bongo)},
    {gal_funkus, sizeof(gal_funkus)},
    {gal_gaylord, sizeof(gal_gaylord)},
    {gal_logo, sizeof(gal_logo)},
    {gal_snort, sizeof(gal_snort)},
};

/*==============================================================================
 * Functions
 *============================================================================*/

/**
 * Initializer for gallery
 */
void ICACHE_FLASH_ATTR galEnterMode(void)
{
    // Clear everything out, for safety
    memset(&gal, 0, sizeof(gal));

    // Allocate a bunch of memory for decompresed images
    gal.imageData = os_malloc(MAX_DECOMPRESSED_SIZE);

    //Set up software timers for animation and panning
    os_timer_disarm(&gal.timerAnimate);
    os_timer_setfn(&gal.timerAnimate, (os_timer_func_t*)galDrawNextFrame, NULL);
    os_timer_disarm(&gal.timerPan);
    os_timer_setfn(&gal.timerPan, (os_timer_func_t*)galTimerPan, NULL);

    // Load the image
    galLoadFirstImage();
}

/**
 * Called when gallery is exited
 */
void ICACHE_FLASH_ATTR galExitMode(void)
{
    os_free(gal.imageData);
}

/**
 * TODO
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
            case 1:
                galClearImage();
                gal.cImage = (gal.cImage + 1) %
                             (sizeof(galImages) / sizeof(galImages[0]));
                galLoadFirstImage();
                break;
            case 2:
                galClearImage();
                if(0 == gal.cImage)
                {
                    gal.cImage = (sizeof(galImages) / sizeof(galImages[0])) - 1;
                }
                else
                {
                    gal.cImage--;
                }
                galLoadFirstImage();
                break;
            default:
                break;
        }
    }
}

/**
 * @brief TODO
 *
 */
void ICACHE_FLASH_ATTR galLoadFirstImage(void)
{
    // Decompress the image
    uint32_t dLen = fastlz_decompress(galImages[gal.cImage].data,
                                      galImages[gal.cImage].len,
                                      gal.imageData,
                                      MAX_DECOMPRESSED_SIZE);
    DBG_GAL("dLen=%d\n", dLen);

    // Save the metadata
    gal.width = (gal.imageData[0] << 8) | gal.imageData[1];
    gal.height = (gal.imageData[2] << 8) | gal.imageData[3];
    gal.nFrames = (gal.imageData[4] << 8) | gal.imageData[5];
    gal.duration = (gal.imageData[6] << 8) | gal.imageData[7];
    DBG_GAL("w=%d, h=%d, nfr=%d, dur=%d\n", gal.width, gal.height, gal.nFrames,
            gal.duration);

    // Set the current frame to 0
    gal.cFrame = 0;

    // Adjust the animation timer to this image's speed
    os_timer_disarm(&gal.timerAnimate);
    os_timer_arm(&gal.timerAnimate, gal.duration, 1);

    // Draw the first frame in it's entirety to the OLED
    galDrawFirstFrame();
}

/**
 * @brief TODO
 *
 */
void ICACHE_FLASH_ATTR galClearImage(void)
{
    memset(gal.imageData, 0, MAX_DECOMPRESSED_SIZE);
    gal.width = 0;
    gal.height = 0;
    gal.nFrames = 0;
    gal.cFrame = 0;
}

/**
 * @brief TODO
 *
 */
void ICACHE_FLASH_ATTR galDrawFirstFrame(void)
{
    // Draw the frame to the OLED
    for (int w = 0; w < OLED_WIDTH; w++)
    {
        for (int h = 0; h < OLED_HEIGHT; h++)
        {
            uint16_t linearIdx = (OLED_HEIGHT * w) + h;
            uint16_t byteIdx = linearIdx / 8;
            uint8_t bitIdx = linearIdx % 8;

            if (gal.imageData[METADATA_LEN + byteIdx] & (0x80 >> bitIdx))
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
 * @brief TODO
 *
 */
void ICACHE_FLASH_ATTR galDrawNextFrame(void* arg __attribute__((unused)))
{
    // Set the current frame to 0
    gal.cFrame = (gal.cFrame + 1) % gal.nFrames;

    if(0 == gal.cFrame)
    {
        // Draw the whole frame
        galDrawFirstFrame();
    }
    else
    {
        // Draw only the differences
        for (int w = 0; w < OLED_WIDTH; w++)
        {
            for (int h = 0; h < OLED_HEIGHT; h++)
            {
                uint32_t linearIdx = ((OLED_HEIGHT * w) + h) +
                                     (gal.width * gal.height * gal.cFrame);
                uint16_t byteIdx = linearIdx / 8;
                uint8_t bitIdx = linearIdx % 8;

                if (gal.imageData[METADATA_LEN + byteIdx] & (0x80 >> bitIdx))
                {
                    drawPixel(w, h, INVERSE);
                }
            }
        }
    }
}

/**
 * @brief TODO
 *
 * @param arg
 */
static void ICACHE_FLASH_ATTR galTimerPan(void* arg __attribute__((unused)))
{
}