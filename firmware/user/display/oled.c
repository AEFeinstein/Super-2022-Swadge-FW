/*
 * oled.c
 *
 *  Created on: Mar 16, 2019
 *      Author: adam
 */

//==============================================================================
// Includes
//==============================================================================

#include <osapi.h>

#include "oled.h"
#include "cnlohr_i2c.h"
#include "gpio_user.h"

#if defined(FEATURE_OLED)

//==============================================================================
// Defines and Enums
//==============================================================================

#define OLED_ADDRESS (0x78 >> 1)
#define OLED_FREQ 800
#define OLED_HIGH_SPEED 1

#define SSD1306_NUM_PAGES 8
#define SSD1306_NUM_COLS 128

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
    SSD1306_MEMORYMODE = 0x20,
    SSD1306_COLUMNADDR = 0x21,
    SSD1306_PAGEADDR = 0x22,
    SSD1306_SETCONTRAST = 0x81,
    SSD1306_CHARGEPUMP = 0x8D,
    SSD1306_SEGREMAP = 0xA0,
    SSD1306_DISPLAYALLON_RESUME = 0xA4,
    SSD1306_DISPLAYALLON = 0xA5,
    SSD1306_NORMALDISPLAY = 0xA6,
    SSD1306_INVERTDISPLAY = 0xA7,
    SSD1306_SETMULTIPLEX = 0xA8,
    SSD1306_DISPLAYOFF = 0xAE,
    SSD1306_DISPLAYON = 0xAF,
    SSD1306_PAGEADDRPAGING = 0xB0,
    SSD1306_COMSCANINC = 0xC0,
    SSD1306_COMSCANDEC = 0xC8,
    SSD1306_SETDISPLAYOFFSET = 0xD3,
    SSD1306_SETDISPLAYCLOCKDIV = 0xD5,
    SSD1306_SETPRECHARGE = 0xD9,
    SSD1306_SETCOMPINS = 0xDA,
    SSD1306_SETVCOMDETECT = 0xDB,

    SSD1306_SETLOWCOLUMN = 0x00,
    SSD1306_SETHIGHCOLUMN = 0x10,
    SSD1306_SETSTARTLINE = 0x40,

    SSD1306_EXTERNALVCC = 0x01,
    SSD1306_SWITCHCAPVCC = 0x02,

    SSD1306_RIGHT_HORIZONTAL_SCROLL = 0x26,
    SSD1306_LEFT_HORIZONTAL_SCROLL = 0x27,
    SSD1306_VERTICAL_AND_RIGHT_HORIZONTAL_SCROLL = 0x29,
    SSD1306_VERTICAL_AND_LEFT_HORIZONTAL_SCROLL = 0x2A,
    SSD1306_DEACTIVATE_SCROLL = 0x2E,
    SSD1306_ACTIVATE_SCROLL = 0x2F,
    SSD1306_SET_VERTICAL_SCROLL_AREA = 0xA3,
} SSD1306_cmd;

//==============================================================================
// Internal Function Declarations
//==============================================================================

void ICACHE_FLASH_ATTR setContrastControl(uint8_t contrast);
void ICACHE_FLASH_ATTR entireDisplayOn(bool ignoreRAM);
void ICACHE_FLASH_ATTR setInverseDisplay(bool inverse);
void ICACHE_FLASH_ATTR setDisplayOn(bool on);

void ICACHE_FLASH_ATTR activateScroll(bool on);

void ICACHE_FLASH_ATTR setMemoryAddressingMode(memoryAddressingMode mode);
void ICACHE_FLASH_ATTR setColumnAddress(uint8_t startAddr, uint8_t endAddr);
void ICACHE_FLASH_ATTR setPageAddress(uint8_t startAddr, uint8_t endAddr);

void ICACHE_FLASH_ATTR setDisplayStartLine(uint8_t startLineRegister);
void ICACHE_FLASH_ATTR setSegmentRemap(bool colAddr);
void ICACHE_FLASH_ATTR setMultiplexRatio(uint8_t ratio);
void ICACHE_FLASH_ATTR setComOutputScanDirection(bool increment);
void ICACHE_FLASH_ATTR setDisplayOffset(uint8_t offset);
void ICACHE_FLASH_ATTR setComPinsHardwareConfig(bool sequential, bool remap);

