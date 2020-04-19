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
#include <user_interface.h>

#include "user_main.h"
#include "synced_timer.h"
#include "mode_swadgepass.h"
#include "printControl.h"

#ifdef SWADGEPASS_DBG
    #include "driver/uart.h"
#endif

/*============================================================================
 * Defines
 *==========================================================================*/

/* There are three configurable times for SwadgePass, the time the Swadge stays
 * awake after transmission, the minimum time the Swadge is asleep after
 * transmission, and random interval the Swadge will continue to sleep after
 * transmission.
 *
 * The numbers picked below were based on Monte Carlo simulations to find the
 * maximum sleep time while achieving one packet per minute
 */

// ~1.42 packets per minute, expected 1.72ppm, 9.178% on
// #define TIME_ON_MS         228
// #define MIN_TIME_SLEEP_US 2509000
// #define RND_TIME_SLEEP_US 2227000

// ~1.05ppm, expected 1.23ppm, 7.714% on
#define TIME_ON_MS         217
#define MIN_TIME_SLEEP_US 2361000
#define RND_TIME_SLEEP_US 3797000

// The number of stored MAC addresses
#define MAC_ENTRIES 10
// The length of a MAC address
#define MAC_LEN      6

/*============================================================================
 * Structs
 *==========================================================================*/

typedef struct
{
    uint8_t macAddr[MAC_LEN]; ///< A MAC Address
    uint32_t tAdded;          ///< The time this MAC address was added
} rememberedMac_t;

/*============================================================================
 * Prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR passEnterMode(void);
void ICACHE_FLASH_ATTR passExitMode(void);
void ICACHE_FLASH_ATTR passEspNowSendCb(uint8_t* mac_addr, mt_tx_status status);
void ICACHE_FLASH_ATTR passEspNowRecvCb(uint8_t* mac_addr, uint8_t* data,
                                        uint8_t len, uint8_t rssi);
void ICACHE_FLASH_ATTR passDeepSleep(void* arg);
void ICACHE_FLASH_ATTR passSendMsg(void* arg);
bool ICACHE_FLASH_ATTR passCheckMacAddr(uint8_t* mac_addr);

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
    syncedTimer_t sleepTimer;
    syncedTimer_t sendTimer;
    bool ourDataSent;
    bool theirDataReceived;
    rememberedMac_t macTbl[MAC_ENTRIES];
} pass;

/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * Initialize Swadgepass mode
 */
void ICACHE_FLASH_ATTR passEnterMode(void)
{
#ifdef SWADGEPASS_DBG
    uart_tx_one_char_no_wait(UART0, '#');
#endif
    // Clear everything
    ets_memset(&pass, 0, sizeof(pass));

    // Set a timer to go back to deep sleep in TIME_ON_MS
    syncedTimerDisarm(&pass.sleepTimer);
    syncedTimerSetFn(&pass.sleepTimer, passDeepSleep, NULL);
    syncedTimerArm(&pass.sleepTimer, TIME_ON_MS, false);

    // Start a timer to send a broadcast. If we try to broadcast during init,
    // it crashes
    syncedTimerDisarm(&pass.sendTimer);
    syncedTimerSetFn(&pass.sendTimer, passSendMsg, NULL);
    syncedTimerArm(&pass.sendTimer, 1, false);
}

/**
 * Clean up swadge pass mode by disarming timers
 */
void ICACHE_FLASH_ATTR passExitMode(void)
{
    syncedTimerDisarm(&pass.sleepTimer);
    syncedTimerDisarm(&pass.sendTimer);
}

/**
 * Callback function when ESP-NOW receives a packet. If it's from a unique MAC
 * address, process it and broadcast our information again. If it's from a
 * MAC address we've heard from before, just go to deep sleep
 *
 * TODO actually process data from other swadge
 *
 * @param mac_addr The MAC of the swadge that sent the data
 * @param data     The data
 * @param len      The length of the data
 * @param rssi     The RSSI of th received message, a proxy for distance
 */
