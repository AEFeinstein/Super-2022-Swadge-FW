#ifndef _TEXTENTRY_H
#define _TEXTENTRY_H

#include "user_main.h"
#include "buttons.h"


void ICACHE_FLASH_ATTR textEntryStart( int max_len, char* buffer );
void ICACHE_FLASH_ATTR textEntryEnd( void );

bool ICACHE_FLASH_ATTR textEntryDraw(void);
bool ICACHE_FLASH_ATTR textEntryInput( uint8_t down, button_num button );

#endif

