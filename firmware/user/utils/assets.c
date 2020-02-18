#include <osapi.h>
#include <mem.h>
#include "spi_memory_addrs.h"
#include "assets.h"
#include "oled.h"
#include "fastlz.h"

#define assetDbg(...) // os_printf(__VA_ARGS__)

void ICACHE_FLASH_ATTR gifTimerFn(void* arg);

/**
 * @brief Get a pointer to an asset
 *
 * @param name   The name of the asset to fetch
 * @param retLen A pointer to a uint32_t where the asset length will be written
 * @return A pointer to the asset, or NULL if not found
 */
uint32_t* ICACHE_FLASH_ATTR getAsset(const char* name, uint32_t* retLen)
{
    /* Note assets are placed immediately after irom0
     * See "irom0_0_seg" in "eagle.app.v6.ld" for where this value comes from
     * The makefile flashes ASSETS_FILE to 0x6C000
     */
    uint32_t* assets = (uint32_t*)(0x40210000 + 0x5C000);
    uint32_t idx = 0;
    uint32_t numIndexItems = assets[idx++];
    assetDbg("Scanning %d items\n", numIndexItems);

    for(uint32_t ni = 0; ni < numIndexItems; ni++)
    {
        // Read the name from the index
        char assetName[16] = {0};
        ets_memcpy(assetName, &assets[idx], sizeof(uint32_t) * 4);
        idx += 4;

        // Read the address from the index
        uint32_t assetAddress = assets[idx++];

        // Read the length from the index
        uint32_t assetLen = assets[idx++];

        assetDbg("%s, addr: %d, len: %d\n", assetName, assetAddress, assetLen);

        // Compare names
        if(0 == ets_strcmp(name, assetName))
        {
            assetDbg("Found asset\n");
            *retLen = assetLen;
            return &assets[assetAddress / sizeof(uint32_t)];
        }
    }
    *retLen = 0;
    return NULL;
}

/**
 * @brief Draw a bitmap asset to the OLED
 *
 * @param name The name of the asset to draw
 * @param x The x coordinate of the asset
 * @param y The y coordinate of the asset
 */
void ICACHE_FLASH_ATTR drawBitmapFromAsset(const char* name, int16_t x, int16_t y, bool flipLR)
{
    // Get the image from the packed assets
    uint32_t assetLen = 0;
    uint32_t* assetPtr = getAsset(name, &assetLen);

    if(NULL != assetPtr)
    {
        uint32_t idx = 0;

        // Get the width and height
        uint32_t width = assetPtr[idx++];
        uint32_t height = assetPtr[idx++];
        assetDbg("Width: %d, height: %d\n", width, height);

        // Read 32 bits at a time
        uint32_t chunk = assetPtr[idx++];
        uint32_t bitIdx = 0;
        // Draw the image's pixels
        for(uint16_t h = 0; h < height; h++)
        {
            for(uint16_t w = 0; w < width; w++)
            {
                int16_t xPos;
                if(flipLR)
                {
                    xPos = x + (width - w - 1);
                }
                else
                {
                    xPos = x + w;
                }

                bool isZero = true;
                if(chunk & (0x80000000 >> (bitIdx++)))
                {
                    assetDbg(" ");
                    drawPixel(xPos, y + h, BLACK);
                    isZero = false;
                }

                // After bitIdx was incremented, check it
                if(bitIdx == 32)
                {
                    chunk = assetPtr[idx++];
                    bitIdx = 0;
                }

                // A zero can be followed by a zero or a one
                if(isZero)
                {
                    if(chunk & (0x80000000 >> (bitIdx++)))
                    {
                        // Transparent
                        assetDbg(".");
                    }
                    else
                    {
                        // Transparent
                        assetDbg("X");
                        drawPixel(xPos, y + h, WHITE);
                    }

                    // After bitIdx was incremented, check it
                    if(bitIdx == 32)
                    {
                        chunk = assetPtr[idx++];
                        bitIdx = 0;
                    }
                }
            }
        }
        assetDbg("\n");
    }
    assetDbg("\n");
}

/**
 * @brief Start drawing a gif from an asset
 *
 * @param name The name of the asset to draw
 * @param x The x coordinate of the asset
 * @param y The y coordinate of the asset
 * @param handle A handle to store the gif state
 */
