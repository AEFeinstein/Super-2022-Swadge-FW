/*
 * oled.c
 *
 *  Created on: Mar 16, 2019
 *      Author: adam, CNLohr
 */

//==============================================================================
// Includes
//==============================================================================

#include <osapi.h>

#include "oled.h"
#include "cnlohr_i2c.h"
#include "gpio_user.h"
#include "user_main.h"

#if defined(FEATURE_OLED)

//==============================================================================
// Defines and Enums
//==============================================================================

#define OLED_ADDRESS (0x78 >> 1)
//#define OLED_FREQ 800
#define OLED_HIGH_SPEED 1

#define SSD1306_NUM_PAGES 8
#define SSD1306_NUM_COLS OLED_WIDTH

typedef enum
{
    HORIZONTAL_ADDRESSING = 0x00,
    VERTICAL_ADDRESSING = 0x01,
    PAGE_ADDRESSING = 0x02
} memoryAddressingMode;

typedef enum
{
    Vcc_X_0_65 = 0x00,
    Vcc_X_0_77 = 0x20,
    Vcc_X_0_83 = 0x30,
} VcomhDeselectLevel;

typedef enum
{
    SSD1306_CMD = 0x00,
    SSD1306_DATA = 0x40
} SSD1306_prefix;

typedef enum
{
    /**
     * Set Memory Addressing Mode
     *
     * @param mode The mode: horizontal, vertical or page
     *  1 PARAMETER: Mode i.e. VERTICAL_ADDRESSING, HORIZONTAL_ADDRESSING, PAGE_ADDRESSING
     */
    SSD1306_MEMORYMODE = 0x20,

    /**
     * Setup column start and end address
     *
     * This command is only for horizontal or vertical addressing mode.
     *
     * @param startAddr Column start address, range : 0-127d, (RESET=0d)
     * @param endAddr   Column end address, range : 0-127d, (RESET =127d)
     *
     * 2 PARAMETERS: startAddr and endAddr
     */
    SSD1306_COLUMNADDR = 0x21,

    /**
     * Setup page start and end address
     *
     * This command is only for horizontal or vertical addressing mode.
     *
     * @param startAddr Page start Address, range : 0-7d, (RESET = 0d)
     * @param endAddr   Page end Address, range : 0-7d, (RESET = 7d)
     *
     * 2 PARAMETERS: startAddr and endAddr
     */
    SSD1306_PAGEADDR = 0x22,

    /**
     * Double byte command to select 1 out of 256 contrast steps. Contrast increases
     * as the value increases.
     *
     * (RESET = 7Fh)
     *
     * @param contrast the value to set as contrast. Adafruit uses 0 for dim, 0x9F
     *                 for external VCC contrast and 0xCF for 3.3V voltage
     *
     * 1 PARAMETER: param
     */
    SSD1306_SETCONTRAST = 0x81,

    /**
     * Charge Pump Setting
     *
     * The Charge Pump must be enabled by the following command:
     * 8Dh ; Charge Pump Setting
     * 14h ; Enable Charge Pump
     * AFh; Display ON
     *
     * @param enable true  - Enable charge pump during display on
     *               false - Disable charge pump(RESET)
     *
     * PARAMETER 1:  0x10 | (enable ? 0x04 : 0x00)
     */
    SSD1306_CHARGEPUMP = 0x8D,

    /**
     * Set Segment Re-map
     *
     * @param colAddr true  - column address 127 is mapped to SEG0
     *                false - column address 0 is mapped to SEG0 (RESET)
     *
     * 0 PARAMETERS: SSD1306_SEGREMAP | (colAddr ? 0x01 : 0x00)
     */
    SSD1306_SEGREMAP = 0xA0,

    /**
     * Turn the entire display on
     *
     * @param ignoreRam:    true  - Entire display ON, Output ignores RAM content
     *                     false - Resume to RAM content display (RESET) Output follows RAM content
     *
     * 0 PARAMETERS: ignoreRAM ? SSD1306_DISPLAYALLON : SSD1306_DISPLAYALLON_RESUME
     */
    SSD1306_DISPLAYALLON_RESUME = 0xA4,
    SSD1306_DISPLAYALLON = 0xA5,


    /**
     * Set whether the display is color inverted or not
     *
     * @param inverse true  - Normal display (RESET)
     *                        0 in RAM: OFF in display panel
     *                        1 in RAM: ON in display panel
     *                false - Inverse display (RESET)
     *                        0 in RAM: ON in display panel
     *                        1 in RAM: OFF in display panel
     * 0 PARAMETERS: inverse ? SSD1306_INVERTDISPLAY : SSD1306_NORMALDISPLAY
     */
    SSD1306_NORMALDISPLAY = 0xA6,
    SSD1306_INVERTDISPLAY = 0xA7,

    /**
     * Set Multiplex Ratio
     *
     * @param ratio  from 16MUX to 64MUX, RESET= 111111b (i.e. 63d, 64MUX)
     *
     * 1 PARAMETER: ratio
     */
    SSD1306_SETMULTIPLEX = 0xA8,

    /**
     * Set the display on or off
     *
     * @param on true  - Display ON in normal mode
     *           false - Display OFF (sleep mode)
     *
     * 0 PARAMETERS: on ? SSD1306_DISPLAYON : SSD1306_DISPLAYOFF
     */
    SSD1306_DISPLAYOFF = 0xAE,
    SSD1306_DISPLAYON = 0xAF,

    /**
     * @brief When in PAGE_ADDRESSING, address the page to write to
     *
     * @param page The page to write to, 0 to 7
     *
     * 0 PARAMETERS: Usage:  SSD1306_PAGEADDRPAGING | pageno
     */
    SSD1306_PAGEADDRPAGING = 0xB0,

    /**
     * Set COM Output Scan Direction
     *
     * @param increment SSD1306_COMSCANINC  -  normal mode (RESET) Scan from COM0 to COM[N –1]
     *                  SSD1306_COMSCANDEC  -  remapped mode. Scan from COM[N-1] to COM0
     *
     * 0 PARAMETERS (no parameters)
     */
    SSD1306_COMSCANINC = 0xC0,
    SSD1306_COMSCANDEC = 0xC8,

    /**
     * Set vertical shift by COM from 0d~63d. The value is reset to 00h after RESET.
     *
     * @param offset The offset, 0d~63d
     *
     * 1 PARAMETER: offset
     */
    SSD1306_SETDISPLAYOFFSET = 0xD3,

    /**
     * Set Display Clock Divide Ratio/Oscillator Frequency
     *
     * @param LSB Nibble  Define the divide ratio (D) of the display clocks (DCLK)
     *                     The actual ratio is 1 + this param, RESET is 0000b (divide ratio = 1)
     *                     Range:0000b~1111b
     * @param msbNibble   Set the Oscillator Frequency. Oscillator Frequency increases
     *                with the value and vice versa. RESET is 1000b
     *                Range:0000b~1111b
     *
     * 1 PARAMETER: (divideRatio & 0x0F) | ((oscFreq << 4) & 0xF0)
     */
    SSD1306_SETDISPLAYCLOCKDIV = 0xD5,

    /**
     * Set Pre-charge Period
     *
     * @param Nibble-LSB   Phase 1 period of up to 15 DCLK clocks 0 is invalid entry (RESET=2h)
     *                     Range:0000b~1111b
     * @param Nibble-MSB   Phase 2 period of up to 15 DCLK clocks 0 is invalid entry (RESET=2h )
     *                     Range:0000b~1111b
     *
     *  1 PARAMETER:         (phase1period & 0x0F) | ((phase2period << 4) & 0xF0)
     */
    SSD1306_SETPRECHARGE = 0xD9,

    /**
     * One parameter:
     * @param sequential 0x10  - Sequential COM pin configuration
     *                   0x00  - (RESET), Alternative COM pin configuration
     * @param remap 0x20  - Enable COM Left/Right remap
     *              0x00  - (RESET), Disable COM Left/Right remap
     *
     * 1 PARAMETER: (sequential ? 0x00 : 0x10) | (remap ? 0x20 : 0x00) | 0x02
     */
    SSD1306_SETCOMPINS = 0xDA,

    /**
     * Set VCOMH Deselect Level
     *
     * @param level ~0.65 x VCC, ~0.77 x VCC (RESET), or ~0.83 x VCC
     *
     * 1 PARAMETER: level
     *
     */
    SSD1306_SETVCOMDETECT = 0xDB,

    /**
     * @brief When in PAGE_ADDRESSING, address the column's lower nibble
     *
     * @param col The lower nibble of the column to write to, 0 to 15
     *
     * 0 PARAMETERS: SSD1306_SETLOWCOLUMN | param
     */
    SSD1306_SETLOWCOLUMN = 0x00,

    SSD1306_SETHIGHCOLUMN = 0x10,

    /**
     * Set display RAM display start line register from 0-63
     * Display start line register is reset to 000000b during RESET.
     *
     * @param startLineRegister start line register, 0-63
     *
     * 0 PARAMETERS: SSD1306_SETSTARTLINE | (startLineRegister & 0x3F)
     */
    SSD1306_SETSTARTLINE = 0x40,

    SSD1306_EXTERNALVCC = 0x01,
    SSD1306_SWITCHCAPVCC = 0x02,

    SSD1306_RIGHT_HORIZONTAL_SCROLL = 0x26,
    SSD1306_LEFT_HORIZONTAL_SCROLL = 0x27,
    SSD1306_VERTICAL_AND_RIGHT_HORIZONTAL_SCROLL = 0x29,
    SSD1306_VERTICAL_AND_LEFT_HORIZONTAL_SCROLL = 0x2A,

    /**
     *
     * @param on true  - Start scrolling that is configured by the scrolling setup
     *                   commands :26h/27h/29h/2Ah with the following valid sequences:
     *                     Valid command sequence 1: 26h ;2Fh.
     *                     Valid command sequence 2: 27h ;2Fh.
     *                     Valid command sequence 3: 29h ;2Fh.
     *                     Valid command sequence 4: 2Ah ;2Fh.
     *           false - Stop scrolling that is configured by command 26h/27h/29h/2Ah.
     *
     *   0 PARAMETERS: on ? SSD1306_ACTIVATE_SCROLL : SSD1306_DEACTIVATE_SCROLL
     */
    SSD1306_DEACTIVATE_SCROLL = 0x2E,
    SSD1306_ACTIVATE_SCROLL = 0x2F,

    /**
     *
     * @param A - Set No. of rows in top fixed area. The No. of rows in top fixed area
     *            is referenced to the top of the GDDRAM (i.e. row 0).[RESET = 0]
     *
     * @param B - Set No. of rows in scroll area. This is the number of rows to be used
     *            for vertical scrolling. The scroll area starts in the first row below
     *            the top fixed area. [RESET = 64]
     *
     *  2 PARAMETERS: a and b.
     */
    SSD1306_SET_VERTICAL_SCROLL_AREA = 0xA3,

    /**
     *
     * @ param A - Set vertical shift by COM from 0d~63d The value is reset to 00h after RESET
     *
     *  1 PARAMETER: a
     */
    SSD1306_SET_DISPLAY_OFFSET = 0xD3,

    //Special commands added for our interpreter
    PCD_CMD0 = 0xF0,
    PCD_CMD1 = 0xF1,
    PCD_CMD2 = 0xF2,
    PCD_CMD3 = 0xF3,
    PCD_COND0 = 0xF4,
    PCD_COND1 = 0xF5,
    PCD_COND2 = 0xF6,
    PCD_COND3 = 0xF7,
    PCD_END  = 0xFF,
} SSD1306_cmd;


