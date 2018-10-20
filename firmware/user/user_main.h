/*
 * user_main.h
 *
 *  Created on: Oct 19, 2018
 *      Author: adam
 */

#ifndef USER_USER_MAIN_H_
#define USER_USER_MAIN_H_

#include "c_types.h"
#include "esp82xxutil.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef bool (*fnEnterMode)(void);
typedef void (*fnExitMode)(void);
typedef void (*fnTimerCallback)(void);

typedef void (*fnAudioCallback)(int32_t audoSample);
typedef void (*fnButtonCallback)(uint8_t state, int button, int down);

typedef void (*fnConnectionCallback)(bool isConnected);
typedef void (*fnPacketCallback)(uint8_t * packet, uint8_t packetLen);

typedef struct _swadgeMode swadgeMode;

struct _swadgeMode {
	bool shouldConnect;
	fnEnterMode enterMode;
	fnExitMode exitMode;
	fnTimerCallback timerCallback;
	fnButtonCallback buttonCallback;
	fnAudioCallback audioCallback;
	fnConnectionCallback connectionCallback;
	fnPacketCallback packetCallback;
	swadgeMode * next;
};

void ICACHE_FLASH_ATTR setLeds(uint8_t * ledData, uint16_t ledDataLen);
void ICACHE_FLASH_ATTR sendPacket(uint8_t * packet, uint16_t packetLen);

#endif /* USER_USER_MAIN_H_ */
