/*
 * mode_espnow_test.c
 *
 *  Created on: Oct 27, 2018
 *      Author: adam
 *
 * TODO add timers during the connection process! Think it all through
 * TODO add timers during the game too
 * TODO add LEDs
 */

/* PlantUML for the connection process:

    group connection part 1
    "Swadge_AB:AB:AB:AB:AB:AB"->"Swadge_12:12:12:12:12:12" : "ref_con" (broadcast)
    "Swadge_12:12:12:12:12:12"->"Swadge_AB:AB:AB:AB:AB:AB" : "ref_str_AB:AB:AB:AB:AB:AB"
    note left: Stop Broadcasting, set rxGameStartMsg
    "Swadge_AB:AB:AB:AB:AB:AB"->"Swadge_12:12:12:12:12:12" : "ref_ack_12:12:12:12:12:12"
    note right: set rxGameStartAck
    end

    group connection part 2
    "Swadge_12:12:12:12:12:12"->"Swadge_AB:AB:AB:AB:AB:AB" : "ref_con" (broadcast)
    "Swadge_AB:AB:AB:AB:AB:AB"->"Swadge_12:12:12:12:12:12" : "ref_str_12:12:12:12:12:12"
    note right: Stop Broadcasting, set rxGameStartMsg, become CLIENT
    "Swadge_12:12:12:12:12:12"->"Swadge_AB:AB:AB:AB:AB:AB" : "ref_ack_AB:AB:AB:AB:AB:AB"
    note left: set rxGameStartAck, become SERVER
    end

    loop until someone loses
    "Swadge_AB:AB:AB:AB:AB:AB"->"Swadge_AB:AB:AB:AB:AB:AB" : Play game
    "Swadge_AB:AB:AB:AB:AB:AB"->"Swadge_12:12:12:12:12:12" : Send game state message ((success or fail) and speed)
    "Swadge_12:12:12:12:12:12"->"Swadge_12:12:12:12:12:12" : Play game
    "Swadge_12:12:12:12:12:12"->"Swadge_AB:AB:AB:AB:AB:AB" : Send game state message ((success or fail) and speed)
    end
*/

/*============================================================================
 * Includes
 *==========================================================================*/

#include "user_main.h"
#include "mode_reflector_game.h"
#include "osapi.h"

/*============================================================================
 * Defines
 *==========================================================================*/

#define REFLECTOR_ACK_RETRIES 3
#define CONNECTION_RSSI 60

/*============================================================================
 * Enums
 *==========================================================================*/

typedef enum
{
    R_CONNECTING,
    R_PLAYING,
} reflectorGameState_t;

typedef enum
{
    CLIENT,
    SERVER
} csRole_t;

typedef enum
{
    RX_GAME_START_ACK,
    RX_GAME_START_MSG
} connectionEvt_t;

/*============================================================================
 * Prototypes
 *==========================================================================*/

// SwadgeMode Callbacks
void ICACHE_FLASH_ATTR refInit(void);
void ICACHE_FLASH_ATTR refDeinit(void);
void ICACHE_FLASH_ATTR refButton(uint8_t state, int button, int down);
void ICACHE_FLASH_ATTR refSendCb(uint8_t* mac_addr, mt_tx_status status);
void ICACHE_FLASH_ATTR refRecvCb(uint8_t* mac_addr, uint8_t* data, uint8_t len, uint8_t rssi);

// Transmission Functions
void ICACHE_FLASH_ATTR refSendMsg(const char* msg, uint16_t len, bool shouldAck, void (*success)(void),
                                  void (*failure)(void));
void ICACHE_FLASH_ATTR refSendAckToMac(uint8_t* mac_addr);
void ICACHE_FLASH_ATTR refTxRetryTimeout(void* arg);

// Connection functions
void ICACHE_FLASH_ATTR refConnectionTimeout(void* arg);
void ICACHE_FLASH_ATTR refGameStartAckRecv(void);
void ICACHE_FLASH_ATTR refProcConnectionEvt(connectionEvt_t event);

// Game functions
void ICACHE_FLASH_ATTR refStartPlaying(void);
void ICACHE_FLASH_ATTR refStartRound(void);