//==============================================================================
// Script of commands which get executed at system start
//==============================================================================

static const uint8_t displayInitStartCommands[] RODATA_ATTR =
{
    PCD_COND0, SSD1306_DISPLAYOFF, //Conditionally execute!
    PCD_CMD1, SSD1306_SETMULTIPLEX, OLED_HEIGHT - 1,
    PCD_CMD1, SSD1306_SETDISPLAYOFFSET, 0,
    PCD_CMD0, SSD1306_SETSTARTLINE | 0,
    PCD_CMD1, SSD1306_MEMORYMODE, VERTICAL_ADDRESSING,
#if (SWADGE_VERSION == BARREL_1_0_0)
    PCD_CMD0, SSD1306_SEGREMAP | true,
    PCD_CMD0, SSD1306_COMSCANDEC,
#else
    PCD_CMD0, SSD1306_SEGREMAP | false,
    PCD_CMD0, SSD1306_COMSCANINC,
#endif
    PCD_CMD1, SSD1306_SETCOMPINS, 0x12,
    PCD_CMD1, SSD1306_SETCONTRAST, 0x7f,
    PCD_CMD1, SSD1306_SETPRECHARGE, 0xf1,
    PCD_CMD1, SSD1306_SETVCOMDETECT, Vcc_X_0_77,
    PCD_COND0, SSD1306_DISPLAYALLON_RESUME, //Conditionally execute.
    PCD_CMD0, SSD1306_NORMALDISPLAY,
    PCD_CMD1, SSD1306_SETDISPLAYCLOCKDIV, 0x80,
    PCD_CMD1, SSD1306_CHARGEPUMP, 0x14,
    PCD_CMD0, SSD1306_DEACTIVATE_SCROLL,
    PCD_CMD2, SSD1306_SET_VERTICAL_SCROLL_AREA, 0, 64,
    PCD_CMD1, SSD1306_SET_DISPLAY_OFFSET, 0,
    PCD_COND0, SSD1306_DISPLAYON, //Conditionally execute.
    PCD_END
};

