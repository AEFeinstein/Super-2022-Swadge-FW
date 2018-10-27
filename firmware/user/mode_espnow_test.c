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
 * Defines
 *==========================================================================*/

#define CHANNEL 3

/*============================================================================
 * Enums
 *==========================================================================*/

enum mt_tx_status
{
    MT_TX_STATUS_OK = 0,
    MT_TX_STATUS_FAILED,
};

/*============================================================================
 * Prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR espNowInit(void);
void ICACHE_FLASH_ATTR espNowDeinit(void);
void ICACHE_FLASH_ATTR espNowButton(uint8_t state, int button, int down);

/*============================================================================
 * Variables
 *==========================================================================*/

uint8_t broadcastMac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

swadgeMode espNowTestMode =
{
    .modeName = "espNowTestMode",
    .fnEnterMode = espNowInit,
    .fnExitMode = espNowDeinit,
    .fnTimerCallback = NULL,
    .fnButtonCallback = espNowButton,
    .fnAudioCallback = NULL,
    .wifiMode = ESP_NOW,
    .connectionColor = 0x00000000,
    .fnConnectionCallback = NULL,
    .fnPacketCallback = NULL,
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
void ICACHE_FLASH_ATTR recvCbTest(u8* mac_addr, u8* data, u8 len)
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

    printf("RECV MAC %02X:%02X:%02X:%02X:%02X:%02X\r\nbytes: %s\r\n",
           mac_addr[0],
           mac_addr[1],
           mac_addr[2],
           mac_addr[3],
           mac_addr[4],
           mac_addr[5],
           dbg);

    // Buried in a header, goes from 1 (far away) to 91 (practically touching)
    uint8_t rssi = data[-51];

    printf("nRSSI: %d \r\n", rssi);
}

/**
 * TODO doc
 *
 * @param mac_addr
 * @param status
 */
void ICACHE_FLASH_ATTR sendCbTest(u8* mac_addr, u8 status)
{
    printf("SEND MAC %02X:%02X:%02X:%02X:%02X:%02X\r\n",
           mac_addr[0],
           mac_addr[1],
           mac_addr[2],
           mac_addr[3],
           mac_addr[4],
           mac_addr[5]);

    switch(status)
    {
        case MT_TX_STATUS_OK:
        {
            printf("MT_TX_STATUS_OK\r\n");
            break;
        }
        case MT_TX_STATUS_FAILED:
        {
            printf("MT_TX_STATUS_FAILED\r\n");
            break;
        }
    }
}

/**
 * TODO doc
 */
void ICACHE_FLASH_ATTR espNowInit(void)
{
    if(0 == esp_now_init())
    {
        printf("ESP NOW init!\r\n");
        if(0 == esp_now_set_self_role(ESP_NOW_ROLE_COMBO))
        {
            printf("set as combo\r\n");
        }
        else
        {
            printf("esp now mode set fail\r\n");
        }

        if(0 == esp_now_register_recv_cb(recvCbTest))
        {
            printf("recvCb registered\r\n");
        }
        else
        {
            printf("recvCb NOT registered\r\n");
        }

        if(0 == esp_now_register_send_cb(sendCbTest))
        {
            printf("sendCb registered\r\n");
        }
        else
        {
            printf("sendCb NOT registered\r\n");
        }

        if(esp_now_is_peer_exist(broadcastMac))
        {
            printf("peer already exists\r\n");
        }
        else
        {
            if(0 == esp_now_add_peer(broadcastMac, ESP_NOW_ROLE_COMBO, CHANNEL, NULL, 0))
            {
                printf("added peer\r\n");
            }
            else
            {
                printf("DID NOT add peer\r\n");
            }
        }
    }
    else
    {
        printf("esp now fail\r\n");
    }
}

/**
 * TODO doc
 */
void ICACHE_FLASH_ATTR espNowDeinit(void)
{
    esp_now_del_peer(broadcastMac);
    esp_now_unregister_recv_cb();
    esp_now_unregister_send_cb();
    esp_now_deinit();
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
    if(1 == button && down)
    {
        printf("Sending message\r\n");

        // Call this before each transmission to set the wifi speed
        wifi_set_user_fixed_rate(FIXED_RATE_MASK_ALL, PHY_RATE_54);
        // Send a test packet
        char* testmsg = "Test Message";
        esp_now_send(broadcastMac, (uint8_t*)testmsg, ets_strlen(testmsg));
    }
}
