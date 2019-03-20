/*
 * oled.c
 *
 *  Created on: Mar 16, 2019
 *      Author: adam
 */

#include "osapi.h"
#include "oled.h"
#include "brzo_i2c.h"
#include "gpio_buttons.h"

#define WIDTH 128
#define HEIGHT 64

#define OLED_ADDRESS (0x78 >> 1)
#define OLED_FREQ    400

typedef enum {
	HORIZONTAL_ADDRESSING = 0x00,
	VERTICAL_ADDRESSING   = 0x01,
	PAGE_ADDRESSING       = 0x02
} memoryAddressingMode;

typedef enum {
	Vcc_X_0_65 = 0x00,
	Vcc_X_0_77 = 0x20,
	Vcc_X_0_83 = 0x30,
} VcomhDeselectLevel;

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

//uint8_t buffer[WIDTH][HEIGHT / 8] = { { 0 } };
uint8_t buffer[WIDTH * (HEIGHT / 8)] = { 0 };
uint8_t vccstate = 0;

/*!
    @brief  Set/clear/invert a single pixel. This is also invoked by the
            Adafruit_GFX library in generating many higher-level graphics
            primitives.
    @param  x
            Column of display -- 0 at left to (screen width - 1) at right.
    @param  y
            Row of display -- 0 at top to (screen height -1) at bottom.
    @param  color
            Pixel color, one of: BLACK, WHITE or INVERT.
    @return None (void).
    @note   Changes buffer contents only, no immediate effect on display.
            Follow up with a call to display(), or with other graphics
            commands as needed by one's own application.
*/
void ICACHE_FLASH_ATTR drawPixel(uint8_t x, uint8_t y, color c) {
	if((x < WIDTH) && (y < HEIGHT)) {
		// Pixel is in-bounds. Rotate coordinates if needed.
		x = WIDTH  - x - 1;
		y = HEIGHT - y - 1;
		switch(c) {
			case WHITE:   buffer[x + (y/8)*WIDTH] |=  (1 << (y&7)); break;
			case BLACK:   buffer[x + (y/8)*WIDTH] &= ~(1 << (y&7)); break;
			case INVERSE: buffer[x + (y/8)*WIDTH] ^=  (1 << (y&7)); break;
		}
	}
}

/**
 *
 * @param vcs
 * @param reset
 * @return
 */
bool ICACHE_FLASH_ATTR begin(uint8_t vcs, bool reset) {

	ets_memset(buffer, 0, sizeof(buffer));

	uint8_t w, h;
	for (w = 0; w < WIDTH; w++) {
		for (h = 0; h < HEIGHT; h++) {
			if (w % 2 == h % 2) {
				drawPixel(w, h, BLACK);
			}
			else {
				drawPixel(w, h, WHITE);
			}
		}
	}

	vccstate = vcs;

	// Reset SSD1306 if requested and reset pin specified in constructor
	if(reset) {
		gpio16_output_set(1);  // VDD goes high at start
		ets_delay_us(1000);    // pause for 1 ms
		gpio16_output_set(0);  // Bring reset low
		ets_delay_us(10000);   // Wait 10 ms
		gpio16_output_set(1);  // Bring out of reset
	}

	brzo_i2c_start_transaction(OLED_ADDRESS, OLED_FREQ);
	// Init sequence

	setDisplayOn(false);
	setDisplayClockDivideRatio(0, 8);
	setMultiplexRatio(HEIGHT - 1);

	setDisplayOffset(0);
	setDisplayStartLine(0);
	if(vccstate == SSD1306_EXTERNALVCC)
	{
		setChargePumpSetting(false);
	}
	else
	{
		setChargePumpSetting(true);
	}

	setMemoryAddressingMode(HORIZONTAL_ADDRESSING);
	setSegmentRemap(true);
	setComOutputScanDirection(false);

	setComPinsHardwareConfig(true, false);
	if(vccstate == SSD1306_EXTERNALVCC)
	{
		setContrastControl(0x9F);
	}
	else
	{
		setContrastControl(0xCF);
	}

	if (vccstate == SSD1306_EXTERNALVCC)
	{
		setPrechargePeriod(2, 2);
	}
	else
	{
		setPrechargePeriod(1, 15);
	}

	setVcomhDeselectLevel(Vcc_X_0_77);
	entireDisplayOn(false);
	setInverseDisplay(false);
	activateScroll(false);
	setDisplayOn(true);

	uint8_t err = brzo_i2c_end_transaction();
	os_printf("%s:%d: %02X\n", __func__, __LINE__, err);

	return true; // Success
}

/*!
 @brief  Push data currently in RAM to SSD1306 display.
 @return None (void).
 @note   Drawing operations are not visible until this function is
 called. Call after each graphics command, or after a whole set
 of graphics commands, as best needed by one's own application.
 */
