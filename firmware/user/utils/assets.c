#include <osapi.h>
#include "spi_memory_addrs.h"
#include "assets.h"
#include "oled.h"

// #define assetDbg(...) os_printf(__VA_ARGS__)
#define assetDbg(...)

/**
 * @brief Get a pointer to an asset
 *
 * @param name   The name of the asset to fetch
 * @param retLen A pointer to a uint32_t where the asset length will be written
 * @return A pointer to the asset, or NULL if not found
 */
uint32_t* getAsset(char* name, uint32_t* retLen)
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
void drawBitmapFromAsset(char* name, int16_t x, int16_t y, bool flipLR)
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
