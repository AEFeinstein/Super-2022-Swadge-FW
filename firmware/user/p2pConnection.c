/*============================================================================
 * Includes
 *==========================================================================*/

#include <osapi.h>
#include <user_interface.h>
#include <mem.h>

#include "user_main.h"
#include "p2pConnection.h"

/*============================================================================
 * Defines
 *==========================================================================*/

#define P2P_DEBUG_PRINT
#ifdef P2P_DEBUG_PRINT
    #define p2p_printf(...) os_printf(__VA_ARGS__)
#else
    #define p2p_printf(...)
#endif

// The time we'll spend retrying messages
#define RETRY_TIME_MS 3000

// Minimum RSSI to accept a connection broadcast
#define CONNECTION_RSSI 55

// Time to wait between connection events and game rounds.
// Transmission can be 3s (see above), the round @ 12ms period is 3.636s
// (240 steps of rotation + (252/4) steps of decay) * 12ms
#define FAILURE_RESTART_MS 8000

// Indices into messages to send
#define CMD_IDX 4
#define SEQ_IDX 8
#define MAC_IDX 11
// #define EXT_IDX 29

/*============================================================================
 * Variables
 *==========================================================================*/

// Messages to send.
const char p2pConnectionMsgFmt[] = "%s_con";
const char p2pNoPayloadMsgFmt[]  = "%s_%s_%02d_%02X:%02X:%02X:%02X:%02X:%02X";
const char p2pPayloadMsgFmt[]    = "%s_%s_%02d_%02X:%02X:%02X:%02X:%02X:%02X_%s";
const char p2pMacFmt[] = "%02X:%02X:%02X:%02X:%02X:%02X";

/*============================================================================
 * Function Prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR p2pConnectionTimeout(void* arg);
void ICACHE_FLASH_ATTR p2pTxAllRetriesTimeout(void* arg);
void ICACHE_FLASH_ATTR p2pTxRetryTimeout(void* arg);
void ICACHE_FLASH_ATTR p2pRestart(void* arg);
void ICACHE_FLASH_ATTR p2pStartRestartTimer(void* arg);
void ICACHE_FLASH_ATTR p2pProcConnectionEvt(p2pInfo* p2p, connectionEvt_t event);
void ICACHE_FLASH_ATTR p2pGameStartAckRecv(void* arg __attribute__((unused)));
void ICACHE_FLASH_ATTR p2pSendAckToMac(p2pInfo* p2p, uint8_t* mac_addr);
void ICACHE_FLASH_ATTR p2pSendMsgEx(p2pInfo* p2p, char* msg, uint16_t len,
                                    bool shouldAck, void (*success)(void*), void (*failure)(void*));

/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * Initialize everything and start sending broadcast messages
 */
void ICACHE_FLASH_ATTR p2pInitialize(p2pInfo* p2p, char* msgId)
{
    p2p_printf("%s\r\n", __func__);

    // Make sure everything is zero!
    ets_memset(p2p, 0, sizeof(p2pInfo));

    // Except the tracked sequence number, which starts at 255 so that a 0
    // received is valid.
    p2p->cnc.lastSeqNum = 255;

    ets_strncpy(p2p->msgId, msgId, sizeof(p2p->msgId));

    // Get and save the string form of our MAC address
    uint8_t mymac[6];
    wifi_get_macaddr(SOFTAP_IF, mymac);
    ets_snprintf(p2p->cnc.macStr, sizeof(p2p->cnc.macStr), p2pMacFmt,
                 mymac[0],
                 mymac[1],
                 mymac[2],
                 mymac[3],
                 mymac[4],
                 mymac[5]);

    // Set up the connection message
    ets_snprintf(p2p->conMsg, sizeof(p2p->conMsg), p2pConnectionMsgFmt,
                 p2p->msgId);

    // Set up dummy ACK message
    ets_snprintf(p2p->ackMsg, sizeof(p2p->ackMsg), p2pNoPayloadMsgFmt,
                 p2p->msgId,
                 "ack",
                 0,
                 0xFF,
                 0xFF,
                 0xFF,
                 0xFF,
                 0xFF,
                 0xFF);

    // Set up dummy start message
    ets_snprintf(p2p->startMsg, sizeof(p2p->startMsg), p2pNoPayloadMsgFmt,
                 p2p->msgId,
                 "str",
                 0,
                 0xFF,
                 0xFF,
                 0xFF,
                 0xFF,
                 0xFF,
                 0xFF);

    // Set up a timer for acking messages, don't start it
    os_timer_disarm(&p2p->tmr.TxRetry);
    os_timer_setfn(&p2p->tmr.TxRetry, p2pTxRetryTimeout, p2p);

    os_timer_disarm(&p2p->tmr.TxAllRetries);
    os_timer_setfn(&p2p->tmr.TxAllRetries, p2pTxAllRetriesTimeout, p2p);

    // Set up a timer to restart after failure. don't start it
    os_timer_disarm(&p2p->tmr.Reinit);
    os_timer_setfn(&p2p->tmr.Reinit, p2pRestart, p2p);

    // Set up a timer to do an initial connection, don't start it
    os_timer_disarm(&p2p->tmr.Connection);
    os_timer_setfn(&p2p->tmr.Connection, p2pConnectionTimeout, p2p);
}

