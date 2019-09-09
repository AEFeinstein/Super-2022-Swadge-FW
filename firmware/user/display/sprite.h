#ifndef _SPRITE_H_
#define _SPRITE_H_

#include "oled.h"

typedef struct
{
    const uint8_t  width;
    const uint8_t  height;
    const uint16_t data[16];
} sprite_t;

uint8_t ICACHE_FLASH_ATTR plotSprite(uint8_t x, uint8_t y, const sprite_t* sprite, color col);

#endif
