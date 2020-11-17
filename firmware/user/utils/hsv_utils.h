//Copyright 2015 <>< Charles Lohr under the ColorChord License.

#ifndef _hsv_utils_H
#define _hsv_utils_H

#include "user_main.h"

uint32_t ICACHE_FLASH_ATTR EHSVtoHEXhelper( uint8_t hue, uint8_t sat, uint8_t val, bool applyGamma );
uint32_t EHSVtoHEX( uint8_t hue, uint8_t sat, uint8_t val ); //hue = 0..255 // TODO: TEST ME!!!
uint8_t ICACHE_FLASH_ATTR GAMMA_CORRECT(uint8_t val);

//Clamps values
led_t ICACHE_FLASH_ATTR SafeEHSVtoHEXhelper( int16_t hue, int16_t sat, int16_t val, bool applyGamma );

#endif

