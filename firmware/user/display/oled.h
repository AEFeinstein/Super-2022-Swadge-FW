/*
 * oled.h
 *
 *  Created on: Mar 16, 2019
 *      Author: adam
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
void ICACHE_FLASH_ATTR drawPixelFastWhite( int x, int y );
void ICACHE_FLASH_ATTR speedyWhiteLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1 );
color getPixel(int16_t x, int16_t y);
bool ICACHE_FLASH_ATTR setOLEDparams(bool turnOnOff);
int ICACHE_FLASH_ATTR updateOLEDScreenRange( uint8_t minX, uint8_t maxX, uint8_t minPage, uint8_t maxPage );
oledResult_t updateOLED(bool drawDifference);
void clearDisplay(void);
void fillDisplayArea(int16_t x1, int16_t y1, int16_t x2, int16_t y2, color c);

#endif

#endif /* OLED_H_ */
