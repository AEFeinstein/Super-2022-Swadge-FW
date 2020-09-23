#include <display/oled.h>
#include "swadgemu.h"

#define SSD1306_NUM_PAGES 8
#define SSD1306_NUM_COLS 128

#define OLEDMEM ((OLED_WIDTH * (OLED_HEIGHT / 8)))
uint8_t currentFb[OLEDMEM] = {0};
uint8_t priorFb[OLEDMEM] = {0};
uint8_t mBarLen = 0;
bool fbChanges = false;
bool fbOnline = false;


bool initOLED(bool reset)
{
    int i;
    for( i = 0; i < OLEDMEM; i++ )
    {
        priorFb[i] = currentFb[i] = rand() & 0xff;
    }
    fbChanges = 1;
    updateOLED(0);
    return true;
}

void drawPixel(int16_t x, int16_t y, color c)
{
    if (c != TRANSPARENT_COLOR &&
            (0 <= x) && (x < OLED_WIDTH) &&
            (0 <= y) && (y < OLED_HEIGHT))
    {
        fbChanges = true;
        uint8_t * addy = &currentFb[(y + x * OLED_HEIGHT)/8];
        uint8_t mask = 1<<(y&7);
        switch (c)
        {
            case WHITE:
                *addy |= mask;
                break;
            case BLACK:
                *addy &= ~mask;
                break;
            case INVERSE:
                *addy ^= mask;
                break;
            case TRANSPARENT_COLOR:
            default:
            {
                break;
            }
        }
    }
}


void drawPixelUnsafe( int x, int y )
{
    uint8_t* addy = &currentFb[(y + x * OLED_HEIGHT) / 8];
    uint8_t mask = 1 << (y & 7);
    *addy |= mask;
}


/**
 * @brief Optimized method to quickly draw a white line.
 *
 * @param x1, x0 Column of display, 0 is at the left
 * @param y1, y0 Row of the display, 0 is at the top
 *
 */
