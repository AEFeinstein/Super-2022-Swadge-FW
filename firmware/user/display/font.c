/*
 * font.c
 *
 *  Created on: Mar 3, 2019
 *      Author: Adam Feinstein
 */

#include "osapi.h"
#include "oled.h"

#include "font.h"

uint8_t ICACHE_FLASH_ATTR plotChar(uint8_t x, uint8_t y,
                                   char character, sprite_t* table, uint8_t height)
{

    if ('a' <= character && character <= 'z')
    {
        character = (char) (character - 'a' + 'A');
    }
    const sprite_t* sprite = &table[character - ' '];

    uint8_t charX, charY;
    for (charX = 0; charX < sprite->width; charX++)
    {
        for (charY = 0; charY < height; charY++)
        {
            uint8_t xPx = (uint8_t) (x + (sprite->width - charX) - 1);
            uint8_t yPx = (uint8_t) (y + charY);
            if (0 != (sprite->data[charY] & (1 << charX)))
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

void ICACHE_FLASH_ATTR plotText(uint8_t x, uint8_t y, char* text, fonts font)
{
    while (0 != *text)
    {
        switch (font)
        {
            case TOM_THUMB:
            {
                x = plotChar(x, y, *text, font_TomThumb, FONT_HEIGHT_TOMTHUMB);
                break;
            }
            case IBM_VGA_8:
            {
                x = plotChar(x, y, *text, font_IbmVga8, FONT_HEIGHT_IBMVGA8);
                break;
            }
            case RADIOSTARS:
            {
                x = plotChar(x, y, *text, font_Radiostars, FONT_HEIGHT_RADIOSTARS);
                break;
            }
            default:
            {
                break;
            }
        }
        text++;
    }
}