void ICACHE_FLASH_ATTR setDisplayClockDivideRatio(uint8_t divideRatio, uint8_t oscFreq);
void ICACHE_FLASH_ATTR setPrechargePeriod(uint8_t phase1period, uint8_t phase2period);
void ICACHE_FLASH_ATTR setVcomhDeselectLevel(VcomhDeselectLevel level);

void ICACHE_FLASH_ATTR setChargePumpSetting(bool enable);

void ICACHE_FLASH_ATTR setPageAddressPagingMode(uint8_t page);
void ICACHE_FLASH_ATTR setLowerColAddrPagingMode(uint8_t col);
void ICACHE_FLASH_ATTR setUpperColAddrPagingMode(uint8_t col);

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
 * This takes ~0.9ms to execute
 */
bool ICACHE_FLASH_ATTR setOLEDparams(bool turnOnOff)
{
    // Start i2c
    cnlohr_i2c_start_transaction(OLED_ADDRESS, OLED_FREQ);

    // Init sequence
    if(true == turnOnOff)
    {
        setDisplayOn(false);
    }
    setMultiplexRatio(OLED_HEIGHT - 1);
    setDisplayOffset(0);
    setDisplayStartLine(0);
    setMemoryAddressingMode(VERTICAL_ADDRESSING);
#if (SWADGE_VERSION == BARREL_1_0_0)
    setSegmentRemap(true);
    setComOutputScanDirection(false);
#else
    setSegmentRemap(false);
    setComOutputScanDirection(true);
#endif
    setComPinsHardwareConfig(false, false);
    setContrastControl(0x7F);
    setPrechargePeriod(1, 15);
    setVcomhDeselectLevel(Vcc_X_0_77);
    if(true == turnOnOff)
    {
        entireDisplayOn(false);
    }
    setInverseDisplay(false);
    setDisplayClockDivideRatio(0, 8);
    setChargePumpSetting(true);
    activateScroll(false);
    if(turnOnOff)
    {
        setDisplayOn(true);
    }

    // End i2c
    return (0 == cnlohr_i2c_end_transaction());
}

