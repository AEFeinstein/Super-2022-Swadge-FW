/*
 * mode_music.h
 *
 *  Created on: 14 Nov 2019
 *      Author: bbkiwi
 */

#ifndef MODES_MODE_MUSIC_H_
#define MODES_MODE_MUSIC_H_

notePeriod_t ICACHE_FLASH_ATTR midi2note(uint8_t mid);
void ICACHE_FLASH_ATTR generateScale(uint8_t* midiScale, uint8_t numNotes, uint8_t intervals[], uint8_t nIntervals);

extern swadgeMode musicMode;

#endif /* MODES_MODE_MUSIC_H_ */
