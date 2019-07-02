/*
 * espNowUtils.c
 *
 *  Created on: Oct 29, 2018
 *      Author: adam
 */

/*============================================================================
 * Includes
 *==========================================================================*/

#include <c_types.h>
#include <user_interface.h>
#include <uart.h>
#include <espnow.h>
#include <osapi.h>

#include "espNowUtils.h"
#include "user_main.h"

/*============================================================================
 * Variables
 *==========================================================================*/

/// This is the MAC address to transmit to for broadcasting
const uint8_t espNowBroadcastMac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

/*============================================================================
 * Prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR espNowRecvCb(uint8_t* mac_addr, uint8_t* data, uint8_t len);
void ICACHE_FLASH_ATTR espNowSendCb(uint8_t* mac_addr, uint8_t status);

/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * Initialize ESP-NOW and attach callback functions
 */
void ICACHE_FLASH_ATTR espNowInit(void)
{
    // Set up all the wifi softAP mode configs
    if(false == wifi_set_opmode_current(SOFTAP_MODE))
    {
        os_printf("Could not set as station mode\r\n");
        return;
    }

    struct softap_config config =
    {
        .ssid = {0},
        .password = {0},
        .ssid_len = 0,
        .channel = SOFTAP_CHANNEL,
        .authmode = AUTH_OPEN,
        .ssid_hidden = true,
        .max_connection = 0,
        .beacon_interval = 60000,
    };
    if(false == wifi_softap_set_config_current(&config))
    {
        os_printf("Couldn't set softap config\r\n");
        return;
    }

    if(false == wifi_softap_dhcps_stop())
    {
        os_printf("Couldn't stop dhcp\r\n");
        return;
    }

    if(false == wifi_set_phy_mode(PHY_MODE_11G))
    {
        os_printf("Couldn't set phy mode\r\n");
        return;
    }

    wifi_country_t wc =
    {
        .cc = "USA",
        .schan = 1,
        .nchan = 11,
        .policy = WIFI_COUNTRY_POLICY_AUTO
    };
    if(false == wifi_set_country(&wc))
    {
        os_printf("Couldn't set country info\r\n");
        return;
    }

    // Set the channel
    if(false == wifi_set_channel( SOFTAP_CHANNEL ))
    {
        os_printf("Couldn't set channel\r\n");
        return;
    }

    if(0 == esp_now_init())
    {
        os_printf("ESP NOW init!\r\n");
        if(0 == esp_now_set_self_role(ESP_NOW_ROLE_COMBO))
        {
            os_printf("set as combo\r\n");
        }
        else
        {
            os_printf("esp now mode set fail\r\n");
        }

        if(0 == esp_now_register_recv_cb(espNowRecvCb))
        {
            os_printf("recvCb registered\r\n");
        }
        else
        {
            os_printf("recvCb NOT registered\r\n");
        }

        if(0 == esp_now_register_send_cb(espNowSendCb))
        {
            os_printf("sendCb registered\r\n");
        }
        else
        {
            os_printf("sendCb NOT registered\r\n");
        }
    }
    else
    {
        os_printf("esp now fail\r\n");
    }
}

/**
 * This callback function is called whenever an ESP-NOW packet is received
 *
 * @param mac_addr The MAC address of the sender
 * @param data     The data which was received
 * @param len      The length of the data which was received
 */
void ICACHE_FLASH_ATTR espNowRecvCb(uint8_t* mac_addr, uint8_t* data, uint8_t len)
{
    // Buried in a header, goes from 1 (far away) to 91 (practically touching)
    uint8_t rssi = data[-51];

#ifdef EXTRA_DEBUG
    // Debug print the received payload
    char dbg[256] = {0};
    char tmp[8] = {0};
    int i;
    for (i = 0; i < len; i++)
    {
        ets_sprintf(tmp, "%02X ", data[i]);
        strcat(dbg, tmp);
    }
    os_printf("%s, MAC [%02X:%02X:%02X:%02X:%02X:%02X], RSSI [%d], Bytes [%s]\r\n",
              __func__,
              mac_addr[0],
              mac_addr[1],
              mac_addr[2],
              mac_addr[3],
              mac_addr[4],
              mac_addr[5],
              rssi,
              dbg);
#endif

    swadgeModeEspNowRecvCb(mac_addr, data, len, rssi);
}

/**
 * This is a wrapper for esp_now_send. It also sets the wifi power with
 * wifi_set_user_fixed_rate()
 *
 * @param data The data to broadcast using ESP NOW
 * @param len  The length of the data to broadcast
 */
void ICACHE_FLASH_ATTR espNowSend(const uint8_t* data, uint8_t len)
{
    // Call this before each transmission to set the wifi speed
    wifi_set_user_fixed_rate(FIXED_RATE_MASK_ALL, PHY_RATE_54);

    // Send a packet
    esp_now_send((uint8_t*)espNowBroadcastMac, (uint8_t*)data, len);
}

/**
 * This callback function is registered to be called after an ESP NOW
 * transmission occurs. It notifies the program if the transmission
 * was successful or not. It gives no information about if the transmission
 * was received
 *
 * @param mac_addr The MAC address which was transmitted to
 * @param status   MT_TX_STATUS_OK or MT_TX_STATUS_FAILED
 */
void ICACHE_FLASH_ATTR espNowSendCb(uint8_t* mac_addr, uint8_t status)
{
#ifdef EXTRA_DEBUG
    os_printf("SEND MAC %02X:%02X:%02X:%02X:%02X:%02X\r\n",
              mac_addr[0],
              mac_addr[1],
              mac_addr[2],
              mac_addr[3],
              mac_addr[4],
              mac_addr[5]);
#endif

    switch((mt_tx_status)status)
    {
        case MT_TX_STATUS_OK:
        {
            // os_printf("ESP NOW MT_TX_STATUS_OK\r\n");
            break;
        }
        case MT_TX_STATUS_FAILED:
        {
            os_printf("ESP NOW MT_TX_STATUS_FAILED\r\n");
            break;
        }
        default:
        {
            os_printf("ESP UNKNOWN STATUS %d\r\n", (mt_tx_status)status);
            break;
        }
    }

    swadgeModeEspNowSendCb(mac_addr, (mt_tx_status)status);
}

/**
 * This function is automatically called to de-initialize ESP-NOW
 */
void ICACHE_FLASH_ATTR espNowDeinit(void)
{
    esp_now_unregister_recv_cb();
    esp_now_unregister_send_cb();
    esp_now_deinit();
}
