/*
 * mode_colorchord.h
 *
 *  Created on: Oct 19, 2018
 *      Author: adam
 */

#ifndef USER_MODE_COLORCHORD_H_
#define USER_MODE_COLORCHORD_H_

// Gain is an 8 bit number, max is 252 with these numbers
#define AMP_OFFSET    20
#define AMP_STEPS     9
#define AMP_STEP_SIZE 29

extern swadgeMode colorchordMode;

void ICACHE_FLASH_ATTR cycleColorchordSensitivity(void);
void ICACHE_FLASH_ATTR cycleColorchordOutput(void);

#endif /* USER_MODE_COLORCHORD_H_ */