/**
 * @brief TODO
 *
 * @param p2p
 */
void ICACHE_FLASH_ATTR p2pStartConnection(p2pInfo* p2p)
{
    p2p_printf("%s\r\n", __func__);

    os_timer_arm(&p2p->tmr.Connection, 1, false);
}

/**
 * Clean up all timers
 */
void ICACHE_FLASH_ATTR p2pDeinit(p2pInfo* p2p)
{
    p2p_printf("%s\r\n", __func__);

    os_timer_disarm(&p2p->tmr.Connection);
    os_timer_disarm(&p2p->tmr.TxRetry);
    os_timer_disarm(&p2p->tmr.Reinit);
    os_timer_disarm(&p2p->tmr.TxAllRetries);
}

/**
 * @brief TODO
 *
 * @param arg
 */
void ICACHE_FLASH_ATTR p2pConnectionTimeout(void* arg)
{
    p2p_printf("%s\r\n", __func__);

    p2pInfo* p2p = (p2pInfo*)arg;

    // Send a connection broadcast
    p2pSendMsgEx(p2p, p2p->conMsg, ets_strlen(p2p->conMsg), false, NULL, NULL);

    // os_random returns a 32 bit number, so this is [500ms,1500ms]
    uint32_t timeoutMs = 100 * (5 + (os_random() % 11));

    // Start the timer again
    p2p_printf("retry broadcast in %dms\r\n", timeoutMs);
    os_timer_arm(&p2p->tmr.Connection, timeoutMs, false);
}

/**
 * @brief TODO
 *
 * @param arg
 */
void ICACHE_FLASH_ATTR p2pTxRetryTimeout(void* arg)
{
    p2p_printf("%s\r\n", __func__);

    p2pInfo* p2p = (p2pInfo*)arg;

    if(p2p->ack.msgToAckLen > 0)
    {
        p2p_printf("Retrying message \"%s\"\r\n", p2p->ack.msgToAck);
        p2pSendMsgEx(p2p, p2p->ack.msgToAck, p2p->ack.msgToAckLen, true, p2p->ack.SuccessFn, p2p->ack.FailureFn);
    }
}

/**
 * @brief TODO
 *
 * @param arg
 */
void ICACHE_FLASH_ATTR p2pTxAllRetriesTimeout(void* arg)
{
    p2p_printf("%s\r\n", __func__);

    p2pInfo* p2p = (p2pInfo*)arg;

    // Disarm all timers
    os_timer_disarm(&p2p->tmr.TxRetry);
    os_timer_disarm(&p2p->tmr.TxAllRetries);

    // Call the failure function
    p2p_printf("Message totally failed \"%s\"\r\n", p2p->ack.msgToAck);
    if(NULL != p2p->ack.FailureFn)
    {
        p2p->ack.FailureFn(p2p);
    }

    // Clear out the ack variables
    ets_memset(&p2p->ack, 0, sizeof(p2p->ack));
}