//==============================================================================
// Internal Function Declarations
//==============================================================================

#define PCD_FLAGS_EXECUTE_ALL 0x01
#define PCD_FLAGS_EXECUTE_CONDITION 0x02
#define PCD_FAIL_DEVICE -1
#define PCD_FAIL_COMMANDS -2
int ICACHE_FLASH_ATTR processDisplayCommands( const uint8_t * buffer, uint8_t flags );

//==============================================================================
// Variables
//==============================================================================

uint8_t currentFb[(OLED_WIDTH * (OLED_HEIGHT / 8))] = {0};
uint8_t priorFb[(OLED_WIDTH * (OLED_HEIGHT / 8))] = {0};


bool fbChanges = false;

//==============================================================================
// Functions
//==============================================================================

/**
 * Clear the display.
 */
void ICACHE_FLASH_ATTR clearDisplay(void)
{
    ets_memset(currentFb, 0, (OLED_WIDTH * (OLED_HEIGHT / 8)) );
    fbChanges = true;
}

/**
 * Fill a rectangular display area with a single color
 *
 * @param x1 The X pixel to start at
 * @param y1 The Y pixel to start at
 * @param x2 The X pixel to end at
 * @param y2 The Y pixel to end at
 * @param c  The color to fill
 */
void ICACHE_FLASH_ATTR fillDisplayArea(int16_t x1, int16_t y1, int16_t x2, int16_t y2, color c)
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