void ICACHE_FLASH_ATTR display(void) {
	brzo_i2c_start_transaction(OLED_ADDRESS, OLED_FREQ);

	setPageAddress(0, 7);
	setColumnAddress(0, WIDTH-1);
	setDisplayStartLine(0);

	brzo_i2c_write(buffer, sizeof(buffer), false);

	uint8_t err = brzo_i2c_end_transaction();
	os_printf("%s:%d: %02X\n", __func__, __LINE__, err);
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
void ICACHE_FLASH_ATTR setContrastControl(uint8_t contrast) {
	uint8_t data[] = { SSD1306_SETCONTRAST, contrast };
	brzo_i2c_write(data, sizeof(data), false);
}

/**
 * Turn the entire display on
 *
 * @param ignoreRAM true  - Entire display ON, Output ignores RAM content
 *                  false - Resume to RAM content display (RESET) Output follows RAM content
 */
void ICACHE_FLASH_ATTR entireDisplayOn(bool ignoreRAM) {
	uint8_t data[] = {
			ignoreRAM ? SSD1306_DISPLAYALLON : SSD1306_DISPLAYALLON_RESUME };
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
void ICACHE_FLASH_ATTR setInverseDisplay(bool inverse) {
	uint8_t data[] = { inverse ? SSD1306_INVERTDISPLAY : SSD1306_NORMALDISPLAY };
	brzo_i2c_write(data, sizeof(data), false);
}

/**
 * Set the display on or off
 *
 * @param on true  - Display ON in normal mode
 *           false - Display OFF (sleep mode)
 */
void ICACHE_FLASH_ATTR setDisplayOn(bool on) {
	uint8_t data[] = { on ? SSD1306_DISPLAYON : SSD1306_DISPLAYOFF };
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
void ICACHE_FLASH_ATTR activateScroll(bool on) {
	uint8_t data[] = { on ? SSD1306_ACTIVATE_SCROLL : SSD1306_DEACTIVATE_SCROLL };
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
void ICACHE_FLASH_ATTR setMemoryAddressingMode(memoryAddressingMode mode) {
	uint8_t data[] = { SSD1306_MEMORYMODE, mode };
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
	uint8_t data[] = { SSD1306_COLUMNADDR, startAddr, endAddr };
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
	uint8_t data[] = { SSD1306_PAGEADDR, startAddr, endAddr };
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
	uint8_t data[] = { SSD1306_SETSTARTLINE | (startLineRegister & 0x3F) };
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
	uint8_t data[] = { SSD1306_SEGREMAP | (colAddr ? 0x01 : 0x00) };
	brzo_i2c_write(data, sizeof(data), false);
}

/**
 * Set Multiplex Ratio
 *
 * @param ratio  from 16MUX to 64MUX, RESET= 111111b (i.e. 63d, 64MUX)
 */
void ICACHE_FLASH_ATTR setMultiplexRatio(uint8_t ratio)
{
	if(ratio < 15 || ratio > 63) {
		// Invalid
		return;
	}
	uint8_t data[] = { SSD1306_SETMULTIPLEX, ratio};
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
	uint8_t data[] = { increment ? SSD1306_COMSCANINC : SSD1306_COMSCANDEC};
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
	uint8_t data[] = { SSD1306_SETDISPLAYOFFSET, offset};
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
	uint8_t data[] = { SSD1306_SETCOMPINS,
	(sequential ? 0x00 : 0x10) | (remap ? 0x20 : 0x00) | 0x02};
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
void ICACHE_FLASH_ATTR setDisplayClockDivideRatio(uint8_t divideRatio, uint8_t oscFreq) {
	if(divideRatio > 15 || oscFreq > 15)
	{
		return;
	}
	uint8_t data[] = { SSD1306_SETDISPLAYCLOCKDIV, (divideRatio & 0x0F) | ((oscFreq << 4) & 0xF0) };
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
void ICACHE_FLASH_ATTR setPrechargePeriod(uint8_t phase1period, uint8_t phase2period) {
	if(phase1period > 15 || phase2period > 15)
	{
		return;
	}
	uint8_t data[] = { SSD1306_SETPRECHARGE, (phase1period & 0x0F) | ((phase2period << 4) & 0xF0) };
	brzo_i2c_write(data, sizeof(data), false);
}

/**
 * Set VCOMH Deselect Level
 *
 * @param ratio ~0.65 x VCC, ~0.77 x VCC (RESET), or ~0.83 x VCC
 */
void ICACHE_FLASH_ATTR setVcomhDeselectLevel(VcomhDeselectLevel level) {
	uint8_t data[] = { SSD1306_SETVCOMDETECT, level };
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
void ICACHE_FLASH_ATTR setChargePumpSetting(bool enable) {
	uint8_t data[] = { SSD1306_CHARGEPUMP, 0x10 | (enable ? 0x04 : 0x00)};
	brzo_i2c_write(data, sizeof(data), false);
}
