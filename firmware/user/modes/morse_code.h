/*
 * morse_code.h
 *
 *  Created on: Dec 6, 2018
 *      Author: adam
 */

#ifndef MORSE_CODE_H_
#define MORSE_CODE_H_

void ICACHE_FLASH_ATTR startMorseSequence(void (*fnWhenDone)(void));
void ICACHE_FLASH_ATTR endMorseSequence(void);

#endif /* MORSE_CODE_H_ */