/**
 * Set/clear/invert a single pixel.
 *
 * This intentionally does not have ICACHE_FLASH_ATTR because it may be called often
 *
 * @param x Column of display, 0 is at the left
 * @param y Row of the display, 0 is at the top
 * @param c Pixel color, one of: BLACK, WHITE or INVERT
 */
void drawPixel(int16_t x, int16_t y, color c)
{
    if (c != TRANSPARENT_COLOR &&
            (0 <= x) && (x < OLED_WIDTH) &&
            (0 <= y) && (y < OLED_HEIGHT))
    {
        fbChanges = true;
        uint8_t* addy = &currentFb[(y + x * OLED_HEIGHT) / 8];
        uint8_t mask = 1 << (y & 7);
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


/**
 * Set a single pixel unsafely but quickly.
 *
 * This intentionally does not have ICACHE_FLASH_ATTR because it may be called often
 *
 * @param x Column of display, 0 is at the left
 * @param y Row of the display, 0 is at the top
 */
void drawPixelUnsafe( int x, int y )
{
    uint8_t* addy = &currentFb[(y + x * OLED_HEIGHT) / 8];
    uint8_t mask = 1 << (y & 7);
    *addy |= mask;
}

/**
 * Draw a single pixel unsafely but quickly.
 *
 * This intentionally does not have ICACHE_FLASH_ATTR because it may be called often
 *
 * @param x Column of display, 0 is at the left
 * @param y Row of the display, 0 is at the top
 * @param c Color of pixel to draw.
 */
void drawPixelUnsafeC( int x, int y, color c )
{
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

/**
 * @brief Optimized method to quickly draw a white line.
 *
 * @param x1, x0 Column of display, 0 is at the left
 * @param y1, y0 Row of the display, 0 is at the top
 *
 */
void ICACHE_FLASH_ATTR speedyWhiteLine( int16_t x0, int16_t y0, int16_t x1, int16_t y1, bool thicc )
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
            yerrnumerator = (((dy * sdy)<<FIXEDPOINT) + yerrdiv/2) / yerrdiv;
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
            xerrnumerator = (((dx * sdx)<<FIXEDPOINT) + xerrdiv/2 ) / xerrdiv;
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
                if( thicc ) drawPixelUnsafe( cx, cy );
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
                if( thicc ) drawPixelUnsafe( cx, cy );
                yerr -= 1<<FIXEDPOINT;
            }
        }
        drawPixelUnsafe( cx, cy );
    }
}

