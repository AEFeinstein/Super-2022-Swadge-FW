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
	static const uint8_t init1[] = {
			SSD1306_DISPLAYOFF,                   // 0xAE
			SSD1306_SETDISPLAYCLOCKDIV,           // 0xD5
			0x80,                                 // the suggested ratio 0x80
			SSD1306_SETMULTIPLEX };               // 0xA8
	brzo_i2c_write(init1, sizeof(init1), false);

	static const uint8_t init2[] = {
			SSD1306_SETDISPLAYOFFSET,             // 0xD3
			0x0,                                  // no offset
			SSD1306_SETSTARTLINE | 0x0,           // line #0
			SSD1306_CHARGEPUMP };                 // 0x8D
	brzo_i2c_write(init2, sizeof(init2), false);

	uint8_t vccByte = (vccstate == SSD1306_EXTERNALVCC) ? 0x10 : 0x14;
	brzo_i2c_write(&vccByte, 1, false);

	static const uint8_t init3[] = {
			SSD1306_MEMORYMODE,                   // 0x20
			0x00,                                 // 0x0 act like ks0108
			SSD1306_SEGREMAP | 0x1,
			SSD1306_COMSCANDEC };
	brzo_i2c_write(init3, sizeof(init3), false);

	static const uint8_t init4b[] = {
			SSD1306_SETCOMPINS,                 // 0xDA
			0x12,
			SSD1306_SETCONTRAST };              // 0x81
	brzo_i2c_write(init4b, sizeof(init4b), false);

	vccByte = (vccstate == SSD1306_EXTERNALVCC) ? 0x9F : 0xCF;
	brzo_i2c_write(&vccByte, 1, false);

	vccByte = SSD1306_SETPRECHARGE;
	brzo_i2c_write(&vccByte, 1, false);

	vccByte = (vccstate == SSD1306_EXTERNALVCC) ? 0x22 : 0xF1;
	brzo_i2c_write(&vccByte, 1, false);

	static const uint8_t init5[] = {
			SSD1306_SETVCOMDETECT,               // 0xDB
			0x40,
			SSD1306_DISPLAYALLON_RESUME,         // 0xA4
			SSD1306_NORMALDISPLAY,               // 0xA6
			SSD1306_DEACTIVATE_SCROLL,
			SSD1306_DISPLAYON };                 // Main screen turn on
	brzo_i2c_write(init5, sizeof(init5), false);

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

	static const uint8_t dlist1[] = {
			SSD1306_PAGEADDR,
			0,                         // Page start address
			0xFF,                      // Page end (not really, but works here)
			SSD1306_COLUMNADDR,
			0 };                     // Column start address
	brzo_i2c_write(dlist1, sizeof(dlist1), false);

	uint8_t colEndAddr = WIDTH-1;
	brzo_i2c_write(&colEndAddr, 1, false);

	uint8_t startByte = 0x40;
	brzo_i2c_write(&startByte, 1, false);

	brzo_i2c_write(buffer, sizeof(buffer), false);

	uint8_t err = brzo_i2c_end_transaction();
	os_printf("%s:%d: %02X\n", __func__, __LINE__, err);
}

/*!
 @brief  Dim the display.
 @param  dim
 true to enable lower brightness mode, false for full brightness.
 @return None (void).
 @note   This has an immediate effect on the display, no need to call the
 display() function -- buffer contents are not changed.
 */
void ICACHE_FLASH_ATTR dim(bool dim) {
	uint8_t contrast;

	if (dim) {
		contrast = 0; // Dimmed display
	} else {
		contrast = (vccstate == SSD1306_EXTERNALVCC) ? 0x9F : 0xCF;
	}
	// the range of contrast to too small to be really useful
	// it is useful to dim the display
	uint8_t dimCmd[2] = { SSD1306_SETCONTRAST, contrast };

	brzo_i2c_start_transaction(OLED_ADDRESS, OLED_FREQ);
	brzo_i2c_write(dimCmd, sizeof(dimCmd), false);
	uint8_t err = brzo_i2c_end_transaction();
	os_printf("%s:%d: %02X\n", __func__, __LINE__, err);
}
