/*
 * font.h
 *
 *  Created on: Mar 3, 2019
 *      Author: Adam Feinstein
 */

#ifndef SRC_FONT_H_
#define SRC_FONT_H_

#include <stdint.h>

typedef enum {
	TOM_THUMB,
	IBM_VGA_8,
	RADIOSTARS
} fonts;

typedef struct {
    const uint8_t  width;
    const uint16_t data[15];
} sprite_t;

#define FONT_HEIGHT_RADIOSTARS 12
extern sprite_t font_Radiostars[];

#define FONT_HEIGHT_IBMVGA8 10
extern sprite_t font_IbmVga8[];

#define FONT_HEIGHT_TOMTHUMB 5
extern sprite_t font_TomThumb[];

uint8_t plotChar(uint8_t x, uint8_t y, char character, sprite_t * table, uint8_t height);
void plotText(uint8_t x, uint8_t y, char * text, fonts font);

#endif /* SRC_FONT_H_ */
