/*
 * font.h
 *
 *  Created on: Mar 3, 2019
 *      Author: Adam Feinstein
 */

#ifndef SRC_FONT_H_
#define SRC_FONT_H_

#include <osapi.h>
#include "sprite.h"
#include "oled.h"
#include "user_main.h"

typedef enum
{
    TOM_THUMB,
    IBM_VGA_8,
    RADIOSTARS
} fonts;

#define FONT_HEIGHT_RADIOSTARS 12
extern const sprite_t font_Radiostars[] RODATA_ATTR;

#define FONT_HEIGHT_IBMVGA8 10
extern const sprite_t font_IbmVga8[] RODATA_ATTR;

#define FONT_HEIGHT_TOMTHUMB 5
extern const sprite_t font_TomThumb[] RODATA_ATTR;

int16_t plotChar(int16_t x, int16_t y, char character, const sprite_t* table, color col);
int16_t plotText(int16_t x, int16_t y, char* text, fonts font, color col);

#endif /* SRC_FONT_H_ */
