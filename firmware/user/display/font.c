/*
 * font.c
 *
 *  Created on: Mar 3, 2019
 *      Author: Adam Feinstein
 */

#include <osapi.h>

#include "oled.h"
#include "sprite.h"
#include "font.h"

#if defined(FEATURE_OLED)

/**
 * @brief Draw a single character to the OLED display
 *        Special characters (< ' ') not drawn
 * @param x The x position where to draw the character
 * @param y The y position where to draw the character
 * @param character The character to print
 * @param table A table of character sprites, in ASCII order
 * @param col WHITE, BLACK or INVERSE
 * @return The x position of the end of the character drawn
 */
int16_t ICACHE_FLASH_ATTR plotChar(int16_t x, int16_t y,
                                   char character, const sprite_t* table, color col)
{
    if(character >= ' ')
    {
        if ('a' <= character && character <= 'z')
        {
            character = (char) (character - 'a' + 'A');
        }
        else if(character >= '{')
        {
            // These usually come after lowercase, but lowercase doesn't exist
            character = '`' + 1 + (character - '{');
        }
        return plotSprite(x, y, &table[character - ' '], col);
    }

    return x;
}

/**
 * @brief Draw a string to the display
 *        Special characters (< ' ') skipped
 * @param x The x position where to draw the string
 * @param y The y position where to draw the string
 * @param text The string to draw
 * @param font The font to draw the string in
 * @param col WHITE, BLACK or INVERSE
 * @return The x position of the end of the string drawn
 */
int16_t ICACHE_FLASH_ATTR plotText(int16_t x, int16_t y, const char* text, fonts font, color col)
{
    while (0 != *text)
    {
        switch (font)
        {
            case TOM_THUMB:
            {
                x = plotChar(x, y, *text, font_TomThumb, col);
                break;
            }
            case IBM_VGA_8:
            {
                x = plotChar(x, y, *text, font_IbmVga8, col);
                break;
            }
            case RADIOSTARS:
            {
                x = plotChar(x, y, *text, font_Radiostars, col);
                break;
            }
            default:
            {
                break;
            }
        }
        text++;
    }
    return x;
}

/**
 * @brief TODO
 *
 * @param character
 * @param table
 * @return int16_t
 */
int16_t ICACHE_FLASH_ATTR charWidth(char character, const sprite_t* table)
{
    if(character >= ' ')
    {
        if ('a' <= character && character <= 'z')
        {
            character = (char) (character - 'a' + 'A');
        }
        else if(character >= '{')
        {
            // These usually come after lowercase, but lowercase doesn't exist
            character = '`' + 1 + (character - '{');
        }
#ifdef USE_ESP_GDB // If we use GDB, read these to RAM first to avoid SIGSEV
        sprite_t sprite_ram;
        ets_memcpy ( &sprite_ram, &(table[character - ' ']), sizeof(sprite_t) );
        return sprite_ram.width + 1;
#else
        return table[character - ' '].width + 1;
#endif
    }
    return 0;
}

/**
 * @brief TODO
 *
 * @param text
 * @param font
 * @return int16_t
 */
int16_t ICACHE_FLASH_ATTR textWidth(const char* text, fonts font)
{
    int16_t width = 0;
    while (0 != *text)
    {
        switch (font)
        {
            case TOM_THUMB:
            {
                width += charWidth(*text, font_TomThumb);
                break;
            }
            case IBM_VGA_8:
            {
                width += charWidth(*text, font_IbmVga8);
                break;
            }
            case RADIOSTARS:
            {
                width += charWidth(*text, font_Radiostars);
                break;
            }
            default:
            {
                break;
            }
        }
        text++;
    }
    return width;
}

#endif