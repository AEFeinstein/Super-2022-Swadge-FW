/*
 * oled.h
 *
 *  Created on: Mar 16, 2019
 *      Author: adam
 */

#ifndef OLED_H_
#define OLED_H_

#include <c_types.h>

typedef enum
{
    BLACK = 0,
    WHITE = 1,
    INVERSE = 2
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
color getPixel(int16_t x, int16_t y);
oledResult_t updateOLED(bool drawDifference);
void clearDisplay(void);
void fillDisplayArea(int16_t x1, int16_t y1, int16_t x2, int16_t y2, color c);
void zeroMenuBar(void);
uint8_t incrementMenuBar(void);

#endif /* OLED_H_ */