/**
 * @brief
 *
 * @param p2p
 * @param msg
 * @param len
 */
void ICACHE_FLASH_ATTR p2pSendMsg(p2pInfo* p2p, char* msg, char* payload, uint16_t len)
{
    p2p_printf("%s\r\n", __func__);

    char builtMsg[64] = {0};

    if(NULL == payload || len == 0)
    {
        ets_snprintf(builtMsg, sizeof(builtMsg), p2pNoPayloadMsgFmt,
                     p2p->msgId,
                     msg,
                     0, // sequence number
                     p2p->cnc.otherMac[0],
                     p2p->cnc.otherMac[1],
                     p2p->cnc.otherMac[2],
                     p2p->cnc.otherMac[3],
                     p2p->cnc.otherMac[4],
                     p2p->cnc.otherMac[5]);
    }
    else
    {
        ets_snprintf(builtMsg, sizeof(builtMsg), p2pPayloadMsgFmt,
                     p2p->msgId,
                     msg,
                     0, // sequence number, filled in later
                     p2p->cnc.otherMac[0],
                     p2p->cnc.otherMac[1],
                     p2p->cnc.otherMac[2],
                     p2p->cnc.otherMac[3],
                     p2p->cnc.otherMac[4],
                     p2p->cnc.otherMac[5],
                     payload);
    }


    p2pSendMsgEx(p2p, builtMsg, strlen(builtMsg), true, p2pStartRestartTimer, p2pRestart);
}

/**
 * Wrapper for sending an ESP-NOW message. Handles ACKing and retries for
 * non-broadcast style messages
 *
 * @param msg       The message to send, may contain destination MAC
 * @param len       The length of the message to send
 * @param shouldAck true if this message should be acked, false if we don't care
 * @param success   A callback function if the message is acked. May be NULL
 * @param failure   A callback function if the message isn't acked. May be NULL
 */
void ICACHE_FLASH_ATTR p2pSendMsgEx(p2pInfo* p2p, char* msg, uint16_t len,
                                    bool shouldAck, void (*success)(void*), void (*failure)(void*))
{
    p2p_printf("%s\r\n", __func__);

    // If this is a first time message and longer than a connection message
    if( (p2p->ack.msgToAck != msg) && ets_strlen(p2p->conMsg) < len)
    {
        // Insert a sequence number
        msg[SEQ_IDX + 0] = '0' + (p2p->cnc.mySeqNum / 10);
        msg[SEQ_IDX + 1] = '0' + (p2p->cnc.mySeqNum % 10);

        // Increment the sequence number, 0-99
        p2p->cnc.mySeqNum++;
        if(100 == p2p->cnc.mySeqNum++)
        {
            p2p->cnc.mySeqNum = 0;
        }
    }

#ifdef P2P_DEBUG_PRINT
    char* dbgMsg = (char*)os_zalloc(sizeof(char) * (len + 1));
    ets_memcpy(dbgMsg, msg, len);
    p2p_printf("%s: %s\r\n", __func__, dbgMsg);
    os_free(dbgMsg);
#endif

    if(shouldAck)
    {
        // Set the state to wait for an ack
        p2p->ack.isWaitingForAck = true;

        // If this is not a retry
        if(p2p->ack.msgToAck != msg)
        {
            p2p_printf("sending for the first time\r\n");

            // Store the message for potential retries
            ets_memcpy(p2p->ack.msgToAck, msg, len);
            p2p->ack.msgToAckLen = len;
            p2p->ack.SuccessFn = success;
            p2p->ack.FailureFn = failure;

            // Start a timer to retry for 3s total
            os_timer_disarm(&p2p->tmr.TxAllRetries);
            os_timer_arm(&p2p->tmr.TxAllRetries, RETRY_TIME_MS, false);
        }
        else
        {
            p2p_printf("this is a retry\r\n");
        }

        // Mark the time this transmission started, the retry timer gets
        // started in refSendCb()
        p2p->ack.timeSentUs = system_get_time();
    }
    espNowSend((const uint8_t*)msg, len);
}

