#ifndef _SWADGEEMU_H
#define _SWADGEEMU_H

#include <stdlib.h>
#include "c_types.h"
#include "display/oled.h"

//Configuration
#define INIT_PX_SCALE 4
#define FOOTER_PIXELS 40

//which_display = 0 -> mainscreen, = 1 -> footer
void emuSendOLEDData( int which_display, uint8_t * currentFb );

void  * ets_memcpy( void * dest, const void * src, size_t n );
void  * ets_memset( void * s, int c, size_t n );


#endif
 
