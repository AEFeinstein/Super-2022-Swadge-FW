/*
 * mode_pass.c
 *
 *  Created on: Mar 27, 2019
 *      Author: adam
 */

/*============================================================================
 * Includes
 *==========================================================================*/

#include <osapi.h>
#include <stdlib.h>

#include "user_main.h"
#include "synced_timer.h"
#include "mode_swadgepass.h"

/*============================================================================
 * Defines
 *==========================================================================*/

/*============================================================================
 * Prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR passEnterMode(void);
void ICACHE_FLASH_ATTR passExitMode(void);
void ICACHE_FLASH_ATTR passEspNowSendCb(uint8_t* mac_addr, mt_tx_status status);
void ICACHE_FLASH_ATTR passEspNowRecvCb(uint8_t* mac_addr, uint8_t* data,
                                        uint8_t len, uint8_t rssi);
void ICACHE_FLASH_ATTR passSendMsg(void* arg);
void ICACHE_FLASH_ATTR passDeepSleep(void* arg);

/*============================================================================
 * Const data
 *==========================================================================*/

/*============================================================================
 * Variables
 *==========================================================================*/

swadgeMode passMode =
{
    .modeName = "pass",
    .fnEnterMode = passEnterMode,
    .fnExitMode = passExitMode,
    .fnButtonCallback = NULL,
    .wifiMode = SWADGE_PASS,
    .fnEspNowRecvCb = passEspNowRecvCb,
    .fnEspNowSendCb = passEspNowSendCb,
    .fnAccelerometerCallback = NULL
};

struct
{
    syncedTimer_t sendTimer;
    syncedTimer_t sleepTimer;
} pass;

/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * Initializer for pass
 */
void ICACHE_FLASH_ATTR passEnterMode(void)
{
    // Clear everything
    ets_memset(&pass, 0, sizeof(pass));

    syncedTimerDisarm(&pass.sleepTimer);
    syncedTimerSetFn(&pass.sleepTimer, passDeepSleep, NULL);

    syncedTimerDisarm(&pass.sendTimer);
    syncedTimerSetFn(&pass.sendTimer, passSendMsg, NULL);
    syncedTimerArm(&pass.sendTimer, 1, false);
}

/**
 * Called when pass is exited
 */
void ICACHE_FLASH_ATTR passExitMode(void)
{
    syncedTimerDisarm(&pass.sleepTimer);
}

/**
 * Callback function when ESP-NOW receives a packet. Forward everything to all
 * p2p connections and let them handle it
 *
 * @param mac_addr The MAC of the swadge that sent the data
 * @param data     The data
 * @param len      The length of the data
 * @param rssi     The RSSI of th received message, a proxy for distance
 */
void ICACHE_FLASH_ATTR passEspNowRecvCb(uint8_t* mac_addr, uint8_t* data,
                                        uint8_t len, uint8_t rssi)
{
    syncedTimerDisarm(&pass.sleepTimer);
}

/**
 * Callback function when ESP-NOW sends a packet. Forward everything to all p2p
 * connections and let them handle it
 *
 * @param mac_addr unused
 * @param status   Whether the transmission succeeded or failed
 */
void ICACHE_FLASH_ATTR passEspNowSendCb(uint8_t* mac_addr, mt_tx_status status)
{
    switch(status)
    {
        case MT_TX_STATUS_OK:
        {
            syncedTimerArm(&pass.sleepTimer, 100, false);
            break;
        }
        default:
        case MT_TX_STATUS_FAILED:
        {
            passDeepSleep(NULL);
            break;
        }
    }
}

/**
 * @brief Send a message
 *
 * @param arg
 */
void ICACHE_FLASH_ATTR passSendMsg(void* arg)
{
    char testData[] = "test message";
    espNowSend((const uint8_t*)testData, strlen(testData));
}

/**
 * @brief Goes to sleep
 *
 * @param arg
 */
void ICACHE_FLASH_ATTR passDeepSleep(void* arg)
{
    enterDeepSleep(SWADGE_PASS, 5000000);
}