/**
 * This is called whenever an ESP NOW packet is received
 *
 * @param mac_addr The MAC of the swadge that sent the data
 * @param data     The data
 * @param len      The length of the data
 */
bool ICACHE_FLASH_ATTR p2pRecvMsg(p2pInfo* p2p, uint8_t* mac_addr, uint8_t* data, uint8_t len, uint8_t rssi)
{
#ifdef P2P_DEBUG_PRINT
    char* dbgMsg = (char*)os_zalloc(sizeof(char) * (len + 1));
    ets_memcpy(dbgMsg, data, len);
    p2p_printf("%s: %s\r\n", __func__, dbgMsg);
    os_free(dbgMsg);
#endif

    // Check if this is a "ref" message
    if(len < CMD_IDX ||
            (0 != ets_memcmp(data, p2p->conMsg, CMD_IDX)))
    {
        // This message is too short, or not a "ref" message
        p2p_printf("DISCARD: Not a ref message\r\n");
        return false;
    }

    // If this message has a MAC, check it
    if(len >= ets_strlen(p2p->ackMsg) &&
            0 != ets_memcmp(&data[MAC_IDX], p2p->cnc.macStr, ets_strlen(p2p->cnc.macStr)))
    {
        // This MAC isn't for us
        p2p_printf("DISCARD: Not for our MAC\r\n");
        return false;
    }

    // If this is anything besides a broadcast, check the other MAC
    if(p2p->cnc.otherMacReceived &&
            len > ets_strlen(p2p->conMsg) &&
            0 != ets_memcmp(mac_addr, p2p->cnc.otherMac, sizeof(p2p->cnc.otherMac)))
    {
        // This isn't from the other known swadge
        p2p_printf("DISCARD: Not from the other MAC\r\n");
        return false;
    }

    // By here, we know the received message was a "ref" message, either a
    // broadcast or for us. If this isn't an ack message, ack it
    if(len >= SEQ_IDX &&
            0 != ets_memcmp(data, p2p->ackMsg, SEQ_IDX))
    {
        p2pSendAckToMac(p2p, mac_addr);
    }

    // After ACKing the message, check the sequence number to see if we should
    // process it or ignore it (we already did!)
    if(len >= ets_strlen(p2p->ackMsg))
    {
        // Extract the sequence number
        uint8_t theirSeq = 0;
        theirSeq += (data[SEQ_IDX + 0] - '0') * 10;
        theirSeq += (data[SEQ_IDX + 1] - '0');

        // Check it against the last known sequence number
        if(theirSeq == p2p->cnc.lastSeqNum)
        {
            p2p_printf("DISCARD: Duplicate sequence number\r\n");
            return false;
        }
        else
        {
            p2p->cnc.lastSeqNum = theirSeq;
            p2p_printf("Store lastSeqNum %d\r\n", p2p->cnc.lastSeqNum);
        }
    }

    // ACKs can be received in any state
    if(p2p->ack.isWaitingForAck)
    {
        // Check if this is an ACK
        if(ets_strlen(p2p->ackMsg) == len &&
                0 == ets_memcmp(data, p2p->ackMsg, SEQ_IDX))
        {
            p2p_printf("ACK Received\r\n");

            // Call the function after receiving the ack
            if(NULL != p2p->ack.SuccessFn)
            {
                p2p->ack.SuccessFn(p2p);
            }

            // Clear ack timeout variables
            os_timer_disarm(&p2p->tmr.TxRetry);
            // Disarm the whole transmission ack timer
            os_timer_disarm(&p2p->tmr.TxAllRetries);
            // Clear out ACK variables
            ets_memset(&p2p->ack, 0, sizeof(p2p->ack));

            p2p->ack.isWaitingForAck = false;
        }
        // Don't process anything else when waiting for an ack
        return false;
    }

    if(false == p2p->isConnected)
    {
        // Received another broadcast, Check if this RSSI is strong enough
        if(!p2p->cnc.broadcastReceived &&
                rssi > CONNECTION_RSSI &&
                ets_strlen(p2p->conMsg) == len &&
                0 == ets_memcmp(data, p2p->conMsg, len))
        {
            p2p_printf("Broadcast Received, sending game start message\r\n");

            // We received a broadcast, don't allow another
            p2p->cnc.broadcastReceived = true;

            // Save the other ESP's MAC
            ets_memcpy(p2p->cnc.otherMac, mac_addr, sizeof(p2p->cnc.otherMac));
            p2p->cnc.otherMacReceived = true;

            // Send a message to that ESP to start the game.
            ets_snprintf(p2p->startMsg, sizeof(p2p->startMsg), p2pNoPayloadMsgFmt,
                         p2p->msgId,
                         "str",
                         0,
                         mac_addr[0],
                         mac_addr[1],
                         mac_addr[2],
                         mac_addr[3],
                         mac_addr[4],
                         mac_addr[5]);

            // If it's acked, call p2pGameStartAckRecv(), if not reinit with refInit()
            p2pSendMsgEx(p2p, p2p->startMsg, ets_strlen(p2p->startMsg), true, p2pGameStartAckRecv, p2pRestart);
        }
        // Received a response to our broadcast
        else if (!p2p->cnc.rxGameStartMsg &&
                 ets_strlen(p2p->startMsg) == len &&
                 0 == ets_memcmp(data, p2p->startMsg, SEQ_IDX))
        {
            p2p_printf("Game start message received, ACKing\r\n");

            // This is another swadge trying to start a game, which means
            // they received our p2p->conMsg. First disable our p2p->conMsg
            os_timer_disarm(&p2p->tmr.Connection);

            // And process this connection event
            p2pProcConnectionEvt(p2p, RX_GAME_START_MSG);
        }
        return false;
    }
    else
    {
        // Let the mode handle it
        return true;
    }
}