void ICACHE_FLASH_ATTR passEspNowRecvCb(uint8_t* mac_addr,
                                        uint8_t* data  __attribute__((unused)),
                                        uint8_t len  __attribute__((unused)),
                                        uint8_t rssi __attribute__((unused)))
{
#ifdef SWADGEPASS_DBG
    uart_tx_one_char_no_wait(UART0, '<');
#endif

    // Received a message. Check if we've responded to this MAC yet and if
    // we haven't, send a response
    if(passCheckMacAddr(mac_addr))
    {
#ifdef SWADGEPASS_DBG
        uart_tx_one_char_no_wait(UART0, '^');
#endif
        passSendMsg(NULL);
    }
    else
    {
        // Received a response twice, time to sleep!
        passDeepSleep(NULL);
    }
}

/**
 * Check if the MAC address is new or known. Any MAC address checked is inserted
 * into the table of MAC addresses. If the table is full, it will overwrite the
 * oldest entry
 * 
 * TODO save in NVM? That requires RTC time tracking
 *
 * @param mac_addr the MAC address to check
 * @return true  if this was a new MAC address
 *         false if this was a duplicate mac address
 */
bool ICACHE_FLASH_ATTR passCheckMacAddr(uint8_t* mac_addr)
{
    // Keep track of the oldest entry, just in case
    uint32_t tAddedMin = 0xFFFFFFFF;
    uint8_t oldestIdx = 0;

    // Look through the table of MAC addresses
    for(uint8_t idx = 0; idx < MAC_ENTRIES; idx++)
    {
        // If there is a match
        if(0 == ets_memcmp(mac_addr, pass.macTbl[idx].macAddr, 6))
        {
            return false;
        }
        else if (pass.macTbl[idx].tAdded < tAddedMin)
        {
            // Note the new oldest index
            tAddedMin = pass.macTbl[idx].tAdded;
            oldestIdx = idx;
        }
    }

    // It wasn't in the table, so add it
    memcpy(pass.macTbl[oldestIdx].macAddr, mac_addr, 6);
    pass.macTbl[oldestIdx].tAdded = system_get_time();
    return true;
}

/**
 * Callback function when ESP-NOW sends a packet. If the transmission failed,
 * just go to deep sleep.
 *
 * @param mac_addr unused
 * @param status   Whether the transmission succeeded or failed
 */
void ICACHE_FLASH_ATTR passEspNowSendCb(
    uint8_t* mac_addr __attribute__((unused)),
    mt_tx_status status)
{
    switch(status)
    {
        case MT_TX_STATUS_OK:
        {
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
 * Helper function to send this swadge's information in a broadcast packet.
 * This may be called from a timer, and will arm a timer to be called again in
 * 50ms.
 *
 * TODO fill the message with our data
 *
 * @param arg unused
 */
void ICACHE_FLASH_ATTR passSendMsg(void* arg __attribute__((unused)))
{
#ifdef SWADGEPASS_DBG
    uart_tx_one_char_no_wait(UART0, '>');
#endif

    const char testMsg[] = "My Name is Earl";
    espNowSend((const uint8_t*)testMsg, sizeof(testMsg));

    // After sending the first msg, rearm to ping every 50ms
    syncedTimerDisarm(&pass.sendTimer);
    syncedTimerArm(&pass.sendTimer, 50, false);
}

/**
 * @brief Goes to sleep for a randomized interval
 *
 * @param arg unused
 */
void ICACHE_FLASH_ATTR passDeepSleep(void* arg __attribute__((unused)))
{
#ifdef SWADGEPASS_DBG
    uart_tx_one_char_no_wait(UART0, 'Z');
#endif

    enterDeepSleep(SWADGE_PASS, MIN_TIME_SLEEP_US +
                   (os_random() % RND_TIME_SLEEP_US));
}
