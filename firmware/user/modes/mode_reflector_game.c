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
#include "mode_reflector_game.h"
#include "osapi.h"

/*============================================================================
 * Defines
 *==========================================================================*/

#define REFLECTOR_ACK_RETRIES 3

/*============================================================================
 * Enums
 *==========================================================================*/

typedef enum
{
    R_LOOKING_FOR_SWADGE,
    R_CONFIRMING_CONNECTION,
    R_WAITING_FOR_ACK,
    R_GAME_START,
} reflectorGameState_t;

/*============================================================================
 * Prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR reflectorInit(void);
void ICACHE_FLASH_ATTR reflectorButton(uint8_t state, int button, int down);
void ICACHE_FLASH_ATTR reflectorSendCb(uint8_t* mac_addr, mt_tx_status status);
void ICACHE_FLASH_ATTR reflectorRecvCb(uint8_t* mac_addr, uint8_t* data, uint8_t len, uint8_t rssi);
void ICACHE_FLASH_ATTR reflectorSendMsg(const char* msg, uint16_t len, bool shouldAck, void (*success)(void),
                                        void (*failure)(void));

void ICACHE_FLASH_ATTR initConnectionTimeout(void* arg);
void ICACHE_FLASH_ATTR retryTimeout(void* arg);
void ICACHE_FLASH_ATTR reflectorGameStart(void);

/*============================================================================
 * Variables
 *==========================================================================*/

swadgeMode reflectorGameMode =
{
    .modeName = "reflector_game",
    .fnEnterMode = reflectorInit,
    .fnExitMode = NULL,
    .fnTimerCallback = NULL,
    .fnButtonCallback = reflectorButton,
    .fnAudioCallback = NULL,
    .wifiMode = ESP_NOW,
    .fnEspNowRecvCb = reflectorRecvCb,
    .fnEspNowSendCb = reflectorSendCb,
};

reflectorGameState_t gameState = R_LOOKING_FOR_SWADGE;

char msgToAck[32] = {0};
uint16_t msgToAckLen = 0;
void (*ackSuccess)(void) = NULL;
void (*ackFailure)(void) = NULL;

char macStr[] = "00:00:00:00:00:00";

char connectionMsg[] = "ref_con";
char ackMsg[]        = "ref_ack_00:00:00:00:00:00";
char gameStartMsg[]  = "ref_str_00:00:00:00:00:00";

uint8_t otherMac[6] = {0};

static os_timer_t initConnectionTimer = {0};

static os_timer_t reflectorRetryTimer = {0};
uint8_t reflectorAckRetries = 0;

/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * Initialize everything and start sending broadcast messages
 */
void ICACHE_FLASH_ATTR reflectorInit(void)
{
    // Set the state
    gameState = R_LOOKING_FOR_SWADGE;

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

    // Start a timer to do an initial connection
    os_timer_disarm(&initConnectionTimer);
    os_timer_setfn(&initConnectionTimer, initConnectionTimeout, NULL);
    os_timer_arm(&initConnectionTimer, 1, false);

    // Set up a timer for acking messages
    os_timer_disarm(&reflectorRetryTimer);
    os_timer_setfn(&reflectorRetryTimer, retryTimeout, NULL);
}

/**
 * This is called on the timer initConnectionTimer. It broadcasts the connectionMsg
 *
 * @param arg unused
 */
void ICACHE_FLASH_ATTR initConnectionTimeout(void* arg __attribute__((unused)) )
{
    // Send a connection broadcast
    reflectorSendMsg(connectionMsg, sizeof(connectionMsg), false, NULL, NULL);

    // os_random returns a 32 bit number, so this is [500ms,1500ms]
    uint32_t timeoutMs = 100 * (5 + (os_random() % 11));

    // Start the timer again
    os_printf("retry in %dms\r\n", timeoutMs);
    os_timer_arm(&initConnectionTimer, timeoutMs, false);
}

/**
 * This is called whenever an ESP NOW packet is received
 *
 * @param mac_addr
 * @param data
 * @param len
 */
