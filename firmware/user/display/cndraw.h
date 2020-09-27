/*
 * cndraw.h
 *
 *  Created on: Sep 26, 2020
 *      Author: adam, CNLohr
 */

#ifndef CNDRAW_H_
#define CNDRAW_H_

void fillDisplayArea(int16_t x1, int16_t y1, int16_t x2, int16_t y2, color c);
void ICACHE_FLASH_ATTR outlineTriangle( int16_t v0x, int16_t v0y, int16_t v1x, int16_t v1y,
                                        int16_t v2x, int16_t v2y, color colorA, color colorB );

void ICACHE_FLASH_ATTR speedyWhiteLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, bool thicc );

#endif