void ICACHE_FLASH_ATTR drawGifFromAsset(const char* name, int16_t x, int16_t y, gifHandle* handle)
{
    // Only do anything if the handle is uninitialized
    if(NULL == handle->compressed)
    {
        // Get the image from the packed assets
        uint32_t assetLen = 0;
        handle->assetPtr = getAsset(name, &assetLen);

        if(NULL != handle->assetPtr)
        {
            // Save where to draw the gif
            handle->x = x;
            handle->y = y;
            // Read metadata from memory
            handle->idx = 0;
            handle->width    = handle->assetPtr[handle->idx++];
            handle->height   = handle->assetPtr[handle->idx++];
            handle->nFrames  = handle->assetPtr[handle->idx++];
            handle->duration = handle->assetPtr[handle->idx++];

            assetDbg("%s\n  w: %d\n  h: %d\n  f: %d\n  d: %d\n", __func__,
                     handle->width,
                     handle->height,
                     handle->nFrames,
                     handle->duration);

            // Allocate enough space for the compressed data, decompressed data
            // and the actual gif
            handle->allocedSize = ((handle->width * handle->height) + 8) / 8;
            handle->compressed = (uint8_t*)os_malloc(handle->allocedSize);
            handle->decompressed = (uint8_t*)os_malloc(handle->allocedSize);
            handle->frame = (uint8_t*)os_malloc(handle->allocedSize);

            // Set up a timer to draw the other frames of the gif
            os_timer_setfn(&handle->timer, gifTimerFn, handle);
            os_timer_arm(&handle->timer, handle->duration, true);
        }
    }
}

/**
 * @brief Free all the memory associated with a gif
 *
 * @param handle A handle to store the gif state
 */
void ICACHE_FLASH_ATTR freeGifMemory(gifHandle* handle)
{
    os_timer_disarm(&handle->timer);
    os_free(handle->compressed);
    os_free(handle->decompressed);
    os_free(handle->frame);
    handle->compressed = NULL;
    handle->decompressed = NULL;
    handle->frame = NULL;
}

/**
 * @brief Timer function to draw gifs
 *
 * @param arg A handle containing the gif state
 */
void ICACHE_FLASH_ATTR gifTimerFn(void* arg)
{
    // Cast the pointer to something more useful
    gifHandle* handle = (gifHandle*)arg;

    // Read the compressed length of this frame
    uint32_t compressedLen = handle->assetPtr[handle->idx++];
    // Pad the length to a 32 bit boundary for memcpy
    uint32_t paddedLen = compressedLen;
    while(paddedLen % 4 != 0)
    {
        paddedLen++;
    }
    assetDbg("%s\n  frame: %d\n  cLen: %d\n  pLen: %d\n", __func__,
             handle->cFrame, compressedLen, paddedLen);

    // Copy the compressed data from flash to RAM
    os_memcpy(handle->compressed, &handle->assetPtr[handle->idx], paddedLen);
    handle->idx += (paddedLen / 4);

    // If this is the first frame
    if(handle->cFrame == 0)
    {
        // Decompress it straight to the frame data
        fastlz_decompress(handle->compressed, compressedLen,
                          handle->frame, handle->allocedSize);
    }
    else
    {
        // Otherwise decompress it to decompressed
        fastlz_decompress(handle->compressed, compressedLen,
                          handle->decompressed, handle->allocedSize);
        // Then apply the changes to the current frame
        uint16_t i;
        for(i = 0; i < handle->allocedSize; i++)
        {
            handle->frame[i] ^= handle->decompressed[i];
        }
    }

    // Increment the frame count, mod the number of frames
    handle->cFrame = (handle->cFrame + 1) % handle->nFrames;
    if(handle->cFrame == 0)
    {
        // Reset the index if we're starting again
        handle->idx = 4;
    }

    // Draw the current frame to the OLED
    int16_t h, w;
    for(h = 0; h < handle->height; h++)
    {
        for(w = 0; w < handle->width; w++)
        {
            int16_t byteIdx = (w + (h * handle->width)) / 8;
            int16_t bitIdx  = (w + (h * handle->width)) % 8;
            if(handle->frame[byteIdx] & (0x80 >> bitIdx))
            {
                drawPixel(w + handle->x, h + handle->y, WHITE);
            }
            else
            {
                drawPixel(w + handle->x, h + handle->y, BLACK);
            }
        }
    }
}
