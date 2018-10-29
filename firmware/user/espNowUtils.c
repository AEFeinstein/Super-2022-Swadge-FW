/*
 * espNowUtils.c
 *
 *  Created on: Oct 29, 2018
 *      Author: adam
 */

#include <c_types.h>
#include "user_interface.h"
#include "espNowUtils.h"
#include "uart.h"
#include <espnow.h>
#include "user_main.h"
#include <osapi.h>
#include "commonservices.h"

uint8_t espNowBroadcastMac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

/**
 * Initialize ESP-NOW and attach callback functions
 */
void ICACHE_FLASH_ATTR espNowInit(void)
{
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
 * This callback function is called whenever an ESP-NOW
 *
 * @param mac_addr
 * @param data
 * @param len
 */
void ICACHE_FLASH_ATTR espNowRecvCb(uint8_t* mac_addr, uint8_t* data, uint8_t len)
{
    // Debug print the received payload
    char dbg[256] = {0};
    char tmp[8] = {0};
    int i;
    for (i = 0; i < len; i++)
    {
        ets_sprintf(tmp, "%02X ", data[i]);
        strcat(dbg, tmp);
    }

    os_printf("RECV MAC %02X:%02X:%02X:%02X:%02X:%02X\r\nbytes: %s\r\n",
              mac_addr[0],
              mac_addr[1],
              mac_addr[2],
              mac_addr[3],
              mac_addr[4],
              mac_addr[5],
              dbg);

    // Buried in a header, goes from 1 (far away) to 91 (practically touching)
    uint8_t rssi = data[-51];
    os_printf("nRSSI: %d \r\n", rssi);

    if(swadgeModeInit && NULL != swadgeModes[rtcMem.currentSwadgeMode]->fnEspNowRecvCb)
    {
        swadgeModes[rtcMem.currentSwadgeMode]->fnEspNowRecvCb(mac_addr, data, len, rssi);
    }
}

/**
 * TODO doc
 *
 * @param data
 * @param len
 */
void ICACHE_FLASH_ATTR espNowSend(uint8_t* data, uint8_t len)
{
    // Call this before each transmission to set the wifi speed
    wifi_set_user_fixed_rate(FIXED_RATE_MASK_ALL, PHY_RATE_54);
    // Send a test packet
    esp_now_send(espNowBroadcastMac, data, len);
}

/**
 * TODO doc
 *
 * @param mac_addr
 * @param status
 */
void ICACHE_FLASH_ATTR espNowSendCb(uint8_t* mac_addr, uint8_t status)
{
    os_printf("SEND MAC %02X:%02X:%02X:%02X:%02X:%02X\r\n",
              mac_addr[0],
              mac_addr[1],
              mac_addr[2],
              mac_addr[3],
              mac_addr[4],
              mac_addr[5]);

    switch((mt_tx_status)status)
    {
        case MT_TX_STATUS_OK:
        {
            os_printf("ESP NOW MT_TX_STATUS_OK\r\n");
            break;
        }
        case MT_TX_STATUS_FAILED:
        {
            os_printf("ESP NOW MT_TX_STATUS_FAILED\r\n");
            break;
        }
    }

    if(swadgeModeInit && NULL != swadgeModes[rtcMem.currentSwadgeMode]->fnEspNowSendCb)
    {
        swadgeModes[rtcMem.currentSwadgeMode]->fnEspNowSendCb(mac_addr, (mt_tx_status)status);
    }
}

/**
 * TODO doc
 */
void ICACHE_FLASH_ATTR espNowDeinit(void)
{
    esp_now_unregister_recv_cb();
    esp_now_unregister_send_cb();
    esp_now_deinit();
}