int ICACHE_FLASH_ATTR updateOLEDScreenRange( uint8_t minX, uint8_t maxX, uint8_t minPage, uint8_t maxPage )
{
    uint8_t encountered_error = false;
    uint8_t x, page;

    cnlohr_i2c_start_transaction(OLED_ADDRESS, OLED_FREQ);
    setColumnAddress( minX, maxX );
    setPageAddress( minPage, maxPage );
    if( cnlohr_i2c_end_transaction() ) encountered_error = true;

    SendStart(OLED_HIGH_SPEED);
    SendByte( OLED_ADDRESS << 1, OLED_HIGH_SPEED );
    SendByte( SSD1306_DATA, OLED_HIGH_SPEED );
    for( x = minX; x <= maxX; x++ )
    {
        int index = x*SSD1306_NUM_PAGES+minPage;
		uint8_t * prior = &priorFb[index];
		uint8_t * cur = &currentFb[index];

        for( page = minPage; page <= maxPage; page++ )
        {
            SendByteFast( *(prior++) = *(cur++) );
        }
    }
    SendStop(OLED_HIGH_SPEED);
    return encountered_error?FRAME_NOT_DRAWN:FRAME_DRAWN;
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
        uint8_t * pPrev = priorFb;
        uint8_t * pCur = currentFb;
        for( x = 0; x < OLED_WIDTH; x++ )
        {
            for( page = 0; page < SSD1306_NUM_PAGES; page++ )
            {
                if( *pPrev != *pCur )
                {
                    if( x < minX ) minX = x;
                    if( x > maxX ) maxX = x;
                    if( page < minPage ) minPage = page;
                    if( page > maxPage ) maxPage = page;
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
		return updateOLEDScreenRange( 0, OLED_WIDTH-1, 0, SSD1306_NUM_PAGES-1 );
	}
}

//==============================================================================
// Fundamental Commands
//==============================================================================

/**
 * Double byte command to select 1 out of 256 contrast steps. Contrast increases
 * as the value increases.
 *
 * (RESET = 7Fh)
 *
 * @param contrast the value to set as contrast. Adafruit uses 0 for dim, 0x9F
 *                 for external VCC contrast and 0xCF for 3.3V voltage
 */
void ICACHE_FLASH_ATTR setContrastControl(uint8_t contrast)
{
    uint8_t data[] =
    {
        SSD1306_CMD,
        SSD1306_SETCONTRAST,
        contrast
    };
    cnlohr_i2c_write(data, sizeof(data), false);
}

/**
 * Turn the entire display on
 *
 * @param ignoreRAM true  - Entire display ON, Output ignores RAM content
 *                  false - Resume to RAM content display (RESET) Output follows RAM content
 */
void ICACHE_FLASH_ATTR entireDisplayOn(bool ignoreRAM)
{
    uint8_t data[] =
    {
        SSD1306_CMD,
        ignoreRAM ? SSD1306_DISPLAYALLON : SSD1306_DISPLAYALLON_RESUME
    };
    cnlohr_i2c_write(data, sizeof(data), false);
}

/**
 * Set whether the display is color inverted or not
 *
 * @param inverse true  - Normal display (RESET)
 *                        0 in RAM: OFF in display panel
 *                        1 in RAM: ON in display panel
 *                false - Inverse display (RESET)
 *                        0 in RAM: ON in display panel
 *                        1 in RAM: OFF in display panel
 */
void ICACHE_FLASH_ATTR setInverseDisplay(bool inverse)
{
    uint8_t data[] =
    {
        SSD1306_CMD,
        inverse ? SSD1306_INVERTDISPLAY : SSD1306_NORMALDISPLAY
    };
    cnlohr_i2c_write(data, sizeof(data), false);
}

/**
 * Set the display on or off
 *
 * @param on true  - Display ON in normal mode
 *           false - Display OFF (sleep mode)
 */
void ICACHE_FLASH_ATTR setDisplayOn(bool on)
{
    uint8_t data[] =
    {
        SSD1306_CMD,
        on ? SSD1306_DISPLAYON : SSD1306_DISPLAYOFF
    };
    cnlohr_i2c_write(data, sizeof(data), false);
}

//==============================================================================
// Scrolling Command Table
//==============================================================================

/**
 *
 * @param on true  - Start scrolling that is configured by the scrolling setup
 *                   commands :26h/27h/29h/2Ah with the following valid sequences:
 *                     Valid command sequence 1: 26h ;2Fh.
 *                     Valid command sequence 2: 27h ;2Fh.
 *                     Valid command sequence 3: 29h ;2Fh.
 *                     Valid command sequence 4: 2Ah ;2Fh.
 *           false - Stop scrolling that is configured by command 26h/27h/29h/2Ah.
 */
void ICACHE_FLASH_ATTR activateScroll(bool on)
{
    uint8_t data[] =
    {
        SSD1306_CMD,
        on ? SSD1306_ACTIVATE_SCROLL : SSD1306_DEACTIVATE_SCROLL
    };
    cnlohr_i2c_write(data, sizeof(data), false);
}

//==============================================================================
// Addressing Setting Commands
//==============================================================================

/**
 * Set Memory Addressing Mode
 *
 * @param mode The mode: horizontal, vertical or page
 */
void ICACHE_FLASH_ATTR setMemoryAddressingMode(memoryAddressingMode mode)
{
    uint8_t data[] =
    {
        SSD1306_CMD,
        SSD1306_MEMORYMODE,
        mode
    };
    cnlohr_i2c_write(data, sizeof(data), false);
}

/**
 * Setup column start and end address
 *
 * This command is only for horizontal or vertical addressing mode.
 *
 * @param startAddr Column start address, range : 0-127d, (RESET=0d)
 * @param endAddr   Column end address, range : 0-127d, (RESET =127d)
 */
void ICACHE_FLASH_ATTR setColumnAddress(uint8_t startAddr, uint8_t endAddr)
{
    if (startAddr > 127 || endAddr > 127)
    {
        return;
    }
    uint8_t data[] =
    {
        SSD1306_CMD,
        SSD1306_COLUMNADDR,
        startAddr,
        endAddr
    };
    cnlohr_i2c_write(data, sizeof(data), false);
}

/**
 * Setup page start and end address
 *
 * This command is only for horizontal or vertical addressing mode.
 *
 * @param startAddr Page start Address, range : 0-7d, (RESET = 0d)
 * @param endAddr   Page end Address, range : 0-7d, (RESET = 7d)
 */
void ICACHE_FLASH_ATTR setPageAddress(uint8_t startAddr, uint8_t endAddr)
{
    if (startAddr > 7 || endAddr > 7)
    {
        return;
    }
    uint8_t data[] =
    {
        SSD1306_CMD,
        SSD1306_PAGEADDR,
        startAddr,
        endAddr
    };
    cnlohr_i2c_write(data, sizeof(data), false);
}

/**
 * @brief When in PAGE_ADDRESSING, address the page to write to
 *
 * @param page The page to write to, 0 to 7
 */
void ICACHE_FLASH_ATTR setPageAddressPagingMode(uint8_t page)
{
    if (page > 7)
    {
        return;
    }
    uint8_t data[] =
    {
        SSD1306_CMD,
        SSD1306_PAGEADDRPAGING + page,
    };
    cnlohr_i2c_write(data, sizeof(data), false);
}

/**
 * @brief When in PAGE_ADDRESSING, address the column's lower nibble
 *
 * @param col The lower nibble of the column to write to, 0 to 15
 */
void ICACHE_FLASH_ATTR setLowerColAddrPagingMode(uint8_t col)
{
    if (col > 15)
    {
        return;
    }
    uint8_t data[] =
    {
        SSD1306_CMD,
        SSD1306_SETLOWCOLUMN + col,
    };
    cnlohr_i2c_write(data, sizeof(data), false);
}

/**
 * @brief When in PAGE_ADDRESSING, address the column's upper nibble
 *
 * @param col The upper nibble of the column to write to, 0 to 15
 */
void ICACHE_FLASH_ATTR setUpperColAddrPagingMode(uint8_t col)
{
    if (col > 15)
    {
        return;
    }
    uint8_t data[] =
    {
        SSD1306_CMD,
        SSD1306_SETHIGHCOLUMN + col,
    };
    cnlohr_i2c_write(data, sizeof(data), false);
}

//==============================================================================
// Hardware Configuration Commands
//==============================================================================

/**
 * Set display RAM display start line register from 0-63
 * Display start line register is reset to 000000b during RESET.
 *
 * @param startLineRegister start line register, 0-63
 */
void ICACHE_FLASH_ATTR setDisplayStartLine(uint8_t startLineRegister)
{
    if (startLineRegister > 63)
    {
        return;
    }
    uint8_t data[] =
    {
        SSD1306_CMD,
        SSD1306_SETSTARTLINE | (startLineRegister & 0x3F)
    };
    cnlohr_i2c_write(data, sizeof(data), false);
}

/**
 * Set Segment Re-map
 *
 * @param colAddr true  - column address 127 is mapped to SEG0
 *                false - column address 0 is mapped to SEG0 (RESET)
 */
void ICACHE_FLASH_ATTR setSegmentRemap(bool colAddr)
{
    uint8_t data[] =
    {
        SSD1306_CMD,
        SSD1306_SEGREMAP | (colAddr ? 0x01 : 0x00)
    };
    cnlohr_i2c_write(data, sizeof(data), false);
}

/**
 * Set Multiplex Ratio
 *
 * @param ratio  from 16MUX to 64MUX, RESET= 111111b (i.e. 63d, 64MUX)
 */
void ICACHE_FLASH_ATTR setMultiplexRatio(uint8_t ratio)
{
    if (ratio < 15 || ratio > 63)
    {
        // Invalid
        return;
    }
    uint8_t data[] =
    {
        SSD1306_CMD,
        SSD1306_SETMULTIPLEX,
        ratio
    };
    cnlohr_i2c_write(data, sizeof(data), false);
}

/**
 * Set COM Output Scan Direction
 *
 * @param increment true  -  normal mode (RESET) Scan from COM0 to COM[N –1]
 *                  false -  remapped mode. Scan from COM[N-1] to COM0
 */
void ICACHE_FLASH_ATTR setComOutputScanDirection(bool increment)
{
    uint8_t data[] =
    {
        SSD1306_CMD,
        increment ? SSD1306_COMSCANINC : SSD1306_COMSCANDEC
    };
    cnlohr_i2c_write(data, sizeof(data), false);
}

/**
 * Set vertical shift by COM from 0d~63d. The value is reset to 00h after RESET.
 *
 * @param offset The offset, 0d~63d
 */
void ICACHE_FLASH_ATTR setDisplayOffset(uint8_t offset)
{
    if (offset > 63)
    {
        return;
    }
    uint8_t data[] =
    {
        SSD1306_CMD,
        SSD1306_SETDISPLAYOFFSET,
        offset
    };
    cnlohr_i2c_write(data, sizeof(data), false);
}

/**
 *
 * @param sequential true  - Sequential COM pin configuration
 *                   false - (RESET), Alternative COM pin configuration
 * @param remap true  - Enable COM Left/Right remap
 *              false - (RESET), Disable COM Left/Right remap
 */
void ICACHE_FLASH_ATTR setComPinsHardwareConfig(bool sequential, bool remap)
{
    uint8_t data[] =
    {
        SSD1306_CMD,
        SSD1306_SETCOMPINS,
        (sequential ? 0x00 : 0x10) | (remap ? 0x20 : 0x00) | 0x02
    };
    cnlohr_i2c_write(data, sizeof(data), false);
}

//==============================================================================
// Timing & Driving Scheme Setting Commands
//==============================================================================

/**
 * Set Display Clock Divide Ratio/Oscillator Frequency
 *
 * @param divideRatio  Define the divide ratio (D) of the display clocks (DCLK)
 *                     The actual ratio is 1 + this param, RESET is 0000b (divide ratio = 1)
 *                     Range:0000b~1111b
 * @param oscFreq Set the Oscillator Frequency. Oscillator Frequency increases
 *                with the value and vice versa. RESET is 1000b
 *                Range:0000b~1111b
 */
void ICACHE_FLASH_ATTR setDisplayClockDivideRatio(uint8_t divideRatio, uint8_t oscFreq)
{
    if (divideRatio > 15 || oscFreq > 15)
    {
        return;
    }
    uint8_t data[] =
    {
        SSD1306_CMD,
        SSD1306_SETDISPLAYCLOCKDIV,
        (divideRatio & 0x0F) | ((oscFreq << 4) & 0xF0)
    };
    cnlohr_i2c_write(data, sizeof(data), false);
}

/**
 * Set Pre-charge Period
 *
 * @param phase1period Phase 1 period of up to 15 DCLK clocks 0 is invalid entry (RESET=2h)
 *                     Range:0000b~1111b
 * @param phase2period Phase 2 period of up to 15 DCLK clocks 0 is invalid entry (RESET=2h )
 *                     Range:0000b~1111b
 */
void ICACHE_FLASH_ATTR setPrechargePeriod(uint8_t phase1period, uint8_t phase2period)
{
    if (phase1period > 15 || phase2period > 15)
    {
        return;
    }
    uint8_t data[] =
    {
        SSD1306_CMD,
        SSD1306_SETPRECHARGE,
        (phase1period & 0x0F) | ((phase2period << 4) & 0xF0)
    };
    cnlohr_i2c_write(data, sizeof(data), false);
}

/**
 * Set VCOMH Deselect Level
 *
 * @param ratio ~0.65 x VCC, ~0.77 x VCC (RESET), or ~0.83 x VCC
 */
void ICACHE_FLASH_ATTR setVcomhDeselectLevel(VcomhDeselectLevel level)
{
    uint8_t data[] =
    {
        SSD1306_CMD,
        SSD1306_SETVCOMDETECT,
        level
    };
    cnlohr_i2c_write(data, sizeof(data), false);
}

//==============================================================================
// Charge Bump Command
//==============================================================================

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
 */
void ICACHE_FLASH_ATTR setChargePumpSetting(bool enable)
{
    uint8_t data[] =
    {
        SSD1306_CMD,
        SSD1306_CHARGEPUMP,
        0x10 | (enable ? 0x04 : 0x00)
    };
    cnlohr_i2c_write(data, sizeof(data), false);
}

#endif