/*============================================================================
 * Variables
 *==========================================================================*/

swadgeMode reflectorGameMode =
{
    .modeName = "reflector_game",
    .fnEnterMode = refInit,
    .fnExitMode = refDeinit,
    .fnTimerCallback = NULL,
    .fnButtonCallback = refButton,
    .fnAudioCallback = NULL,
    .wifiMode = ESP_NOW,
    .fnEspNowRecvCb = refRecvCb,
    .fnEspNowSendCb = refSendCb,
};

reflectorGameState_t gameState = R_CONNECTING;

// Variables to track acking messages
bool isWaitingForAck = false;
char msgToAck[32] = {0};
uint16_t msgToAckLen = 0;
uint8_t refTxRetries = 0;
void (*ackSuccess)(void) = NULL;
void (*ackFailure)(void) = NULL;

// Connection state variables
bool broadcastReceived = false;
bool rxGameStartMsg = false;
bool rxGameStartAck = false;
csRole_t csRole = CLIENT;

// This swadge's MAC, in string form
char macStr[] = "00:00:00:00:00:00";
uint8_t otherMac[6] = {0};

// Messages to send.
#define CMD_IDX 4
#define MAC_IDX 8
char connectionMsg[] = "ref_con";
char ackMsg[]        = "ref_ack_00:00:00:00:00:00";
char gameStartMsg[]  = "ref_str_00:00:00:00:00:00";

// Timers
static os_timer_t initConnectionTimer = {0};
static os_timer_t refTxRetryTimer = {0};

/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * Initialize everything and start sending broadcast messages
 */
void ICACHE_FLASH_ATTR refInit(void)
{
    os_printf("%s\r\n", __func__);

    // Set the state
    gameState = R_CONNECTING;

    broadcastReceived = false;
    rxGameStartAck = false;
    rxGameStartMsg = false;
    csRole = CLIENT;

    // Clear the LEDs
    uint8_t blankLeds[18] = {0};
    setLeds(blankLeds, sizeof(blankLeds));

    uint8_t mymac[6];
    wifi_get_macaddr(SOFTAP_IF, mymac);
    ets_sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X",
                mymac[0],
                mymac[1],
                mymac[2],
                mymac[3],
                mymac[4],
                mymac[5]);

    // Set up a timer for acking messages, don't start it
    os_timer_disarm(&refTxRetryTimer);
    os_timer_setfn(&refTxRetryTimer, refTxRetryTimeout, NULL);

    // Start a timer to do an initial connection
    os_timer_disarm(&initConnectionTimer);
    os_timer_setfn(&initConnectionTimer, refConnectionTimeout, NULL);
    os_timer_arm(&initConnectionTimer, 1, false);
}

/**
 * Clean up all timers
 */
void ICACHE_FLASH_ATTR refDeinit(void)
{
    os_printf("%s\r\n", __func__);

    os_timer_disarm(&initConnectionTimer);
    os_timer_disarm(&refTxRetryTimer);
}

/**
 * This is called on the timer initConnectionTimer. It broadcasts the connectionMsg
 *
 * @param arg unused
 */
void ICACHE_FLASH_ATTR refConnectionTimeout(void* arg __attribute__((unused)) )
{
    // Send a connection broadcast
    refSendMsg(connectionMsg, ets_strlen(connectionMsg), false, NULL, NULL);

    // os_random returns a 32 bit number, so this is [500ms,1500ms]
    uint32_t timeoutMs = 100 * (5 + (os_random() % 11));

    // Start the timer again
    os_printf("retry broadcast in %dms\r\n", timeoutMs);
    os_timer_arm(&initConnectionTimer, timeoutMs, false);
}

/**
 * This is called whenever an ESP NOW packet is received
 *
 * @param mac_addr The MAC of the swadge that sent the data
 * @param data     The data
 * @param len      The length of the data
 */