/**
 * @brief Optimized method to draw a triangle with outline.
 *
 * @param x2, x1, x0 Column of display, 0 is at the left
 * @param y2, y1, y0 Row of the display, 0 is at the top
 * @param colorA filled area color
 * @param colorB outline color
 *
 */
void ICACHE_FLASH_ATTR outlineTriangle( int16_t v0x, int16_t v0y, int16_t v1x, int16_t v1y,
    int16_t v2x, int16_t v2y, color colorA, color colorB )
{
    int16_t i16tmp;

    //Sort triangle such that v0 is the top-most vertex.
    //v0->v1 is LEFT edge.
    //v0->v2 is RIGHT edge.

    if( v0y > v1y )
    {
        i16tmp = v0x; v0x = v1x; v1x = i16tmp;
        i16tmp = v0y; v0y = v1y; v1y = i16tmp;
    }
    if( v0y > v2y )
    {
        i16tmp = v0x; v0x = v2x; v2x = i16tmp;
        i16tmp = v0y; v0y = v2y; v2y = i16tmp;
    }

    //v0 is now top-most vertex.  Now orient 2 and 3.
    //Tricky: Use slopes!  Otherwise, we could get it wrong.
    {
        int slope02;
        if( v2y - v0y )
            slope02 = ((v2x - v0x)<<FIXEDPOINT)/(v2y - v0y);
        else
            slope02 = ((v2x - v0x)>0)?0x7fffff:-0x800000;

        int slope01;
        if( v1y - v0y )
            slope01 = ((v1x - v0x)<<FIXEDPOINT)/(v1y - v0y);
        else
            slope01 = ((v1x - v0x)>0)?0x7fffff:-0x800000;

        if( slope02 < slope01 )
        {
            i16tmp = v1x; v1x = v2x; v2x = i16tmp;
            i16tmp = v1y; v1y = v2y; v2y = i16tmp;
        }
    }

    //We now have a fully oriented triangle.
    int16_t x0A = v0x;
    int16_t y0A = v0y;
    int16_t x0B = v0x;
    //int16_t y0B = v0y;

    //A is to the LEFT of B.
    int dxA = (v1x-v0x);
    int dyA = (v1y-v0y);
    int dxB = (v2x-v0x);
    int dyB = (v2y-v0y);
    int sdxA = (dxA>0)?1:-1;
    int sdyA = (dyA>0)?1:-1;
    int sdxB = (dxB>0)?1:-1;
    int sdyB = (dyB>0)?1:-1;
    int xerrdivA = ( dyA * sdyA );  //dx, but always positive.
    int xerrdivB = ( dyB * sdyB );  //dx, but always positive.
    int xerrnumeratorA = 0;
    int xerrnumeratorB = 0;

    if( xerrdivA )
        xerrnumeratorA = (((dxA * sdxA)<<FIXEDPOINT) + xerrdivA/2 ) / xerrdivA;
    else
        xerrnumeratorA = 0x7fffff;

    if( xerrdivB )
        xerrnumeratorB = (((dxB * sdxB)<<FIXEDPOINT) + xerrdivB/2 ) / xerrdivB;
    else
        xerrnumeratorB = 0x7fffff;

    //X-clipping is handled on a per-scanline basis.
    //Y-clipping must be handled upfront.

/*
    //Optimization BUT! Can't do this here, as we would need to be smarter about it.
    //If we do this, and the second triangle is above y=0, we'll get the wrong answer.
    if( y0A < 0 )
    {
        delta = 0 - y0A;
        y0A = 0;
        y0B = 0;
        x0A += (((xerrnumeratorA*delta)) * sdxA) >> FIXEDPOINT; //Could try rounding.
        x0B += (((xerrnumeratorB*delta)) * sdxB) >> FIXEDPOINT;
    }
*/

    {
        //Section 1 only.
        int yend = (v1y < v2y)?v1y:v2y;
        int errA = 1<<FIXEDPOINTD2;
        int errB = 1<<FIXEDPOINTD2;
        int y;

        //Going between x0A and x0B
        for( y = y0A; y < yend; y++ )
        {
            int x = x0A;
            int endx = x0B;
            int suppress = 1;

            if( y >= 0 && y <= (BRESEN_H-1) )
            {
                suppress = 0;
                if( x < 0 ) x = 0;
                if( endx > (BRESEN_W) ) endx = (BRESEN_W);
                if( x0A >= 0  && x0A <= (BRESEN_W-1) )
                {
                    drawPixelUnsafeC( x0A, y, colorB );
                    x++;
                }
                for( ; x < endx; x++ )
                    drawPixelUnsafeC( x, y, colorA );
                if( x0B <= (BRESEN_W-1) && x0B >= 0 )
                    drawPixelUnsafeC( x0B, y, colorB );
            }

            //Now, advance the start/end X's.
            errA += xerrnumeratorA;
            errB += xerrnumeratorB;
            while( errA >= (1<<FIXEDPOINT) && x0A != v1x )
            {
                x0A += sdxA;
                //if( x0A < 0 || x0A > (BRESEN_W-1) ) break;
                if( x0A >= 0 && x0A <= (BRESEN_W-1) && !suppress )
                    drawPixelUnsafeC( x0A, y, colorB );
                errA -= 1<<FIXEDPOINT;
            }
            while( errB >= (1<<FIXEDPOINT) && x0B != v2x )
            {
                x0B += sdxB;
                //if( x0B < 0 || x0B > (BRESEN_W-1) ) break;
                if( x0B >= 0 && x0B <= (BRESEN_W-1) && !suppress )
                    drawPixelUnsafeC( x0B, y, colorB );
                errB -= 1<<FIXEDPOINT;
            }
        }

        //We've come to the end of section 1.  Now, we need to figure

        //Now, yend is the highest possible hit on the triangle.
        yend = (v1y < v2y)?v2y:v1y;

        //v1 is LEFT OF v2
        // A is LEFT OF B
        if( v1y < v2y )
        {
            //V1 has terminated, move to V1->V2 but keep V0->V2[B] segment
            yend = v2y;
            dxA = (v2x-v1x);
            dyA = (v2y-v1y);
            sdxA = (dxA>0)?1:-1;
            sdyA = (dyA>0)?1:-1;
            xerrdivA = ( dyA * sdyA );  //dx, but always positive.
            if( xerrdivA )
                xerrnumeratorA = (((dxA * sdxA)<<FIXEDPOINT) + xerrdivA/2 ) / xerrdivA;
            else
                xerrnumeratorA = 0x7fffff;
            x0A = v1x;
            errA = 1<<FIXEDPOINTD2;
        }
        else
        {
            //V2 has terminated, move to V2->V1 but keep V0->V1[A] segment
            yend = v1y;
            dxB = (v1x-v2x);
            dyB = (v1y-v2y);
            sdxB = (dxB>0)?1:-1;
            sdyB = (dyB>0)?1:-1;
            xerrdivB = ( dyB * sdyB );  //dx, but always positive.
            if( xerrdivB )
                xerrnumeratorB = (((dxB * sdxB)<<FIXEDPOINT) + xerrdivB/2 ) / xerrdivB;
            else
                xerrnumeratorB = 0x7fffff;
            x0B = v2x;
            errB = 1<<FIXEDPOINTD2;
        }

        if( yend > (BRESEN_H-1) ) yend = BRESEN_H-1;

        for( ; y <= yend; y++ )
        {
            int x = x0A;
            int endx = x0B;
            int suppress = 1;

            if( y >= 0 && y <= (BRESEN_H-1) )
            {
                suppress = 0;
                if( x < 0 ) x = 0;
                if( endx >= (BRESEN_W) ) endx = (BRESEN_W);
                if( x0A >= 0  && x0A <= (BRESEN_W-1) )
                {
                    drawPixelUnsafeC( x0A, y, colorB );
                    x++;
                }
                for( ; x < endx; x++ )
                    drawPixelUnsafeC( x, y, colorA );
                if( x0B <= (BRESEN_W-1) && x0B >= 0 )
                    drawPixelUnsafeC( x0B, y, colorB );
            }

            //Now, advance the start/end X's.
            errA += xerrnumeratorA;
            errB += xerrnumeratorB;
            while( errA >= (1<<FIXEDPOINT) )
            {
                x0A += sdxA;
                //if( x0A < 0 || x0A > (BRESEN_W-1) ) break;
                if( x0A >= 0 && x0A <= (BRESEN_W-1) && !suppress )
                    drawPixelUnsafeC( x0A, y, colorB );
                errA -= 1<<FIXEDPOINT;
                if( x0A == x0B+1 ) return;
            }
            while( errB >= (1<<FIXEDPOINT) )
            {
                x0B += sdxB;
                if( x0B >= 0 && x0B <= (BRESEN_W-1) && !suppress )
                    drawPixelUnsafeC( x0B, y, colorB );
                errB -= 1<<FIXEDPOINT;
                if( x0A == x0B+1 ) return;
            }
        }
    }
}


