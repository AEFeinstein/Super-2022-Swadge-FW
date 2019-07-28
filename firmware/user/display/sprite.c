#include <osapi.h>
#include "oled.h"
#include "sprite.h"

/**
 * @brief Draw a sprite to the display
 *
 * @param x The x position where to draw the sprite
 * @param y The y position where to draw the sprite
 * @param sprite The sprite to draw
 * @return The x position of the end of the sprite drawn
 */
uint8_t ICACHE_FLASH_ATTR plotSprite(uint8_t x, uint8_t y, const sprite_t* sprite)
{
    uint8_t xIdx, yIdx;
    for (xIdx = 0; xIdx < sprite->width; xIdx++)
    {
        for (yIdx = 0; yIdx < sprite->height; yIdx++)
        {
            uint8_t xPx = (uint8_t) (x + (sprite->width - xIdx) - 1);
            uint8_t yPx = (uint8_t) (y + yIdx);
            if (0 != (sprite->data[yIdx] & (1 << xIdx)))
            {
                drawPixel(xPx, yPx, WHITE);
            }
            else
            {
                drawPixel(xPx, yPx, BLACK);
            }
        }
    }
    return (uint8_t) (x + sprite->width + 1);
}