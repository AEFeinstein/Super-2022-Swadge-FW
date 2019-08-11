#ifndef _P2P_CONNECTION_H_
#define _P2P_CONNECTION_H_

#include <osapi.h>

// Indices into messages
#define CMD_IDX 4
#define SEQ_IDX 8
#define MAC_IDX 11
#define EXT_IDX 29

typedef enum
{
    NOT_SET,
    GOING_SECOND,
    GOING_FIRST
} playOrder_t;

typedef enum
{
    CON_STARTED,
    RX_GAME_START_ACK,
    RX_GAME_START_MSG,
    CON_ESTABLISHED,
    CON_LOST
} connectionEvt_t;

typedef void (*p2pConCallbackFn)(connectionEvt_t);

// Variables to track acking messages
typedef struct
{
    bool isConnected;
    char msgId[4];

    char conMsg[8];
    char ackMsg[32];
    char startMsg[32];

    p2pConCallbackFn conCallbackFn;

    struct
    {
        bool isWaitingForAck;
        char msgToAck[64];
        uint16_t msgToAckLen;
        uint32_t timeSentUs;
        void (*SuccessFn)(void*);
        void (*FailureFn)(void*);
    } ack;

    // Connection state variables
    struct
    {
        bool broadcastReceived;
        bool rxGameStartMsg;
        bool rxGameStartAck;
        playOrder_t playOrder;
        char macStr[18];
        uint8_t otherMac[6];
        bool otherMacReceived;
        uint8_t mySeqNum;
        uint8_t lastSeqNum;
    } cnc;

    struct
    {
        os_timer_t TxRetry;
        os_timer_t TxAllRetries;
        os_timer_t Connection;
        os_timer_t Reinit;
    } tmr;
} p2pInfo;


void ICACHE_FLASH_ATTR p2pInitialize(p2pInfo* p2p, char* msgId, p2pConCallbackFn conCallbackFn);
void ICACHE_FLASH_ATTR p2pDeinit(p2pInfo* p2p);

void ICACHE_FLASH_ATTR p2pStartConnection(p2pInfo* p2p);

void ICACHE_FLASH_ATTR p2pSendMsg(p2pInfo* p2p, char* msg, char* payload, uint16_t len);
void ICACHE_FLASH_ATTR p2pSendCb(p2pInfo* p2p, uint8_t* mac_addr, mt_tx_status status);
bool ICACHE_FLASH_ATTR p2pRecvMsg(p2pInfo* p2p, uint8_t* mac_addr, uint8_t* data, uint8_t len, uint8_t rssi);

playOrder_t ICACHE_FLASH_ATTR p2pGetPlayOrder(p2pInfo* p2p);
void ICACHE_FLASH_ATTR p2pSetPlayOrder(p2pInfo* p2p, playOrder_t order);

#endif