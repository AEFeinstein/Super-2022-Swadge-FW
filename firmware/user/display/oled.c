/*
 * oled.c
 *
 *  Created on: Mar 16, 2019
 *      Author: adam
 */

//==============================================================================
// Includes
//==============================================================================

#include "osapi.h"
#include "oled.h"
#include "brzo_i2c.h"
#include "gpio_user.h"

//==============================================================================
// Defines and Enums
//==============================================================================

#define OLED_ADDRESS (0x78 >> 1)
#define OLED_FREQ    800

typedef enum
{
    HORIZONTAL_ADDRESSING = 0x00,
    VERTICAL_ADDRESSING   = 0x01,
    PAGE_ADDRESSING       = 0x02
} memoryAddressingMode;

typedef enum
{
    Vcc_X_0_65 = 0x00,
    Vcc_X_0_77 = 0x20,
    Vcc_X_0_83 = 0x30,
} VcomhDeselectLevel;

typedef enum
{
    SSD1306_CMD  = 0x00,
    SSD1306_DATA = 0x40
} SSD1306_prefix;

typedef enum
{
    SSD1306_MEMORYMODE          = 0x20,
    SSD1306_COLUMNADDR          = 0x21,
    SSD1306_PAGEADDR            = 0x22,
    SSD1306_SETCONTRAST         = 0x81,
    SSD1306_CHARGEPUMP          = 0x8D,
    SSD1306_SEGREMAP            = 0xA0,
    SSD1306_DISPLAYALLON_RESUME = 0xA4,
    SSD1306_DISPLAYALLON        = 0xA5,
    SSD1306_NORMALDISPLAY       = 0xA6,
    SSD1306_INVERTDISPLAY       = 0xA7,
    SSD1306_SETMULTIPLEX        = 0xA8,
    SSD1306_DISPLAYOFF          = 0xAE,
    SSD1306_DISPLAYON           = 0xAF,
    SSD1306_COMSCANINC          = 0xC0,
    SSD1306_COMSCANDEC          = 0xC8,
    SSD1306_SETDISPLAYOFFSET    = 0xD3,
    SSD1306_SETDISPLAYCLOCKDIV  = 0xD5,
    SSD1306_SETPRECHARGE        = 0xD9,
    SSD1306_SETCOMPINS          = 0xDA,
    SSD1306_SETVCOMDETECT       = 0xDB,

    SSD1306_SETLOWCOLUMN        = 0x00,
    SSD1306_SETHIGHCOLUMN       = 0x10,
    SSD1306_SETSTARTLINE        = 0x40,

    SSD1306_EXTERNALVCC         = 0x01,
    SSD1306_SWITCHCAPVCC        = 0x02,

    SSD1306_RIGHT_HORIZONTAL_SCROLL              = 0x26,
    SSD1306_LEFT_HORIZONTAL_SCROLL               = 0x27,
    SSD1306_VERTICAL_AND_RIGHT_HORIZONTAL_SCROLL = 0x29,
    SSD1306_VERTICAL_AND_LEFT_HORIZONTAL_SCROLL  = 0x2A,
    SSD1306_DEACTIVATE_SCROLL                    = 0x2E,
    SSD1306_ACTIVATE_SCROLL                      = 0x2F,
    SSD1306_SET_VERTICAL_SCROLL_AREA             = 0xA3,
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

//==============================================================================
// Variables
//==============================================================================

uint8_t buffer[1 + (OLED_WIDTH * (OLED_HEIGHT / 8))] = { SSD1306_DATA };

//==============================================================================
// Functions
//==============================================================================

/**
 * Clear the display.
 */
void ICACHE_FLASH_ATTR clearDisplay(void)
{
    ets_memset(buffer, 0, sizeof(buffer));
    buffer[0] = SSD1306_DATA;
}

/**
 * TODO
 *
 * @param x1
 * @param y1
 * @param x2
 * @param y2
 * @param c
 */
void ICACHE_FLASH_ATTR fillDisplayArea(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, color c)
{
    uint8_t x, y;
    for(x = x1; x <= x2; x++)
    {
        for(y = y1; y <= y2; y++)
        {
            drawPixel(x, y, c);
        }
    }
    buffer[0] = SSD1306_DATA;
}

/**
 * Set/clear/invert a single pixel.
 *
 * @param x Column of display, 0 is at the left
 * @param y Row of the display, 0 is at the top
 * @param c Pixel color, one of: BLACK, WHITE or INVERT
 */
void ICACHE_FLASH_ATTR drawPixel(uint8_t x, uint8_t y, color c)
{
    if((x < OLED_WIDTH) && (y < OLED_HEIGHT))
    {
        x = (OLED_WIDTH  - 1) - x;
        y = (OLED_HEIGHT - 1) - y;
        if(y % 2 == 0)
        {
            y = (y >> 1);
        }
        else
        {
            y = (y >> 1) + (OLED_HEIGHT >> 1);
        }
        switch(c)
        {
        case WHITE:
            buffer[1 + (x + (y / 8)*OLED_WIDTH)] |=  (1 << (y & 7));
            break;
        case BLACK:
            buffer[1 + (x + (y / 8)*OLED_WIDTH)] &= ~(1 << (y & 7));
            break;
        case INVERSE:
            buffer[1 + (x + (y / 8)*OLED_WIDTH)] ^=  (1 << (y & 7));
            break;
        default: {
            break;
        }
        }
    }
}

/**
 * Initialize the SSD1206 OLED
 *
 * @param reset true to reset the OLED using the RST line, false to leave it alone
 * @return true if it initialized, false if it failed
 */
bool ICACHE_FLASH_ATTR begin(bool reset)
{
    // Clear the RAM
    clearDisplay();

    // Reset SSD1306 if requested and reset pin specified in constructor
    if(reset)
    {
        setOledResetOn(true);  // VDD goes high at start
        ets_delay_us(1000);    // pause for 1 ms
        setOledResetOn(false);  // Bring reset low
        ets_delay_us(10000);   // Wait 10 ms
        setOledResetOn(true);  // Bring out of reset
    }

    // Start i2c
    brzo_i2c_start_transaction(OLED_ADDRESS, OLED_FREQ);

    // Init sequence
    setDisplayOn(false);
    setMultiplexRatio(OLED_HEIGHT - 1);
    setDisplayOffset(0);
    setDisplayStartLine(0);
    setMemoryAddressingMode(HORIZONTAL_ADDRESSING);
    setSegmentRemap(true);
    setComOutputScanDirection(false);
    setComPinsHardwareConfig(true, false);
    setContrastControl(0x7F);
    setPrechargePeriod(1, 15);
    setVcomhDeselectLevel(Vcc_X_0_77);
    entireDisplayOn(false);
    setInverseDisplay(false);
    setDisplayClockDivideRatio(0, 8);
    setChargePumpSetting(true);
    activateScroll(false);
    setDisplayOn(true);

    // End i2c
    return (0 == brzo_i2c_end_transaction());
}

/**
 * Push data currently in RAM to SSD1306 display.
 * This takes ~12ms @ 800KHz
 *
 * @return true if the data was sent, false if it failed
 */
bool ICACHE_FLASH_ATTR display(void)
{
    // Start i2c
    brzo_i2c_start_transaction(OLED_ADDRESS, OLED_FREQ);

    setPageAddress(0, 7);
    setColumnAddress(0, OLED_WIDTH - 1);
    brzo_i2c_write(buffer, sizeof(buffer), false);

    // end i2c
    return (0 == brzo_i2c_end_transaction());
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
    brzo_i2c_write(data, sizeof(data), false);
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
    brzo_i2c_write(data, sizeof(data), false);
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
    brzo_i2c_write(data, sizeof(data), false);
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
    brzo_i2c_write(data, sizeof(data), false);
}



//==============================================================================
// Scrolling Command Table
//==============================================================================

// TODO fill out

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
    brzo_i2c_write(data, sizeof(data), false);
}