void ICACHE_FLASH_ATTR reflectorRecvCb(uint8_t* mac_addr, uint8_t* data, uint8_t len, uint8_t rssi)
{
    // Debug print the received payload
    os_printf("message received\r\n");
    switch(gameState)
    {
        case R_LOOKING_FOR_SWADGE:
        {
            // Check if this is another connection message with strong RSSI
            if(rssi > 60 && 0 == ets_memcmp(data, connectionMsg, len))
            {
                // Save the other ESP's MAC
                ets_memcpy(otherMac, mac_addr, sizeof(otherMac));

                // Send a message to that ESP to start the game. If it isn't acked, reinit
                ets_sprintf(&gameStartMsg[8], "%02X:%02X:%02X:%02X:%02X:%02X",
                            mac_addr[0],
                            mac_addr[1],
                            mac_addr[2],
                            mac_addr[3],
                            mac_addr[4],
                            mac_addr[5]);
                reflectorSendMsg(gameStartMsg, sizeof(gameStartMsg), true, reflectorGameStart, reflectorInit);
                gameState = R_CONFIRMING_CONNECTION;
            }
            else if (ets_memcmp(data, "ref_str", 7) && ets_memcmp(&data[8], macStr, sizeof(macStr)))
            {
                // This is another swadge trying to start a game, which means
                // they received our connectionMsg. First disable our connectionMsg
                os_timer_disarm(&initConnectionTimer);

                // Then ACK their request to start a game
                ets_sprintf(&ackMsg[8], "%02X:%02X:%02X:%02X:%02X:%02X",
                            mac_addr[0],
                            mac_addr[1],
                            mac_addr[2],
                            mac_addr[3],
                            mac_addr[4],
                            mac_addr[5]);
                reflectorSendMsg(ackMsg, 0, false, NULL, NULL);

                // TODO mark this somewhere?
                // TODO check for both RX acked gameStartMsg and RX "ref_str" message, then start the game?
                // TODO figure out who is the server and who is the client, based on order of RX message
            }

            break;
        }
        case R_CONFIRMING_CONNECTION:
        {
            break;
        }
        case R_WAITING_FOR_ACK:
        {
            // Check if this is an ACK from the other MAC
            if(ets_memcmp(mac_addr, otherMac, sizeof(otherMac)) &&
                    ets_memcmp(data, ackMsg, len))
            {
                // Call the function after receiving the ack
                if(NULL != ackSuccess)
                {
                    ackSuccess();
                }

                // Clear ack timeout variables
                os_timer_disarm(&reflectorRetryTimer);
                reflectorAckRetries = 0;
            }
            break;
        }
        case R_GAME_START:
        {
            break;
        }
    }
}

/**
 * This is called when gameStartMsg is acked,
 * so we received a connectionMsg first, then sent a gameStartMsg, then received a gameStartMsg
 */
void ICACHE_FLASH_ATTR reflectorGameStart(void)
{
    // TODO check for both RX acked gameStartMsg and RX "ref_str" message, then start the game?
    // TODO figure out who is the server and who is the client, based on order of RX message
}

/**
 * TODO
 *
 * @param msg
 * @param len
 * @param shouldAck
 * @param _stateAfterAck
 */
void ICACHE_FLASH_ATTR reflectorSendMsg(const char* msg, uint16_t len, bool shouldAck, void (*success)(void),
                                        void (*failure)(void))
{
    if(shouldAck)
    {
        // Set the state to wait for an ack
        gameState = R_WAITING_FOR_ACK;

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
            reflectorAckRetries = REFLECTOR_ACK_RETRIES;
        }
        else
        {
            os_printf("this is a retry\r\n");
        }

        // Start the timer
        uint32_t retryTimeMs = 500 * (REFLECTOR_ACK_RETRIES - reflectorAckRetries + 1);
        os_printf("ack timer set for %d\r\n", retryTimeMs);
        os_timer_arm(&reflectorRetryTimer, retryTimeMs, false);
    }
    espNowSend((const uint8_t*)msg, sizeof(len));
}

/**
 * TODO
 *
 * @param arg
 */
void ICACHE_FLASH_ATTR retryTimeout(void* arg __attribute__((unused)) )
{
    if(0 != reflectorAckRetries)
    {
        os_printf("Retrying message \"%s\"", msgToAck);
        reflectorAckRetries--;
        reflectorSendMsg(msgToAck, msgToAckLen, true, ackSuccess, ackFailure);
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
 * TODO doc
 *
 * @param mac_addr
 * @param status
 */
void ICACHE_FLASH_ATTR reflectorSendCb(uint8_t* mac_addr __attribute__((unused)),
                                       mt_tx_status status  __attribute__((unused)) )
{
    // Debug print the received payload
    os_printf("message sent\r\n");
}

/**
 * TODO doc
 *
 * @param state
 * @param button
 * @param down
 */
void ICACHE_FLASH_ATTR reflectorButton(uint8_t state  __attribute__((unused)), int button  __attribute__((unused)),
                                       int down  __attribute__((unused)) )
{
    ;
}