/**
 * @brief Get a pixel at the current location
 *
 * @param x Column of display, 0 is at the left
 * @param y Row of the display, 0 is at the top
 * @return either BLACK or WHITE
 */
color ICACHE_FLASH_ATTR getPixel(int16_t x, int16_t y)
{
    if ((0 <= x) && (x < OLED_WIDTH) &&
            (0 <= y) && (y < OLED_HEIGHT))
    {
        if(currentFb[(y + x * OLED_HEIGHT) / 8] & (1 << (y & 7)))
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

/**
 * Initialize the SSD1206 OLED
 *
 * @param reset true to reset the OLED using the RST line, false to leave it alone
 * @return true if it initialized, false if it failed
 */
bool ICACHE_FLASH_ATTR initOLED(bool reset)
{
    // Clear the RAM
    clearDisplay();

    // Reset SSD1306 if requested and reset pin specified in constructor
    if (reset)
    {
        setOledResetOn(true);  // VDD goes high at start
        ets_delay_us(1000);    // pause for 1 ms
        setOledResetOn(false); // Bring reset low
        ets_delay_us(10000);   // Wait 10 ms
        setOledResetOn(true);  // Bring out of reset
    }

    // Set the OLED's parameters
    if(false == setOLEDparams(true))
    {
        return false;
    }

    // Also clear the display's RAM on boot
    return updateOLED(false);
}

/**
 * Set all the parameters on the OLED for normal operation
 * This takes ~1.3ms to execute on boot, and slightly less if restarting.
 */
bool ICACHE_FLASH_ATTR setOLEDparams(bool turnOnOff)
{
    int ret = processDisplayCommands( displayInitStartCommands,
        PCD_FLAGS_EXECUTE_ALL |
        ( turnOnOff ? PCD_FLAGS_EXECUTE_CONDITION : 0 )
    );

    if( ret < 0 )
        return false;
    else
        return true;
}

int ICACHE_FLASH_ATTR updateOLEDScreenRange( uint8_t minX, uint8_t maxX, uint8_t minPage, uint8_t maxPage )
{
    uint8_t encountered_error = false;
    uint8_t x, page;

    {
        uint8_t displayRangeUpdate[] = 
        {
            PCD_CMD2, SSD1306_COLUMNADDR, minX, maxX,
            PCD_CMD2, SSD1306_PAGEADDR, minPage, maxPage,
            PCD_END
        };
        processDisplayCommands( displayRangeUpdate, PCD_FLAGS_EXECUTE_ALL );
    }

    SendStart(OLED_HIGH_SPEED);
    SendByte( OLED_ADDRESS << 1, OLED_HIGH_SPEED );
    SendByte( SSD1306_DATA, OLED_HIGH_SPEED );
    for( x = minX; x <= maxX; x++ )
    {
        int index = x * SSD1306_NUM_PAGES + minPage;
        uint8_t* prior = &priorFb[index];
        uint8_t* cur = &currentFb[index];

        for( page = minPage; page <= maxPage; page++ )
        {
            SendByteFast( *(prior++) = *(cur++) );
        }
    }
    SendStop(OLED_HIGH_SPEED);
    return encountered_error ? FRAME_NOT_DRAWN : FRAME_DRAWN;
}

/**
 * Push data currently in RAM to SSD1306 display.
 *
 * @param drawDifference true to only draw differences from the prior frame
 *                       false to draw the entire frame
 * @return true  the data was sent
 *         false there was some I2C error
 */
oledResult_t ICACHE_FLASH_ATTR updateOLED(bool drawDifference)
{
    //Before sending the actual data, we do housekeeping. This can take between 57 and 200 uS
    //But ensures the visual data stays consistent.
    {
        //Slowly refresh the display settings... Executing the script.
        static const uint8_t * displayInitPlace;
        static uint8_t rangeUpColumn;
        if( rangeUpColumn == OLED_WIDTH )
        {
            //Slowly refresh commands.

            if( !displayInitPlace )
            {
                displayInitPlace = displayInitStartCommands;
            }

            int r = processDisplayCommands( displayInitPlace, 0 );
            if( r <= 0 )
            {
                displayInitPlace = displayInitStartCommands;
                rangeUpColumn = 0; //Switch back to just updating data.
            }
            else
            {
                displayInitPlace += r;
            }
        }
        else
        {
            //Also, slowly refresh data.
            updateOLEDScreenRange( rangeUpColumn, rangeUpColumn, 0, 7 );
            rangeUpColumn++;
            //Note: if rangeUpColumn == OLED_WIDTH will switch to refreshing commands.
        }
    }

    if(true == drawDifference && false == fbChanges)
    {
        // We know nothing happened, just return
        return NOTHING_TO_DO;
    }
    else
    {
        // Clear this bool and draw to the OLED
        fbChanges = false;
    }

    uint8_t minX = OLED_WIDTH;
    uint8_t maxX = 0;
    uint8_t minPage = SSD1306_NUM_PAGES;
    uint8_t maxPage = 0;

    if( drawDifference )
    {
        //Right now, we just look for the rect on the screen which encompasses the biggest changed area.
        //We could, however, update multiple rectangles if we wanted, more similar to the previous system.

        uint8_t x, page;
        uint8_t* pPrev = priorFb;
        uint8_t* pCur = currentFb;
        for( x = 0; x < OLED_WIDTH; x++ )
        {
            for( page = 0; page < SSD1306_NUM_PAGES; page++ )
            {
                if( *pPrev != *pCur )
                {
                    if( x < minX )
                    {
                        minX = x;
                    }
                    if( x > maxX )
                    {
                        maxX = x;
                    }
                    if( page < minPage )
                    {
                        minPage = page;
                    }
                    if( page > maxPage )
                    {
                        maxPage = page;
                    }
                }
                pPrev++;
                pCur++;
            }
        }

        if( maxX >= minX && maxPage >= minPage )
        {
            return updateOLEDScreenRange( minX, maxX, minPage, maxPage );
        }
        else
        {
            return NOTHING_TO_DO;
        }
    }
    else
    {
        return updateOLEDScreenRange( 0, OLED_WIDTH - 1, 0, SSD1306_NUM_PAGES - 1 );
    }
}

//==============================================================================
// Commands Processor
//==============================================================================

/**
 * Execute a script of SSD1306 commands
 *
 * @param buffer the buffer of commands to send.  Each command should be prefixed
 *               with PCD_ commands, and data.
 *
 * @param flags  set flags to include one or multiple of:
 *               PCD_FLAGS_EXECUTE_ALL: To execute all commands without pausing
 *               PCD_FLAGS_EXECUTE_CONDITION: To execute conditional commands.

 * @return 0 an PCD_END command was found
 *         positive integer: number of commands found in this command (only
 *                           applicable if PCD_FLAGS_EXECUTE_ALL is not set)
 *         negative number:  see PCD_FAIL_*
 */
int ICACHE_FLASH_ATTR processDisplayCommands( const uint8_t * buffer, uint8_t flags )
{
    int offset = 0;
    while( (flags & PCD_FLAGS_EXECUTE_ALL) || offset == 0 )
    {
        uint8_t cmd = buffer[offset++];
        //Not setup as a switch statement to prevent excessive memory usage.
        if( cmd == PCD_END )
        {
            return 0;
        }
        if( ( cmd & 0xfc ) == 0xf4 )
        {
            if( ! (flags & PCD_FLAGS_EXECUTE_CONDITION ) )
            {
                //If condition is false, don't execute commands.
                offset += (cmd & 0x03) + 1;
                continue;
            }
        }
        if( ( cmd & 0xf8 ) == 0xf0 )
        {
            SendStart( OLED_HIGH_SPEED );
            if( SendByte( OLED_ADDRESS << 1, OLED_HIGH_SPEED ) ) return PCD_FAIL_DEVICE;
            SendByte( SSD1306_CMD, OLED_HIGH_SPEED );
            SendByte( buffer[offset++], OLED_HIGH_SPEED );
            cmd &= 3; //Pull off # of parameters.
            if( cmd > 0 ) SendByte( buffer[offset++], OLED_HIGH_SPEED );
            if( cmd > 1 ) SendByte( buffer[offset++], OLED_HIGH_SPEED );
            if( cmd > 2 ) SendByte( buffer[offset++], OLED_HIGH_SPEED );
            SendStop(OLED_HIGH_SPEED);
        }
        else
        {
            return PCD_FAIL_COMMANDS;
        }
    }
    return offset;
}
#endif