/**
 * Helper function to send an ACK message to the given MAC
 *
 * @param mac_addr The MAC to address this ACK to
 */
void ICACHE_FLASH_ATTR p2pSendAckToMac(p2pInfo* p2p, uint8_t* mac_addr)
{
    p2p_printf("%s\r\n", __func__);

    ets_snprintf(p2p->ackMsg, sizeof(p2p->ackMsg), p2pNoPayloadMsgFmt,
                 p2p->msgId,
                 "ack",
                 0,
                 mac_addr[0],
                 mac_addr[1],
                 mac_addr[2],
                 mac_addr[3],
                 mac_addr[4],
                 mac_addr[5]);
    p2pSendMsgEx(p2p, p2p->ackMsg, ets_strlen(p2p->ackMsg), false, NULL, NULL);
}

/**
 * This is called when p2p->startMsg is acked and processes the connection event
 *
 * @param arg unused
 */
void ICACHE_FLASH_ATTR p2pGameStartAckRecv(void* arg)
{
    p2p_printf("%s\r\n", __func__);

    p2pInfo* p2p = (p2pInfo*)arg;
    p2pProcConnectionEvt(p2p, RX_GAME_START_ACK);
}

/**
 * Two steps are necessary to establish a connection in no particular order.
 * 1. This swadge has to receive a start message from another swadge
 * 2. This swadge has to receive an ack to a start message sent to another swadge
 * The order of events determines who is the 'client' and who is the 'server'
 *
 * @param event The event that occurred
 */