//==============================================================================
// Addressing Setting Commands
//==============================================================================

// TODO fill out

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
    brzo_i2c_write(data, sizeof(data), false);
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
    if(startAddr > 127 || endAddr > 127)
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
    brzo_i2c_write(data, sizeof(data), false);
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
    if(startAddr > 7 || endAddr > 7)
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
    brzo_i2c_write(data, sizeof(data), false);
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
    if(startLineRegister > 63)
    {
        return;
    }
    uint8_t data[] =
    {
        SSD1306_CMD,
        SSD1306_SETSTARTLINE | (startLineRegister & 0x3F)
    };
    brzo_i2c_write(data, sizeof(data), false);
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
    brzo_i2c_write(data, sizeof(data), false);
}

/**
 * Set Multiplex Ratio
 *
 * @param ratio  from 16MUX to 64MUX, RESET= 111111b (i.e. 63d, 64MUX)
 */
void ICACHE_FLASH_ATTR setMultiplexRatio(uint8_t ratio)
{
    if(ratio < 15 || ratio > 63)
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
    brzo_i2c_write(data, sizeof(data), false);
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
    brzo_i2c_write(data, sizeof(data), false);
}

/**
 * Set vertical shift by COM from 0d~63d. The value is reset to 00h after RESET.
 *
 * @param offset The offset, 0d~63d
 */
void ICACHE_FLASH_ATTR setDisplayOffset(uint8_t offset)
{
    if(offset > 63)
    {
        return;
    }
    uint8_t data[] =
    {
        SSD1306_CMD,
        SSD1306_SETDISPLAYOFFSET,
        offset
    };
    brzo_i2c_write(data, sizeof(data), false);
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
    brzo_i2c_write(data, sizeof(data), false);
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
    if(divideRatio > 15 || oscFreq > 15)
    {
        return;
    }
    uint8_t data[] =
    {
        SSD1306_CMD,
        SSD1306_SETDISPLAYCLOCKDIV,
        (divideRatio & 0x0F) | ((oscFreq << 4) & 0xF0)
    };
    brzo_i2c_write(data, sizeof(data), false);
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
    if(phase1period > 15 || phase2period > 15)
    {
        return;
    }
    uint8_t data[] =
    {
        SSD1306_CMD,
        SSD1306_SETPRECHARGE,
        (phase1period & 0x0F) | ((phase2period << 4) & 0xF0)
    };
    brzo_i2c_write(data, sizeof(data), false);
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
    brzo_i2c_write(data, sizeof(data), false);
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
    brzo_i2c_write(data, sizeof(data), false);
}
