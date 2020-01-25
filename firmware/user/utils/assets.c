#include <osapi.h>
#include "spi_memory_addrs.h"
#include "assets.h"
#include "oled.h"

// #define assetDbg(...) os_printf(__VA_ARGS__)
#define assetDbg(...)

#define reverseByteOrder(a) ( ((a >> 24) & 0x000000FF) | \
                              ((a >> 8)  & 0x0000FF00) | \
                              ((a << 8)  & 0x00FF0000) | \
                              ((a << 24) & 0xFF000000) )

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
    numIndexItems = reverseByteOrder(numIndexItems);
    assetDbg("Scanning %d items\n", numIndexItems);

    for(uint32_t ni = 0; ni < numIndexItems; ni++)
    {
        // Read the name from the index
        char assetName[16] = {0};
        ets_memcpy(assetName, &assets[idx], sizeof(uint32_t) * 4);
        idx += 4;

        // Read the address from the index
        uint32_t assetAddress = assets[idx++];
        assetAddress = reverseByteOrder(assetAddress);

        // Read the length from the index
        uint32_t assetLen = assets[idx++];
        assetLen = reverseByteOrder(assetLen);

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
void drawBitmapFromAsset(char* name, uint16_t x, uint16_t y)
{
    // Get the image from the packed assets
    uint32_t assetLen = 0;
    uint32_t* assetPtr = getAsset(name, &assetLen);

    if(NULL != assetPtr)
    {
        uint32_t idx = 0;

        // Get the width and height
        uint32_t width = assetPtr[idx++];
        width = reverseByteOrder(width);
        uint32_t height = assetPtr[idx++];
        height = reverseByteOrder(height);
        assetDbg("Width: %d, height: %d\n", width, height);

        // Read 32 bits at a time
        uint32_t chunk = assetPtr[idx++];
        chunk = reverseByteOrder(chunk);
        uint32_t bitIdx = 0;
        // Draw the image's pixels
        for(uint32_t h = 0; h < height; h++)
        {
            for(uint32_t w = 0; w < width; w++)
            {
                if(chunk & (0x80000000 >> (bitIdx++)))
                {
                    assetDbg("X");
                    drawPixel(x + w, y + h, WHITE);
                }
                else
                {
                    assetDbg(" ");
                    drawPixel(x + w, y + h, BLACK);
                }

                if(bitIdx == 32)
                {
                    chunk = assetPtr[idx++];
                    chunk = reverseByteOrder(chunk);
                    bitIdx = 0;
                }
            }
            assetDbg("\n");
        }
        assetDbg("\n");
    }
}