//This is fast, but broken :(
void ICACHE_FLASH_ATTR speedyWhiteLine( int16_t x0, int16_t y0, int16_t x1, int16_t y1 )
{
//Tune this as a function of the size of your viewing window, line accuracy, and worst-case scenario incoming lines.
#define BRESEN_W OLED_WIDTH
#define BRESEN_H OLED_HEIGHT
#define FIXEDPOINT 16
#define FIXEDPOINTD2 15
    int dx = (x1-x0);
    int dy = (y1-y0);
    int sdx = (dx>0)?1:-1;
    int sdy = (dy>0)?1:-1;
    int yerrdiv = ( dx * sdx );  //dy, but always positive.
    int xerrdiv = ( dy * sdy );  //dx, but always positive.
    int yerrnumerator = 0;
    int xerrnumerator = 0;
	int cx = x0;
	int cy = y0;

    if( cx < 0 && x1 < 0 ) return;
    if( cy < 0 && y1 < 0 ) return;
    if( cx >= BRESEN_W && x1 >= BRESEN_W ) return;
    if( cy >= BRESEN_H && y1 >= BRESEN_H ) return;

    //We put the checks above to check this, in case we have a situation where
    // we have a 0-length line outside of the viewable area.  If that happened,
    // we would have aborted before hitting this code.

    if( yerrdiv > 0 )
    {
        int dxA = 0;
        if( cx < 0 )
        {
            dxA = 0 - cx;
            cx = 0;
        }
        if( cx > BRESEN_W-1 )
        {
            dxA = (cx - (BRESEN_W-1));
            cx = BRESEN_W-1;
        }
        if( dxA || xerrdiv <= yerrdiv )
        {
            yerrnumerator = (((dy * sdy)<<16) + yerrdiv/2) / yerrdiv;
            if( dxA )
            {
                cy += (((yerrnumerator * dxA)) * sdy) >> FIXEDPOINT; //This "feels" right
                //Weird situation - if we cal, and now, both ends are out on the same side abort.
                if( cy < 0 && y1 < 0 ) return;
                if( cy > BRESEN_H-1 && y1 > BRESEN_H-1 ) return;
            }
        }
    }

    if( xerrdiv > 0 )
    {
        int dyA = 0;    
        if( cy < 0 )
        {
            dyA = 0 - cy;
            cy = 0;
        }
        if( cy > BRESEN_H-1 )
        {
            dyA = (cy - (BRESEN_H-1));
            cy = BRESEN_H-1;
        }
        if( dyA || xerrdiv > yerrdiv )
        {
            xerrnumerator = (((dx * sdx)<<16) + xerrdiv/2 ) / xerrdiv;
            if( dyA )
            {
                cx += (((xerrnumerator*dyA)) * sdx) >> FIXEDPOINT; //This "feels" right.
                //If we've come to discover the line is actually out of bounds, abort.
                if( cx < 0 && x1 < 0 ) return;
                if( cx > BRESEN_W-1 && x1 > BRESEN_W-1 ) return;
            }
        }
    }

    if( x1 == cx && y1 == cy )
    {
        drawPixelUnsafe( cx, cy );
        return;
    }

    //Make sure we haven't clamped the wrong way.
    //Also this checks for vertical/horizontal violations.
    if( dx > 0 )
    {
        if( cx > BRESEN_W-1 ) return;
        if( cx > x1 ) return;
    }
    else if( dx < 0 )
    {
        if( cx < 0 ) return;
        if( cx < x1 ) return;
    }

    if( dy > 0 )
    {
        if( cy > BRESEN_H-1 ) return;
        if( cy > y1 ) return;
    }
    else if( dy < 0 )
    {
        if( cy < 0 ) return;
        if( cy < y1 ) return;
    }

    //Force clip end coordinate.
    //NOTE: We have an extra check within the inner loop, to avoid complicated math here.
    //Theoretically, we could math this so that in the end-coordinate clip stage
    //to make sure this condition just could never be hit, however, that is very
    //difficult to guarantee under all situations and may have weird edge cases.
    //So, I've decided to stick this here.

    if( xerrdiv > yerrdiv )
    {
        int xerr = 1<<FIXEDPOINTD2;
        if( x1 < 0 ) x1 = 0;
        if( x1 > BRESEN_W-1) x1 = BRESEN_W-1;
        x1 += sdx; //Tricky - make sure the "next" mark we hit doesn't overflow.

        if( y1 < 0 ) y1 = 0;
        if( y1 > BRESEN_H-1 ) y1 = BRESEN_H-1;

        for( ; cy != y1; cy+=sdy )
        {
            drawPixelUnsafe( cx, cy );
            xerr += xerrnumerator;
            while( xerr >= (1<<FIXEDPOINT) )
            {
                cx += sdx;
                if( cx == x1 ) return;
                drawPixelUnsafe( cx, cy );
                xerr -= 1<<FIXEDPOINT;
            }
        }
        drawPixelUnsafe( cx, cy );
    }
    else
    {
        int yerr = 1<<FIXEDPOINTD2;

        if( y1 < 0 ) y1 = 0;
        if( y1 > BRESEN_H-1 ) y1 = BRESEN_H-1;
        y1 += sdy;        //Tricky: Make sure the NEXT mark we hit doens't overflow.

        if( x1 < 0 ) x1 = 0;
        if( x1 > BRESEN_W-1) x1 = BRESEN_W-1;

        for( ; cx != x1; cx+=sdx )
        {
            drawPixelUnsafe( cx, cy );
            yerr += yerrnumerator;
            while( yerr >= 1<<FIXEDPOINT )
            {
                cy += sdy;
                if( cy == y1 ) return;
                drawPixelUnsafe( cx, cy );
                yerr -= 1<<FIXEDPOINT;
            }
        }
        drawPixelUnsafe( cx, cy );
    }
}


color getPixel(int16_t x, int16_t y)
{
    if ((0 <= x) && (x < OLED_WIDTH) &&
            (0 <= y) && (y < OLED_HEIGHT))
    {
        if(currentFb[(y + x * OLED_HEIGHT)/8] & (1 << (y & 7)))
        {
            return WHITE;
        }
        else
        {
            return BLACK;
        }
    }
    return BLACK;
}

bool ICACHE_FLASH_ATTR setOLEDparams(bool turnOnOff)
{
    fbOnline = turnOnOff;
    return true;
}

oledResult_t updateOLED(bool drawDifference)
{
    if( fbChanges )
    {
        emuSendOLEDData( 0, currentFb );
        //For the emulator, we don't care about differences.
        ets_memcpy(priorFb, currentFb, sizeof(currentFb));
        return FRAME_DRAWN;
    }
    return FRAME_NOT_DRAWN;
}

void clearDisplay(void)
{
    ets_memset(currentFb, 0, sizeof(currentFb));
    fbChanges = true;
}

void fillDisplayArea(int16_t x1, int16_t y1, int16_t x2, int16_t y2, color c)
{
    int16_t x, y;
    for (x = x1; x <= x2; x++)
    {
        for (y = y1; y <= y2; y++)
        {
            drawPixel(x, y, c);
        }
    }
}

void zeroMenuBar(void)
{
    fbChanges = true;
    mBarLen = 0;
}

uint8_t incrementMenuBar(void)
{
    fbChanges = true;
    return ++mBarLen;
}


