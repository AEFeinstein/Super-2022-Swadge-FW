/*
 * oled.h
 *
 *  Created on: Mar 16, 2019
 *      Author: adam
 */

#ifndef OLED_H_
#define OLED_H_

#include "c_types.h"

typedef enum {
	BLACK = 0,
	WHITE = 1,
	INVERSE = 2
} color;

typedef enum {
	SSD1306_MEMORYMODE          = 0x20, ///< See datasheet
	SSD1306_COLUMNADDR          = 0x21, ///< See datasheet
	SSD1306_PAGEADDR            = 0x22, ///< See datasheet
	SSD1306_SETCONTRAST         = 0x81, ///< See datasheet
	SSD1306_CHARGEPUMP          = 0x8D, ///< See datasheet
	SSD1306_SEGREMAP            = 0xA0, ///< See datasheet
	SSD1306_DISPLAYALLON_RESUME = 0xA4, ///< See datasheet
	SSD1306_DISPLAYALLON        = 0xA5, ///< Not currently used
	SSD1306_NORMALDISPLAY       = 0xA6, ///< See datasheet
	SSD1306_INVERTDISPLAY       = 0xA7, ///< See datasheet
	SSD1306_SETMULTIPLEX        = 0xA8, ///< See datasheet
	SSD1306_DISPLAYOFF          = 0xAE, ///< See datasheet
	SSD1306_DISPLAYON           = 0xAF, ///< See datasheet
	SSD1306_COMSCANINC          = 0xC0, ///< Not currently used
	SSD1306_COMSCANDEC          = 0xC8, ///< See datasheet
	SSD1306_SETDISPLAYOFFSET    = 0xD3, ///< See datasheet
	SSD1306_SETDISPLAYCLOCKDIV  = 0xD5, ///< See datasheet
	SSD1306_SETPRECHARGE        = 0xD9, ///< See datasheet
	SSD1306_SETCOMPINS          = 0xDA, ///< See datasheet
	SSD1306_SETVCOMDETECT       = 0xDB, ///< See datasheet

	SSD1306_SETLOWCOLUMN        = 0x00, ///< Not currently used
	SSD1306_SETHIGHCOLUMN       = 0x10, ///< Not currently used
	SSD1306_SETSTARTLINE        = 0x40, ///< See datasheet

	SSD1306_EXTERNALVCC         = 0x01, ///< External display voltage source
	SSD1306_SWITCHCAPVCC        = 0x02, ///< Gen. display voltage from 3.3V

	SSD1306_RIGHT_HORIZONTAL_SCROLL              = 0x26, ///< Init rt scroll
	SSD1306_LEFT_HORIZONTAL_SCROLL               = 0x27, ///< Init left scroll
	SSD1306_VERTICAL_AND_RIGHT_HORIZONTAL_SCROLL = 0x29, ///< Init diag scroll
	SSD1306_VERTICAL_AND_LEFT_HORIZONTAL_SCROLL  = 0x2A, ///< Init diag scroll
	SSD1306_DEACTIVATE_SCROLL                    = 0x2E, ///< Stop scroll
	SSD1306_ACTIVATE_SCROLL                      = 0x2F, ///< Start scroll
	SSD1306_SET_VERTICAL_SCROLL_AREA             = 0xA3, ///< Set scroll range
} SSD1306_cmd;

bool begin(uint8_t vcs, bool reset);
void display(void);

#endif /* OLED_H_ */
