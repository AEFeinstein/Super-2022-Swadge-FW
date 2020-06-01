#ifndef _SWADGEEMU_H
#define _SWADGEEMU_H

#include <stdlib.h>
#include "c_types.h"
#include "display/oled.h"

//Configuration
#define INIT_PX_SCALE 4
#define FOOTER_PIXELS 40
#define NR_WS2812 8

extern int px_scale;
extern uint32_t * rawvidmem;
extern short screenx, screeny;
extern uint32_t footerpix[FOOTER_PIXELS*OLED_WIDTH];
extern uint32_t ws2812s[NR_WS2812];
extern double boottime;
extern uint8_t gpio_status;




//which_display = 0 -> mainscreen, = 1 -> footer
void emuCheckFooterMouse( int x, int y, int finger, int bDown );
void emuSendOLEDData( int which_display, uint8_t * currentFb );
void emuFooter();
void emuCheckResize();


#endif
 
