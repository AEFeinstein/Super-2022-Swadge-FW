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

// Ref: http://marrin.org/2017/01/16/putting-data-in-esp8266-flash-memory/
#define RODATA_ATTR  __attribute__((section(".irom.text"))) __attribute__((aligned(4)))
#define ROMSTR_ATTR  __attribute__((section(".irom.text.romstr"))) __attribute__((aligned(4)))

typedef enum
{
    TOM_THUMB,
    IBM_VGA_8,
    RADIOSTARS
} fonts;

#define FONT_HEIGHT_RADIOSTARS 12
extern const sprite_t font_Radiostars[];

#define FONT_HEIGHT_IBMVGA8 10
extern const sprite_t font_IbmVga8[];

#define FONT_HEIGHT_TOMTHUMB 5
extern const sprite_t font_TomThumb[];

uint8_t plotChar(uint8_t x, uint8_t y, char character, sprite_t* table, color col);
uint8_t plotText(uint8_t x, uint8_t y, char* text, fonts font, color col);

#endif /* SRC_FONT_H_ */
