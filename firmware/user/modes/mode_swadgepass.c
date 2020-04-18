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
#include "printControl.h"
#include "p2pConnection.h"

#ifdef SWADGEPASS_DBG
    #include "driver/uart.h"
#endif

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
void ICACHE_FLASH_ATTR passDeepSleep(void* arg);

void ICACHE_FLASH_ATTR passMsgRxCbFn(p2pInfo* p2p, char* msg,
                                     uint8_t* payload __attribute__((unused)),
                                     uint8_t len);
void ICACHE_FLASH_ATTR passConCbFn(p2pInfo* p2p, connectionEvt_t evt);
void ICACHE_FLASH_ATTR passMsgTxCbFn(p2pInfo* p2p, messageStatus_t status);
void ICACHE_FLASH_ATTR checkSleepAfterTransmission(void);

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
    p2pInfo p2p;
    syncedTimer_t sleepTimer;
    bool ourDataSent;
    bool theirDataReceived;
} pass;

const char dataMsg[] = "dat";

/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * Initializer for pass
 */
void ICACHE_FLASH_ATTR passEnterMode(void)
{
#ifdef SWADGEPASS_DBG
    uart_tx_one_char_no_wait(UART0, 's');
#endif
    // Clear everything
    ets_memset(&pass, 0, sizeof(pass));

    syncedTimerDisarm(&pass.sleepTimer);
    syncedTimerSetFn(&pass.sleepTimer, passDeepSleep, NULL);
    syncedTimerArm(&pass.sleepTimer, 228, false);

    p2pInitialize(&pass.p2p, "swp", passConCbFn, passMsgRxCbFn, 0);
    p2pStartConnection(&pass.p2p);
}

/**
 * Called when pass is exited
 */
void ICACHE_FLASH_ATTR passExitMode(void)
{
    syncedTimerDisarm(&pass.sleepTimer);
    p2pDeinit(&pass.p2p);
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
    p2pRecvCb(&pass.p2p, mac_addr, data, len, rssi);
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
    p2pSendCb(&pass.p2p, mac_addr, status);
}

/**
 * @brief Goes to sleep
 *
 * @param arg
 */
void ICACHE_FLASH_ATTR passDeepSleep(void* arg __attribute__((unused)))
{
#ifdef SWADGEPASS_DBG
    uart_tx_one_char_no_wait(UART0, 'z');
#endif
    enterDeepSleep(SWADGE_PASS, 2509000 + (os_random() % 2227000));
}

/**
 * Callback function when p2p connection events occur. Whenever a connection
 * starts, halt all the other p2ps from connecting.
 *
 * @param p2p The p2p struct which emitted a connection event
 * @param evt The connection event
 */
void ICACHE_FLASH_ATTR passConCbFn(p2pInfo* p2p __attribute__((unused)),
                                   connectionEvt_t evt)
{
    switch(evt)
    {
        case CON_STARTED:
        {
            // Do nothing
            break;
        }
        case RX_BROADCAST:
        case RX_GAME_START_ACK:
        case RX_GAME_START_MSG:
        {
            // Connection started, disarm the sleep timer
            syncedTimerDisarm(&pass.sleepTimer);
            break;
        }
        case CON_ESTABLISHED:
        {
            // Connection established, transfer some data
            switch(p2pGetPlayOrder(&(pass.p2p)))
            {
                // TODO set some sleep timer?
                case GOING_FIRST:
                {
                    char testMsg[] = "My Name is Earl";
                    p2pSendMsg(&(pass.p2p), (char*)dataMsg, testMsg, sizeof(testMsg), passMsgTxCbFn);
#ifdef SWADGEPASS_DBG
                    uart_tx_one_char_no_wait(UART0, 't');
#endif
                    os_printf("SENT %d bytes: \"%s\"\n", sizeof(testMsg), testMsg);
                    break;
                }
                case GOING_SECOND:
                {
                    // Wait to receive the other data first
                    break;
                }
                case NOT_SET:
                default:
                {
                    // This isn't right, just go to sleep
                    passDeepSleep(NULL);
                    break;
                }
            }
            break;
        }
        case CON_STOPPED:
        case CON_LOST:
        default:
        {
            // Connection stopped, go to sleep
            passDeepSleep(NULL);
            break;
        }
    }
    return;
}

/**
 * Callback function when p2p receives a message. Draw a little animation if
 * the message is correct
 *
 * @param p2p     The p2p struct which received a message
 * @param msg     The label for the received message
 * @param payload The payload for the received message
 * @param len     The length of the payload
 */
void ICACHE_FLASH_ATTR passMsgRxCbFn(p2pInfo* p2p __attribute__((unused)),
                                     char* msg, uint8_t* payload, uint8_t len)
{
    // TODO receive data
    if(0 == memcmp(msg, dataMsg, strlen(dataMsg)))
    {
        pass.theirDataReceived = true;

#ifdef SWADGEPASS_DBG
        uart_tx_one_char_no_wait(UART0, 'r');
#endif
        os_printf("RECEIVED %d bytes: \"%s\"\n", len, payload);

        // If we're going second, respond to this data
        if(GOING_SECOND == p2pGetPlayOrder(&pass.p2p))
        {
            char testMsg[] = "My Name is Jim";
            p2pSendMsg(&(pass.p2p), (char*)dataMsg, testMsg, sizeof(testMsg), passMsgTxCbFn);
#ifdef SWADGEPASS_DBG
            uart_tx_one_char_no_wait(UART0, 't');
#endif
            os_printf("SENT %d bytes: \"%s\"\n", sizeof(testMsg), testMsg);
        }

        checkSleepAfterTransmission();
    }
}

/**
 * @brief TODO
 *
 * @param p2p
 * @param status
 */
void ICACHE_FLASH_ATTR passMsgTxCbFn(p2pInfo* p2p __attribute__((unused)),
                                     messageStatus_t status)
{
    switch(status)
    {
        case MSG_ACKED:
        {
            pass.ourDataSent = true;
            checkSleepAfterTransmission();
            break;
        }
        default:
        case MSG_FAILED:
        {
            // Message failed, just go to sleep
            passDeepSleep(NULL);
        }
    }
}

/**
 * @brief TODO
 *
 */
void ICACHE_FLASH_ATTR checkSleepAfterTransmission(void)
{
    if(true == pass.ourDataSent && true == pass.theirDataReceived)
    {
#ifdef SWADGEPASS_DBG
        uart_tx_one_char_no_wait(UART0, 'd');
#endif
        os_printf("ALL DONE!\n");
        // sleep in one second, giving other swadge time to finish
        syncedTimerArm(&pass.sleepTimer, 10, false);
    }
}