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

typedef struct _swadgeMode swadgeMode;

/**
 * A struct of all the function pointers necessary for a swadge mode. If a mode
 * does not need a particular function, say it doesn't do audio handling, it
 * is safe to set the pointer to NULL. It just won't be called.
 */
struct _swadgeMode
{
    /**
     * This swadge mode's name. Used if this mode connects to other swadges
     * over WiFi to label packets. Must be distinct for each mode.
     */
    char* modeName;  // TODO implement this
    /**
     * This function is called when this mode is started. It should initialize
     * any necessary variables
     */
    void (*fnEnterMode)(void);
    /**
     * This function is called when the mode is exited. It should clean up
     * anything that shouldn't happen when the mode is not active
     */
    void (*fnExitMode)(void);
    /**
     * This function is called every 100ms from user_main.c's timerFunc100ms().
     * It should be used to update any state based on time
     */
    void (*fnTimerCallback)(void);
    /**
     * This function is called when a button press is detected from user_main.c's
     * HandleButtonEvent(). It does not pass mode select button events. It is
     * called from an interrupt, so do the minimal amount of processing here as
     * possible. It is better to do heavy processing in timerCallback(). Any
     * global variables shared between buttonCallback() and other functions must
     * be declared volatile.
     *
     * @param state A bitmask of all button statuses
     * @param button  The button number which was pressed
     * @param down 1 if the button was pressed, 0 if it was released
     */
    void (*fnButtonCallback)(uint8_t state, int button, int down);
    /**
     * This function is called whenever an audio sample is read from the
     * microphone (ADC), is filtered, and is ready for processing
     *
     * @param audoSample A 32 bit audio sample
     */
    void (*fnAudioCallback)(int32_t audoSample);
    /**
     * This is a setting, not a function pointer. Set it to one of these
     * values to have the system configure the swadge's WiFi
     * NULL_MODE - Turn off WiFi
     * STATION_MODE - ???
     * SOFTAP_MODE - Become a WiFi access point serving up the colorchord
     *               configuration website
     * STATIONAP_MODE - Attempt to connect to the physically closest swadge
     *                  in an ad-hoc manner
     */
    uint8_t shouldConnect; // TODO implement this
    /**
     * If shouldConnect is set to STATIONAP_MODE, the LED color to be used
     * during the connection process. Must be distinct for each mode.
     */
    uint32_t connectionColor; // TODO implement this
    /**
     * If shouldConnect is set to STATIONAP_MODE, this function will be called
     * when the swadge connects or disconnects to another swadge. While the
     * swadge is connecting, the mode's functions will not be called. The
     * system will take over the LEDs for status purposes
     *
     * @param isConnected true if the swadge connects, false if it disconnects
     */
    void (*fnConnectionCallback)(bool isConnected); // TODO implement this
    /**
     * If shouldConnect is set to STATIONAP_MODE, this function will be called
     * when a packet is received from the other swadge
     *
     * @param packet    The data to send to the connected swadge
     * @param packetLen The length of the data to send to the connected swadge
     */
    void (*fnPacketCallback)(uint8_t* packet, uint8_t packetLen); // TODO implement this
    /**
     * A pointer to another swadgeMode, used by the system to register a
     * linked list of modes. This must be initialized to NULL.
     */
    swadgeMode* next;
};

/**
 * Set the state of the six RGB LEDs, but don't overwrite if the LEDs were
 * set via UDP for at least TICKER_TIMEOUT increments of 100ms
 *
 * @param ledData    Array of LED color data. Every three bytes corresponds to
 *                   one LED in RGB order. So index 0 is LED1_R, index 1 is
 *                   LED1_G, index 2 is LED1_B, index 3 is LED2_R, etc.
 * @param ledDataLen The length of buffer, most likely 6*3
 */
void ICACHE_FLASH_ATTR setLeds(uint8_t* ledData, uint16_t ledDataLen);

/**
 * Send a UDP packet to the swadge this swadge is connected to, if it's connected
 *
 * @param packet    The bytes to send to the other swadge
 * @param packetLen The length of the bytes to send to the other swadge
 */
void ICACHE_FLASH_ATTR sendPacket(uint8_t* packet, uint16_t packetLen);

#endif /* USER_USER_MAIN_H_ */