void ICACHE_FLASH_ATTR refRecvCb(uint8_t* mac_addr, uint8_t* data, uint8_t len, uint8_t rssi)
{
    char* dbgMsg = (char*)os_zalloc(sizeof(char) * (len + 1));
    ets_memcpy(dbgMsg, data, len);
    os_printf("%s: %s\r\n", __func__, dbgMsg);
    os_free(dbgMsg);

    // ACKs can be received in any state
    if(isWaitingForAck)
    {
        // Check if this is an ACK from the other MAC addressed to us
        if(ets_strlen(ackMsg) == len &&
                0 == ets_memcmp(mac_addr, otherMac, sizeof(otherMac)) &&
                0 == ets_memcmp(data, ackMsg, MAC_IDX) &&
                0 == ets_memcmp(&data[MAC_IDX], macStr, ets_strlen(macStr)))
        {
            os_printf("ACK Received\r\n");

            // Call the function after receiving the ack
            if(NULL != ackSuccess)
            {
                ackSuccess();
            }

            // Clear ack timeout variables
            os_timer_disarm(&refTxRetryTimer);
            refTxRetries = 0;

            isWaitingForAck = false;
        }
        // Don't process anything else when waiting for an ack
        return;
    }

    switch(gameState)
    {
        case R_CONNECTING:
        {
            // Received another broadcast, Check if this RSSI is strong enough
            if(!broadcastReceived &&
                    rssi > CONNECTION_RSSI &&
                    ets_strlen(connectionMsg) == len &&
                    0 == ets_memcmp(data, connectionMsg, len))
            {
                os_printf("Broadcast Received, sending game start message\r\n");

                // We received a broadcast, don't allow another
                broadcastReceived = true;

                // Save the other ESP's MAC
                ets_memcpy(otherMac, mac_addr, sizeof(otherMac));

                // Send a message to that ESP to start the game.
                ets_sprintf(&gameStartMsg[MAC_IDX], "%02X:%02X:%02X:%02X:%02X:%02X",
                            mac_addr[0],
                            mac_addr[1],
                            mac_addr[2],
                            mac_addr[3],
                            mac_addr[4],
                            mac_addr[5]);

                // If it's acked, call refGameStartAckRecv(), if not reinit with refInit()
                refSendMsg(gameStartMsg, ets_strlen(gameStartMsg), true, refGameStartAckRecv, refInit);
            }
            // Received a response to our broadcast
            else if (!rxGameStartMsg &&
                     ets_strlen(gameStartMsg) == len &&
                     0 == ets_memcmp(data, gameStartMsg, MAC_IDX) &&
                     0 == ets_memcmp(&data[MAC_IDX], macStr, ets_strlen(macStr)))
            {
                os_printf("Game start message received, ACKing\r\n");

                // This is another swadge trying to start a game, which means
                // they received our connectionMsg. First disable our connectionMsg
                os_timer_disarm(&initConnectionTimer);

                // Then ACK their request to start a game
                refSendAckToMac(mac_addr);

                // And process this connection event
                refProcConnectionEvt(RX_GAME_START_MSG);
            }

            break;
        }
        case R_PLAYING:
        {
            // TODO a game
            break;
        }
    }
}

/**
 * Helper function to send an ACK message to the given MAC
 *
 * @param mac_addr The MAC to address this ACK to
 */
void ICACHE_FLASH_ATTR refSendAckToMac(uint8_t* mac_addr)
{
    os_printf("%s\r\n", __func__);

    ets_sprintf(&ackMsg[MAC_IDX], "%02X:%02X:%02X:%02X:%02X:%02X",
                mac_addr[0],
                mac_addr[1],
                mac_addr[2],
                mac_addr[3],
                mac_addr[4],
                mac_addr[5]);
    refSendMsg(ackMsg, ets_strlen(ackMsg), false, NULL, NULL);
}

/**
 * This is called when gameStartMsg is acked and processes the connection event
 */
void ICACHE_FLASH_ATTR refGameStartAckRecv(void)
{
    refProcConnectionEvt(RX_GAME_START_ACK);
}

/**
 * Two steps are necessary to establish a connection in no particular order.
 * 1. This swadge has to receive a start message from another swadge
 * 2. This swadge has to receive an ack to a start message sent to another swadge
 * The order of events determines who is the 'client' and who is the 'server'
 *
 * @param event The event that occurred
 */
