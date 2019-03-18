/*
 * oled.c
 *
 *  Created on: Mar 16, 2019
 *      Author: adam
 */

#include "osapi.h"
#include "oled.h"
#include "brzo_i2c.h"

#define WIDTH 128
#define HEIGHT 64

#define OLED_ADDRESS 0x78
#define OLED_FREQ    400

uint8_t buffer[WIDTH][HEIGHT] = { { 0 } };

uint8_t vccstate = 0;

// Issue single command to SSD1306, using I2C or hard/soft SPI as needed.
// Because command calls are often grouped, SPI transaction and selection
// must be started/ended in calling function for efficiency.
// This is a private function, not exposed (see ssd1306_command() instead).
inline void ssd1306_command1(uint8_t c) {
	brzo_i2c_start_transaction(OLED_ADDRESS, OLED_FREQ);
	uint8_t data[2] = { 0x00, c };
	brzo_i2c_write(data, sizeof(data), false);
	brzo_i2c_end_transaction();
}

// Issue list of commands to SSD1306, same rules as above re: transactions.
// This is a private function, not exposed.
inline void ssd1306_commandList(const uint8_t *c, uint8_t n) {
	brzo_i2c_start_transaction(OLED_ADDRESS, OLED_FREQ);
	uint8_t data[1] = { 0x00 };
	brzo_i2c_write(data, sizeof(data), false);
	brzo_i2c_write(c, n, false);
	brzo_i2c_end_transaction();
}

/**
 *
 * @param vcs
 * @param addr
 * @param reset
 * @param periphBegin
 * @return
 */
bool ICACHE_FLASH_ATTR begin(uint8_t vcs, uint8_t addr, bool reset, bool periphBegin) {

	ets_memset(&buffer[0][0], 0, sizeof(buffer));

	uint8_t w, h;
	for (w = 0; w < WIDTH; w++) {
		for (h = 0; h < HEIGHT; h++) {
			if (w % 2 == 0 && h % 2 == 0) {
				buffer[w][h] = 0xFF;
			}
		}
	}

	vccstate = vcs;

	// Reset SSD1306 if requested and reset pin specified in constructor
//  if(reset && (rstPin >= 0)) {
//    pinMode(     rstPin, OUTPUT);
//    digitalWrite(rstPin, HIGH);
//    delay(1);                   // VDD goes high at start, pause for 1 ms
//    digitalWrite(rstPin, LOW);  // Bring reset low
//    delay(10);                  // Wait 10 ms
//    digitalWrite(rstPin, HIGH); // Bring out of reset
//  }

// Init sequence
	static const uint8_t init1[] = {
			SSD1306_DISPLAYOFF,                   // 0xAE
			SSD1306_SETDISPLAYCLOCKDIV,           // 0xD5
			0x80,                                 // the suggested ratio 0x80
			SSD1306_SETMULTIPLEX };               // 0xA8
	ssd1306_commandList(init1, sizeof(init1));
	ssd1306_command1(HEIGHT - 1);

	static const uint8_t init2[] = {
			SSD1306_SETDISPLAYOFFSET,             // 0xD3
			0x0,                                  // no offset
			SSD1306_SETSTARTLINE | 0x0,           // line #0
			SSD1306_CHARGEPUMP };                 // 0x8D
	ssd1306_commandList(init2, sizeof(init2));

	ssd1306_command1((vccstate == SSD1306_EXTERNALVCC) ? 0x10 : 0x14);

	static const uint8_t init3[] = {
			SSD1306_MEMORYMODE,                   // 0x20
			0x00,                                 // 0x0 act like ks0108
			SSD1306_SEGREMAP | 0x1,
			SSD1306_COMSCANDEC };
	ssd1306_commandList(init3, sizeof(init3));

//	if ((WIDTH == 128) && (HEIGHT == 32)) {
//		static const uint8_t init4a[] = {
//		SSD1306_SETCOMPINS,                 // 0xDA
//				0x02,
//				SSD1306_SETCONTRAST,                // 0x81
//				0x8F };
//		ssd1306_commandList(init4a, sizeof(init4a));
//	} else if ((WIDTH == 128) && (HEIGHT == 64)) {
		static const uint8_t init4b[] = {
				SSD1306_SETCOMPINS,                 // 0xDA
				0x12,
				SSD1306_SETCONTRAST };              // 0x81
		ssd1306_commandList(init4b, sizeof(init4b));
		ssd1306_command1((vccstate == SSD1306_EXTERNALVCC) ? 0x9F : 0xCF);
//	} else if ((WIDTH == 96) && (HEIGHT == 16)) {
//		static const uint8_t init4c[] = {
//		SSD1306_SETCOMPINS,                 // 0xDA
//				0x2,    // ada x12
//				SSD1306_SETCONTRAST };              // 0x81
//		ssd1306_commandList(init4c, sizeof(init4c));
//		ssd1306_command1((vccstate == SSD1306_EXTERNALVCC) ? 0x10 : 0xAF);
//	} else {
//		// Other screen varieties -- TBD
//	}

	ssd1306_command1(SSD1306_SETPRECHARGE); // 0xd9
	ssd1306_command1((vccstate == SSD1306_EXTERNALVCC) ? 0x22 : 0xF1);
	static const uint8_t init5[] = {
			SSD1306_SETVCOMDETECT,               // 0xDB
			0x40,
			SSD1306_DISPLAYALLON_RESUME,         // 0xA4
			SSD1306_NORMALDISPLAY,               // 0xA6
			SSD1306_DEACTIVATE_SCROLL,
			SSD1306_DISPLAYON };                 // Main screen turn on
	ssd1306_commandList(init5, sizeof(init5));

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
	static const uint8_t dlist1[] = {
			SSD1306_PAGEADDR,
			0,                         // Page start address
			0xFF,                      // Page end (not really, but works here)
			SSD1306_COLUMNADDR,
			0 };                     // Column start address
	ssd1306_commandList(dlist1, sizeof(dlist1));
	ssd1306_command1(WIDTH - 1); // Column end address

	brzo_i2c_start_transaction(OLED_ADDRESS, OLED_FREQ);
	uint8_t startByte = 0x40;
	brzo_i2c_write(&startByte, 1, false);
	brzo_i2c_write(&buffer[0][0], WIDTH * HEIGHT, false);
	brzo_i2c_end_transaction();
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
	ssd1306_commandList(dimCmd, sizeof(dimCmd));
}
