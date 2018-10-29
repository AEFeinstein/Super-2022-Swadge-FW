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
#include "mode_espnow_test.h"
#include "espnow.h"
#include "osapi.h"
#include "uart.h"
#include "commonservices.h"

/*============================================================================
 * Prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR espNowButton(uint8_t state, int button, int down);
void ICACHE_FLASH_ATTR sendCbTest(uint8_t* mac_addr, mt_tx_status status);
void ICACHE_FLASH_ATTR recvCbTest(uint8_t* mac_addr, uint8_t* data, uint8_t len, uint8_t rssi);

/*============================================================================
 * Variables
 *==========================================================================*/

swadgeMode espNowTestMode =
{
    .modeName = "espNowTestMode",
    .fnEnterMode = NULL,
    .fnExitMode = NULL,
    .fnTimerCallback = NULL,
    .fnButtonCallback = espNowButton,
    .fnAudioCallback = NULL,
    .wifiMode = ESP_NOW,
    .fnEspNowRecvCb = recvCbTest,
    .fnEspNowSendCb = sendCbTest,
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
void ICACHE_FLASH_ATTR recvCbTest(uint8_t* mac_addr, uint8_t* data, uint8_t len, uint8_t rssi)
{
    // Debug print the received payload
    printf("message received\r\n");
}

/**
 * TODO doc
 *
 * @param mac_addr
 * @param status
 */
void ICACHE_FLASH_ATTR sendCbTest(uint8_t* mac_addr, mt_tx_status status)
{
    // Debug print the received payload
    printf("message sent\r\n");
}

/**
 * TODO doc
 *
 * @param state
 * @param button
 * @param down
 */
void ICACHE_FLASH_ATTR espNowButton(uint8_t state, int button, int down)
{
    if(2 == button && down)
    {
        printf("Sending message\r\n");
        // Send a test packet
        char* testmsg = "Test Message";
        espNowSend((uint8_t*)testmsg, ets_strlen(testmsg));
    }
}