void ICACHE_FLASH_ATTR refProcConnectionEvt(connectionEvt_t event)
{
    os_printf("%s evt: %d, rxGameStartMsg %d, rxGameStartAck %d\r\n", __func__, event, rxGameStartMsg, rxGameStartAck);

    switch(event)
    {
        case RX_GAME_START_MSG:
        {
            // Already received the ack, become the client
            if(!rxGameStartMsg && rxGameStartAck)
            {
                csRole = CLIENT;
            }
            // Mark this event
            rxGameStartMsg = true;
            break;
        }
        case RX_GAME_START_ACK:
        {
            // Already received the msg, become the server
            if(!rxGameStartAck && rxGameStartMsg)
            {
                csRole = SERVER;
            }
            // Mark this event
            rxGameStartAck = true;
            break;
        }
    }

    // If both the game start messages are good, start the game
    if(rxGameStartMsg && rxGameStartAck)
    {
        refStartPlaying();
    }
}

/**
 * This is called after connection is all done. Start the game!
 */
void ICACHE_FLASH_ATTR refStartPlaying(void)
{
    os_printf("%s\r\n", __func__);

    gameState = R_PLAYING;

    if(SERVER == csRole)
    {
        refStartRound();
    }
}

/**
 * TODO a game
 */
void ICACHE_FLASH_ATTR refStartRound(void)
{
    ;
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
void ICACHE_FLASH_ATTR refSendMsg(const char* msg, uint16_t len, bool shouldAck, void (*success)(void),
                                  void (*failure)(void))
{
    char* dbgMsg = (char*)os_zalloc(sizeof(char) * (len + 1));
    ets_memcpy(dbgMsg, msg, len);
    os_printf("%s: %s\r\n", __func__, dbgMsg);
    os_free(dbgMsg);

    if(shouldAck)
    {
        // Set the state to wait for an ack
        isWaitingForAck = true;

        // If this is not a retry
        if(msgToAck != msg)
        {
            os_printf("sending for the first time\r\n");

            // Store the message for potential retries
            ets_memcpy(msgToAck, msg, len);
            msgToAckLen = len;
            ackSuccess = success;
            ackFailure = failure;

            // Set the number of retries
            refTxRetries = REFLECTOR_ACK_RETRIES;
        }
        else
        {
            os_printf("this is a retry\r\n");
        }

        // Start the timer
        uint32_t retryTimeMs = 500 * (REFLECTOR_ACK_RETRIES - refTxRetries + 1);
        os_printf("ack timer set for %d\r\n", retryTimeMs);
        os_timer_arm(&refTxRetryTimer, retryTimeMs, false);
    }
    espNowSend((const uint8_t*)msg, len);
}

/**
 * This is called on a timer after refSendMsg(). The timer is disarmed if
 * the message is ACKed. If the message isn't ACKed, this will retry
 * transmission, up to REFLECTOR_ACK_RETRIES times
 *
 * @param arg unused
 */
void ICACHE_FLASH_ATTR refTxRetryTimeout(void* arg __attribute__((unused)) )
{
    if(0 != refTxRetries)
    {
        os_printf("Retrying message \"%s\"", msgToAck);
        refTxRetries--;
        refSendMsg(msgToAck, msgToAckLen, true, ackSuccess, ackFailure);
    }
    else
    {
        os_printf("Message totally failed \"%s\"", msgToAck);
        if(NULL != ackFailure)
        {
            ackFailure();
        }
    }
}

/**
 * This callback is called after a transmission. It only cares if the
 * message was transmitted, not if it was received or ACKed or anything
 *
 * @param mac_addr The MAC this message was sent to
 * @param status   The status of this transmission
 */
void ICACHE_FLASH_ATTR refSendCb(uint8_t* mac_addr __attribute__((unused)),
                                 mt_tx_status status  __attribute__((unused)) )
{
    // Debug print the received payload for now
    // os_printf("message sent\r\n");
}

/**
 * This is called whenever a button is pressed
 *
 * TODO a game
 *
 * @param state
 * @param button
 * @param down
 */
void ICACHE_FLASH_ATTR refButton(uint8_t state, int button, int down)
{
    os_printf("button pressed 0x%X, %d %d\r\n", state, button, down);
}
