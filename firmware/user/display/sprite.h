#ifndef _SPRITE_H_
#define _SPRITE_H_

#include "oled.h"
#include "user_config.h"

#if defined(FEATURE_OLED)

typedef struct
{
    uint8_t  width;
    uint8_t  height;
    uint16_t data[16];
    uint16_t dummy; // round size to multiple of 32 bits
} sprite_t;

int16_t ICACHE_FLASH_ATTR plotSprite(int16_t x, int16_t y, const sprite_t* sprite, color col);

#endif
#endif
