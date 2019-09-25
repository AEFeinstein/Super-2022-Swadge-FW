/*
 * user_main.h
 *
 *  Created on: Oct 19, 2018
 *      Author: adam
 */

#ifndef USER_USER_MAIN_H_
#define USER_USER_MAIN_H_

/*============================================================================
 * Defines
 *==========================================================================*/
// The accelerometer has two arrows marked x and y and dot in circle marking up
// can specify how these relate to the landscape view of OLED.
// #define ORIENTATIONFIX
#ifdef ORIENTATIONFIX
//bbkiwi swadge mockup
#define LEFTOLED accel.x
#define TOPOLED (-accel.y)
#define FACEOLED accel.z
//swadge dev kit
//#define LEFTOLED accel.y
//#define TOPOLED  accel.x
//#define FACEOLED accel.z
#endif

/*============================================================================
 * Includes
 *==========================================================================*/

#include <c_types.h>
#include <stdint.h>
#include <stddef.h>

/*============================================================================
* Enums
*==========================================================================*/

typedef enum
{
    NO_WIFI,
    SOFT_AP,
    ESP_NOW
} wifiMode_t;

typedef enum
{
    MT_TX_STATUS_OK = 0,
    MT_TX_STATUS_FAILED,
} mt_tx_status;

/*============================================================================
 * Structs
 *==========================================================================*/

typedef struct __attribute__ ((__packed__))
{
    uint8_t g;
    uint8_t r;
    uint8_t b;
}
led_t;

typedef struct
{
    int16_t x;
    int16_t y;
    int16_t z;
} accel_t;

/**
 * A struct of all the function pointers necessary for a swadge mode. If a mode
 * does not need a particular function, say it doesn't do audio handling, it
 * is safe to set the pointer to NULL. It just won't be called.
 */
typedef struct _swadgeMode
{
    /**
     * This swadge mode's name, mostly for debugging.
     * This is not a function pointer.
     */
    char* modeName;
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
     * This is a setting, not a function pointer. Set it to one of these
     * values to have the system configure the swadge's WiFi
     * NO_WIFI - Don't use WiFi at all. This saves power.
     * SOFT_AP - Become a WiFi access point serving up the colorchord
     *           configuration website
     * ESP_NOW - Send and receive packets to and from all swadges in range
     */
    wifiMode_t wifiMode;
    /**
     * This function is called whenever an ESP NOW packet is received.
     *
     * @param mac_addr The MAC address which sent this data
     * @param data     A pointer to the data received
     * @param len      The length of the data received
     * @param rssi     The RSSI for this packet, from 1 (weak) to ~90 (touching)
     */
    void (*fnEspNowRecvCb)(uint8_t* mac_addr, uint8_t* data, uint8_t len, uint8_t rssi);
    /**
     * This function is called whenever an ESP NOW packet is sent.
     * It is just a status callback whether or not the packet was actually sent.
     * This will be called after calling espNowSend()
     *
     * @param mac_addr The MAC address which the data was sent to
     * @param status   The status of the transmission
     */
    void (*fnEspNowSendCb)(uint8_t* mac_addr, mt_tx_status status);
    /**
     * This function is called periodically with the current acceleration
     * vector.
     *
     * @param accel A struct with 10 bit signed X, Y, and Z accel vectors
     */
    void (*fnAccelerometerCallback)(accel_t* accel);
} swadgeMode;

/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * Set the state of the six RGB LEDs, but don't overwrite if the LEDs were
 * set via UDP for at least TICKER_TIMEOUT increments of 100ms
 *
 * @param ledData    Array of LED color data. Every three bytes corresponds to
 *                   one LED in RGB order. So index 0 is LED1_R, index 1 is
 *                   LED1_G, index 2 is LED1_B, index 3 is LED2_R, etc.
 * @param ledDataLen The length of buffer, most likely 6*3
 */
void ICACHE_FLASH_ATTR setLeds(led_t* ledData, uint16_t ledDataLen);

/**
 * Wrapper for esp_now_send() which always broadcasts packets and sets wifi power
 *
 * @param data The data to be broadcast
 * @param len  The length of the data to broadcast
 */
void ICACHE_FLASH_ATTR espNowSend(const uint8_t* data, uint8_t len);

void ICACHE_FLASH_ATTR showLedCount(uint8_t num, uint32_t color);
uint32_t ICACHE_FLASH_ATTR getLedColorPerNumber(uint8_t num, uint8_t lightness);

void ICACHE_FLASH_ATTR swadgeModeButtonCallback(uint8_t state, int button, int down);
void ICACHE_FLASH_ATTR swadgeModeEspNowRecvCb(uint8_t* mac_addr, uint8_t* data, uint8_t len, uint8_t rssi);
void ICACHE_FLASH_ATTR swadgeModeEspNowSendCb(uint8_t* mac_addr, mt_tx_status status);

void EnterCritical(void);
void ExitCritical(void);

uint8_t ICACHE_FLASH_ATTR getSwadgeModes(swadgeMode***  modePtr);
void ICACHE_FLASH_ATTR switchToSwadgeMode(uint8_t newMode);

#endif /* USER_USER_MAIN_H_ */
