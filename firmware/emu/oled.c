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
    if ((0 <= x) && (x < OLED_WIDTH) &&
            (0 <= y) && (y < OLED_HEIGHT))
    {
        fbChanges = true;
        x = (OLED_WIDTH - 1) - x;
        y = (OLED_HEIGHT - 1) - y;
        if (y % 2 == 0)
        {
            y = (y >> 1);
        }
        else
        {
            y = (y >> 1) + (OLED_HEIGHT >> 1);
        }
        switch (c)
        {
            case WHITE:
                currentFb[(x + (y / 8) * OLED_WIDTH)] |= (1 << (y & 7));
                break;
            case BLACK:
                currentFb[(x + (y / 8) * OLED_WIDTH)] &= ~(1 << (y & 7));
                break;
            case INVERSE:
                currentFb[(x + (y / 8) * OLED_WIDTH)] ^= (1 << (y & 7));
                break;
            default:
            {
                break;
            }
        }
    }
}

color getPixel(int16_t x, int16_t y)
{
    if ((0 <= x) && (x < OLED_WIDTH) &&
            (0 <= y) && (y < OLED_HEIGHT))
    {
        x = (OLED_WIDTH - 1) - x;
        y = (OLED_HEIGHT - 1) - y;
        if (y % 2 == 0)
        {
            y = (y >> 1);
        }
        else
        {
            y = (y >> 1) + (OLED_HEIGHT >> 1);
        }

        if(currentFb[(x + (y / 8) * OLED_WIDTH)] & (1 << (y & 7)))
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