void ICACHE_FLASH_ATTR p2pProcConnectionEvt(p2pInfo* p2p, connectionEvt_t event)
{
    p2p_printf("%s evt: %d, p2p->cnc.rxGameStartMsg %d, p2p->cnc.rxGameStartAck %d\r\n", __func__, event,
               p2p->cnc.rxGameStartMsg, p2p->cnc.rxGameStartAck);

    switch(event)
    {
        case RX_GAME_START_MSG:
        {
            // Already received the ack, become the client
            if(!p2p->cnc.rxGameStartMsg && p2p->cnc.rxGameStartAck)
            {
                p2p->cnc.playOrder = GOING_SECOND;
            }
            // Mark this event
            p2p->cnc.rxGameStartMsg = true;
            break;
        }
        case RX_GAME_START_ACK:
        {
            // Already received the msg, become the server
            if(!p2p->cnc.rxGameStartAck && p2p->cnc.rxGameStartMsg)
            {
                p2p->cnc.playOrder = GOING_FIRST;
            }
            // Mark this event
            p2p->cnc.rxGameStartAck = true;
            break;
        }
        default:
        {
            break;
        }
    }

    // If both the game start messages are good, start the game
    if(p2p->cnc.rxGameStartMsg && p2p->cnc.rxGameStartAck)
    {
        // Connection was successful, so disarm the failure timer
        os_timer_disarm(&p2p->tmr.Reinit);

        p2p->isConnected = true;

        // TODO tell the mode it's connected
    }
    else
    {
        // Start a timer to reinit if we never finish connection
        p2pStartRestartTimer(p2p);
    }
}

/**
 * This starts a timer to reinit everything, used in case of a failure
 *
 * @param arg unused
 */
void ICACHE_FLASH_ATTR p2pStartRestartTimer(void* arg)
{
    p2p_printf("%s\r\n", __func__);

    p2pInfo* p2p = (p2pInfo*)arg;
    // Give 5 seconds to get a result, or else restart
    os_timer_arm(&p2p->tmr.Reinit, FAILURE_RESTART_MS, false);
}

/**
 * Restart by deiniting then initing
 *
 * @param arg unused
 */
void ICACHE_FLASH_ATTR p2pRestart(void* arg)
{
    p2p_printf("%s\r\n", __func__);

    p2pInfo* p2p = (p2pInfo*)arg;
    char msgId[4] = {0};
    ets_strncpy(msgId, p2p->msgId, sizeof(msgId));
    p2pDeinit(p2p);
    p2pInitialize(p2p, msgId);
}

/**
 * This is called after an attempted transmission. If it was successful, and the
 * message should be acked, start a retry timer. If it wasn't successful, just
 * try again
 *
 * @param mac_addr
 * @param status
 */
void ICACHE_FLASH_ATTR p2pSendCb(p2pInfo* p2p, uint8_t* mac_addr __attribute__((unused)),
                                 mt_tx_status status)
{
    p2p_printf("%s\r\n", __func__);

    switch(status)
    {
        case MT_TX_STATUS_OK:
        {
            if(0 != p2p->ack.timeSentUs)
            {
                uint32_t transmissionTimeUs = system_get_time() - p2p->ack.timeSentUs;
                p2p_printf("Transmission time %dus\r\n", transmissionTimeUs);
                // The timers are all millisecond, so make sure that
                // transmissionTimeUs is at least 1ms
                if(transmissionTimeUs < 1000)
                {
                    transmissionTimeUs = 1000;
                }

                // Round it to the nearest Ms, add 69ms (the measured worst case)
                // then add some randomness [0ms to 15ms random]
                uint32_t waitTimeMs = ((transmissionTimeUs + 500) / 1000) + 69 + (os_random() & 0b1111);

                // Start the timer
                p2p_printf("ack timer set for %dms\r\n", waitTimeMs);
                os_timer_arm(&p2p->tmr.TxRetry, waitTimeMs, false);
            }
            break;
        }
        case MT_TX_STATUS_FAILED:
        {
            // If a message is stored
            if(p2p->ack.msgToAckLen > 0)
            {
                // try again in 1ms
                os_timer_arm(&p2p->tmr.TxRetry, 1, false);
            }
            break;
        }
        default:
        {
            break;
        }
    }
}

/**
 * @brief TODO
 *
 * @param p2p
 * @return playOrder_t p2pGetPlayOrder
 */
playOrder_t ICACHE_FLASH_ATTR p2pGetPlayOrder(p2pInfo* p2p)
{
    return p2p->cnc.playOrder;
}

/**
 * @brief TODO
 *
 * @param p2p
 */
void ICACHE_FLASH_ATTR p2pSetPlayOrder(p2pInfo* p2p, playOrder_t order)
{
    p2p->cnc.playOrder = order;
}