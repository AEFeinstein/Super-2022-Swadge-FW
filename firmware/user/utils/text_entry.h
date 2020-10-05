#ifndef _TEXTENTRY_H
#define _TEXTENTRY_H

#include "user_main.h"


void ICACHE_FLASH_ATTR textEntryStart( int max_len, char * buffer );

//Returns false if text entry complete.
bool ICACHE_FLASH_ATTR textEntryDraw();
bool ICACHE_FLASH_ATTR textEntryInput( uint8_t down, uint8_t button );

#endif

