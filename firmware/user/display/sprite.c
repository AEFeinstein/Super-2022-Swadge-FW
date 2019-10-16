#include <osapi.h>
#include "oled.h"
#include "sprite.h"

/**
 * @brief Draw a sprite to the display
 *
 * @param x The x position where to draw the sprite
 * @param y The y position where to draw the sprite
 * @param sprite The sprite to draw
 * @param col WHITE, BLACK or INVERSE
 * @return The x position of the end of the sprite drawn
 */
uint8_t ICACHE_FLASH_ATTR plotSprite(uint8_t x, uint8_t y, const sprite_t* p_sprite, color col)
{
    uint8_t xIdx, yIdx;
    color foreground, background;

    switch (col)
    {
        default:
        case WHITE:
        {
            foreground = WHITE;
            background = BLACK;
            break;
        }
        case BLACK:
        {
            foreground = BLACK;
            background = WHITE;
            break;
        }
        case INVERSE:
        {
            foreground = INVERSE;
            background = INVERSE;
            break;
        }
    }

    // refactor this code if it works!!!
    // sprite_t sprite_ram = p_sprite[0]; // Used to copy 32 bits of flash contents to RAM where 8 bit accesses are allowed
    sprite_t sprite_ram;
    memcpy ( &sprite_ram, p_sprite, sizeof(sprite_t) );
    for (xIdx = 0; xIdx < sprite_ram.width; xIdx++)
    {
        for (yIdx = 0; yIdx < sprite_ram.height; yIdx++)
        {
            uint8_t xPx = (uint8_t) (x + (sprite_ram.width - xIdx) - 1);
            uint8_t yPx = (uint8_t) (y + yIdx);
            if (0 != (sprite_ram.data[yIdx] & (1 << xIdx)))
            {
                drawPixel(xPx, yPx, foreground);
            }
            else
            {
                drawPixel(xPx, yPx, background);
            }
        }
    }
    return (uint8_t) (x + sprite_ram.width + 1);
}
