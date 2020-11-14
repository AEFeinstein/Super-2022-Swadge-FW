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
	if( x < 0 || x>= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT )
	{
		fprintf( stderr, "ERROR: PIXEL OUT OF RANGE in drawPixelUnsafe %d %d\n", x, y );
		return;
	}
    uint8_t* addy = &currentFb[(y + x * OLED_HEIGHT) / 8];
    uint8_t mask = 1 << (y & 7);
    *addy |= mask;
}

void drawPixelUnsafeBlack( int x, int y )
{
	if( x < 0 || x>= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT )
	{
		fprintf( stderr, "ERROR: PIXEL OUT OF RANGE in drawPixelUnsafe %d %d\n", x, y );
		return;
	}
    uint8_t* addy = &currentFb[(y + x * OLED_HEIGHT) / 8];
    uint8_t mask = ~(1 << (y & 7));
    *addy &= mask;
}

void drawPixelUnsafeC( int x, int y, color c )
{
	if( x < 0 || x>= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT )
	{
		fprintf( stderr, "ERROR: PIXEL OUT OF RANGE in drawPixelUnsafeC %d %d\n", x, y );
		return;
	}
	//Ugh, I know this looks weird, but it's faster than saying
	//addy = &currentFb[(y+x*OLED_HEIGHT)/8], and produces smaller code.
	//Found by looking at image.lst.
    uint8_t* addy = currentFb;
	addy = addy + (y + x * OLED_HEIGHT) / 8;

    uint8_t mask = 1 << (y & 7);
    if( c <= WHITE )    //TIL this 'if' tree is slightly faster than a switch.
        if( c == WHITE )
            *addy |= mask;
        else
            *addy &= ~mask;
    else
        if( c == INVERSE )
            *addy ^= mask;
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
        emuSendOLEDData( 1, currentFb );
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
