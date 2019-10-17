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

#define MAX_DECOMPRESSED_SIZE 0xA00
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
void ICACHE_FLASH_ATTR galLoadFirstFrame(void);
void ICACHE_FLASH_ATTR galClearImage(void);
void ICACHE_FLASH_ATTR galDrawFrame(void);
static void ICACHE_FLASH_ATTR galLoadNextFrame(void* arg);
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
    uint8_t* compressedData;
    uint8_t* decompressedData;
    uint8_t* frameData;
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
    .compressedData = NULL,
    .decompressedData = NULL,
    .frameData = NULL,
    .width = 0,
    .height = 0,
    .nFrames = 0,
    .cFrame = 0,
    .timerAnimate = {0},
    .timerPan = {0},
    .cImage = 0,
};

typedef struct
{
    uint8_t* data;
    uint16_t len;
} galFrame_t;

typedef struct
{
    uint8_t nFrames;
    galFrame_t frames[];
} galImage_t;

galImage_t galBongo =
{
    .nFrames = 2,
    .frames = {
        {.data = gal_bongo_0, .len = sizeof(gal_bongo_0)},
        {.data = gal_bongo_1, .len = sizeof(gal_bongo_1)},
    }
};

galImage_t galSnort =
{
    .nFrames = 2,
    .frames = {
        {.data = gal_snort_0, .len = sizeof(gal_snort_0)},
    }
};

galImage_t galGaylord =
{
    .nFrames = 3,
    .frames = {
        {.data = gal_gaylord_0, .len = sizeof(gal_gaylord_0)},
        {.data = gal_gaylord_1, .len = sizeof(gal_gaylord_1)},
        {.data = gal_gaylord_2, .len = sizeof(gal_gaylord_2)},
    }
};

galImage_t galFunkus =
{
    .nFrames = 30,
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

galImage_t galLogo =
{
    .nFrames = 21,
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

galImage_t* galImages[5] =
{
    &galBongo,
    &galFunkus,
    &galLogo,
    &galSnort,
    &galGaylord
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
    gal.compressedData = os_malloc(MAX_DECOMPRESSED_SIZE);
    gal.decompressedData = os_malloc(MAX_DECOMPRESSED_SIZE);
    gal.frameData = os_malloc(MAX_DECOMPRESSED_SIZE);

    //Set up software timers for animation and panning
    os_timer_disarm(&gal.timerAnimate);
    os_timer_setfn(&gal.timerAnimate, (os_timer_func_t*)galLoadNextFrame, NULL);
    os_timer_disarm(&gal.timerPan);
    os_timer_setfn(&gal.timerPan, (os_timer_func_t*)galTimerPan, NULL);

    // Load the image
    galLoadFirstFrame();
}

/**
 * Called when gallery is exited
 */
void ICACHE_FLASH_ATTR galExitMode(void)
{
    os_free(gal.compressedData);
    os_free(gal.decompressedData);
    os_free(gal.frameData);
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
                galLoadFirstFrame();
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
                galLoadFirstFrame();
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
void ICACHE_FLASH_ATTR galLoadFirstFrame(void)
{
    // Decompress the image
    memcpy(gal.compressedData, galImages[gal.cImage]->frames[0].data,
           galImages[gal.cImage]->frames[0].len);
    uint32_t dLen = fastlz_decompress(gal.compressedData,
                                      galImages[gal.cImage]->frames[0].len,
                                      gal.decompressedData,
                                      MAX_DECOMPRESSED_SIZE);
    DBG_GAL("dLen=%d\n", dLen);

    // Save the metadata
    gal.width = (gal.decompressedData[0] << 8) | gal.decompressedData[1];
    gal.height = (gal.decompressedData[2] << 8) | gal.decompressedData[3];
    gal.nFrames = (gal.decompressedData[4] << 8) | gal.decompressedData[5];
    gal.duration = (gal.decompressedData[6] << 8) | gal.decompressedData[7];
    DBG_GAL("w=%d, h=%d, nfr=%d, dur=%d\n", gal.width, gal.height, gal.nFrames,
            gal.duration);

    // Save the first actual frame
    memset(gal.frameData, 0, MAX_DECOMPRESSED_SIZE);
    memcpy(gal.frameData, &gal.decompressedData[METADATA_LEN], dLen - METADATA_LEN);

    // Set the current frame to 0
    gal.cFrame = 0;

    // Adjust the animation timer to this image's speed
    os_timer_disarm(&gal.timerAnimate);
    if(gal.nFrames > 1)
    {
        os_timer_arm(&gal.timerAnimate, gal.duration, 1);
    }

    // Draw the first frame in it's entirety to the OLED
    galDrawFrame();
}

/**
 * @brief TODO
 *
 */
static void ICACHE_FLASH_ATTR galLoadNextFrame(void* arg __attribute__((unused)))
{
    // Set the current frame to 0
    gal.cFrame = (gal.cFrame + 1) % gal.nFrames;

    if(0 == gal.cFrame)
    {
        // Draw the whole frame
        galLoadFirstFrame();
    }
    else
    {
        // Decompress the image
        memcpy(gal.compressedData, galImages[gal.cImage]->frames[gal.cFrame].data,
               galImages[gal.cImage]->frames[gal.cFrame].len);
        uint32_t dLen = fastlz_decompress(gal.compressedData,
                                          galImages[gal.cImage]->frames[gal.cFrame].len,
                                          gal.decompressedData,
                                          MAX_DECOMPRESSED_SIZE);
        DBG_GAL("dLen=%d\n", dLen);

        // Adjust only the differences
        for(uint32_t idx = 0; idx < dLen; idx++)
        {
            gal.frameData[idx] ^= gal.decompressedData[idx];
        }
        galDrawFrame();
    }
}

/**
 * @brief TODO
 *
 */
void ICACHE_FLASH_ATTR galClearImage(void)
{
    memset(gal.decompressedData, 0, MAX_DECOMPRESSED_SIZE);
    memset(gal.compressedData, 0, MAX_DECOMPRESSED_SIZE);
    memset(gal.frameData, 0, MAX_DECOMPRESSED_SIZE);
    gal.width = 0;
    gal.height = 0;
    gal.nFrames = 0;
    gal.cFrame = 0;
}

/**
 * @brief TODO
 *
 */
void ICACHE_FLASH_ATTR galDrawFrame(void)
{
    // Draw the frame to the OLED
    for (int w = 0; w < OLED_WIDTH; w++)
    {
        for (int h = 0; h < OLED_HEIGHT; h++)
        {
            uint16_t linearIdx = (OLED_HEIGHT * w) + h;
            uint16_t byteIdx = linearIdx / 8;
            uint8_t bitIdx = linearIdx % 8;

            if (gal.frameData[METADATA_LEN + byteIdx] & (0x80 >> bitIdx))
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
 * @param arg
 */
static void ICACHE_FLASH_ATTR galTimerPan(void* arg __attribute__((unused)))
{
}