#ifndef _P2P_CONNECTION_H_
#define _P2P_CONNECTION_H_

#include <osapi.h>

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

typedef enum
{
    MSG_ACKED,
    MSG_FAILED
} messageStatus_t;

typedef struct _p2pInfo p2pInfo;

typedef void (*p2pConCbFn)(p2pInfo* p2p, connectionEvt_t);
typedef void (*p2pMsgRxCbFn)(p2pInfo* p2p, char* msg, uint8_t* payload, uint8_t len);
typedef void (*p2pMsgTxCbFn)(p2pInfo* p2p, messageStatus_t status);

// Variables to track acking messages
typedef struct _p2pInfo
{
    // Messages that every mode uses
    char msgId[4];
    char conMsg[8];
    char ackMsg[32];
    char startMsg[32];

    // Callback function pointers
    p2pConCbFn conCbFn;
    p2pMsgRxCbFn msgRxCbFn;
    p2pMsgTxCbFn msgTxCbFn;

    // Variables used for acking and retrying messages
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
        bool isConnected;
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

    // The timers used for connection and acking
    struct
    {
        os_timer_t TxRetry;
        os_timer_t TxAllRetries;
        os_timer_t Connection;
        os_timer_t Reinit;
    } tmr;
} p2pInfo;

void ICACHE_FLASH_ATTR p2pInitialize(p2pInfo* p2p, char* msgId,
                                     p2pConCbFn conCbFn,
                                     p2pMsgRxCbFn msgRxCbFn);
void ICACHE_FLASH_ATTR p2pDeinit(p2pInfo* p2p);

void ICACHE_FLASH_ATTR p2pStartConnection(p2pInfo* p2p);

void ICACHE_FLASH_ATTR p2pSendMsg(p2pInfo* p2p, char* msg, char* payload, uint16_t len, p2pMsgTxCbFn msgTxCbFn);
void ICACHE_FLASH_ATTR p2pSendCb(p2pInfo* p2p, uint8_t* mac_addr, mt_tx_status status);
void ICACHE_FLASH_ATTR p2pRecvCb(p2pInfo* p2p, uint8_t* mac_addr, uint8_t* data, uint8_t len, uint8_t rssi);

playOrder_t ICACHE_FLASH_ATTR p2pGetPlayOrder(p2pInfo* p2p);
void ICACHE_FLASH_ATTR p2pSetPlayOrder(p2pInfo* p2p, playOrder_t order);

#endif