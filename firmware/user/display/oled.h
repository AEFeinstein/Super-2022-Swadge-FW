/*
 * oled.h
 *
 *  Created on: Mar 16, 2019
 *      Author: adam, CNLohr
 */

#ifndef OLED_H_
#define OLED_H_

#include <c_types.h>
#include "user_config.h"

#if defined(FEATURE_OLED)

typedef enum
{
    BLACK = 0,
    WHITE = 1,
    INVERSE = 2,
    TRANSPARENT_COLOR = 3
} color;

typedef enum
{
    NOTHING_TO_DO,
    FRAME_DRAWN,
    FRAME_NOT_DRAWN
} oledResult_t;

#define OLED_WIDTH 128
#define OLED_HEIGHT 64

bool initOLED(bool reset);
void drawPixel(int16_t x, int16_t y, color c);
void drawPixelUnsafe( int x, int y );
void drawPixelUnsafeC( int x, int y, color c );

void ICACHE_FLASH_ATTR outlineTriangle( int16_t v0x, int16_t v0y, int16_t v1x, int16_t v1y,
                                        int16_t v2x, int16_t v2y, color colorA, color colorB );

void ICACHE_FLASH_ATTR speedyWhiteLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, bool thicc );
color getPixel(int16_t x, int16_t y);
bool ICACHE_FLASH_ATTR setOLEDparams(bool turnOnOff);
int ICACHE_FLASH_ATTR updateOLEDScreenRange( uint8_t minX, uint8_t maxX, uint8_t minPage, uint8_t maxPage );
oledResult_t updateOLED(bool drawDifference);
void clearDisplay(void);
void fillDisplayArea(int16_t x1, int16_t y1, int16_t x2, int16_t y2, color c);

#endif

#endif /* OLED_H_ */
