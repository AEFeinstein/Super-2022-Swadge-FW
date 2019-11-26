/*
*   mode_tiltrads.h
*
*   Created on: Aug 2, 2019
*       Author: Jonathan Moriarty
*/

#ifndef _MODE_TILTRADS_H
#define _MODE_TILTRADS_H

#include "font.h"       //draw text

extern swadgeMode tiltradsMode;

int16_t ICACHE_FLASH_ATTR plotCenteredText(int16_t x0, int16_t y, int16_t x1, char* text, fonts font, color col);

#endif
