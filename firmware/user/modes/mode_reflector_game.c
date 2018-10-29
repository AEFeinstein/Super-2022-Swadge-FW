/*
 * mode_espnow_test.c
 *
 *  Created on: Oct 27, 2018
 *      Author: adam
 */

/*============================================================================
 * Includes
 *==========================================================================*/

#include "user_main.h"
#include "mode_reflector_game.h"
#include "osapi.h"

/*============================================================================
 * Prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR reflectorButton(uint8_t state, int button, int down);
void ICACHE_FLASH_ATTR reflectorSendCb(uint8_t* mac_addr, mt_tx_status status);
void ICACHE_FLASH_ATTR reflectorRecvCb(uint8_t* mac_addr, uint8_t* data, uint8_t len, uint8_t rssi);

/*============================================================================
 * Variables
 *==========================================================================*/

swadgeMode reflectorGameMode =
{
    .modeName = "reflector_game",
    .fnEnterMode = NULL,
    .fnExitMode = NULL,
    .fnTimerCallback = NULL,
    .fnButtonCallback = reflectorButton,
    .fnAudioCallback = NULL,
    .wifiMode = ESP_NOW,
    .fnEspNowRecvCb = reflectorRecvCb,
    .fnEspNowSendCb = reflectorSendCb,
};

/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * TODO doc
 *
 * @param mac_addr
 * @param data
 * @param len
 */
void ICACHE_FLASH_ATTR reflectorRecvCb(uint8_t* mac_addr, uint8_t* data, uint8_t len, uint8_t rssi)
{
    // Debug print the received payload
    os_printf("message received\r\n");
}

/**
 * TODO doc
 *
 * @param mac_addr
 * @param status
 */
void ICACHE_FLASH_ATTR reflectorSendCb(uint8_t* mac_addr, mt_tx_status status)
{
    // Debug print the received payload
    os_printf("message sent\r\n");
}

/**
 * TODO doc
 *
 * @param state
 * @param button
 * @param down
 */
void ICACHE_FLASH_ATTR reflectorButton(uint8_t state, int button, int down)
{
    if(2 == button && down)
    {
        os_printf("Sending message\r\n");
        // Send a test packet
        char* testmsg = "Test Message";
        espNowSend((uint8_t*)testmsg, ets_strlen(testmsg));
    }
}
