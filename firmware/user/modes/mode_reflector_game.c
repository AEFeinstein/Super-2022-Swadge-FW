/*
 * mode_espnow_test.c
 *
 *  Created on: Oct 27, 2018
 *      Author: adam
 *
 * TODO add timers during the connection process! Think it all through
 * TODO add timers during the game too
 * TODO add logic for both players have an opportunity to lose? last licks-ish. probably not...
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

//#define DEBUGGING_GAME

#define REFLECTOR_ACK_RETRIES 3
#define CONNECTION_RSSI 60
#define LED_PERIOD_MS 100
#define DEG_PER_LED 60

/*============================================================================
 * Enums
 *==========================================================================*/

typedef enum
{
    R_CONNECTING,
    R_SHOW_CONNECTION,
    R_PLAYING,
    R_WAITING,
    R_SHOW_GAME_RESULT
} reflectorGameState_t;

typedef enum
{
    GOING_SECOND,
    GOING_FIRST
} playOrder_t;

typedef enum
{
    RX_GAME_START_ACK,
    RX_GAME_START_MSG
} connectionEvt_t;

typedef enum
{
    LED_OFF,
    LED_ON_1,
    LED_DIM_1,
    LED_ON_2,
    LED_DIM_2,
    LED_OFF_WAIT,
    LED_CONNECTED_BRIGHT,
    LED_CONNECTED_DIM,
} connLedState_t;

typedef enum
{
    ACT_CLOCKWISE        = 0,
    ACT_COUNTERCLOCKWISE = 1,
    ACT_BOTH             = 2
} gameAction_t;

/*============================================================================
 * Prototypes
 *==========================================================================*/

// SwadgeMode Callbacks
void ICACHE_FLASH_ATTR refInit(void);
void ICACHE_FLASH_ATTR refDeinit(void);
void ICACHE_FLASH_ATTR refButton(uint8_t state, int button, int down);
void ICACHE_FLASH_ATTR refRecvCb(uint8_t* mac_addr, uint8_t* data, uint8_t len, uint8_t rssi);

void ICACHE_FLASH_ATTR refRestart(void);

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
void ICACHE_FLASH_ATTR refStartPlaying(void* arg __attribute__((unused)));
void ICACHE_FLASH_ATTR refStartRound(void);
void ICACHE_FLASH_ATTR refSendRoundLossMsg(void);

// LED Functions
void ICACHE_FLASH_ATTR refDisarmAllLedTimers(void);
void ICACHE_FLASH_ATTR refConnLedTimeout(void* arg __attribute__((unused)));
void ICACHE_FLASH_ATTR refShowConnectionLedTimeout(void* arg __attribute__((unused)));
void ICACHE_FLASH_ATTR refGameLedTimeout(void* arg __attribute__((unused)));
void ICACHE_FLASH_ATTR refRoundResultLed(bool);

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
    .fnEspNowSendCb = NULL,
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
playOrder_t playOrder = GOING_SECOND;

// Game state variables
gameAction_t gameAction = ACT_CLOCKWISE;
bool shouldTurnOnLeds = false;
uint8_t refWins = 0;
uint8_t refLosses = 0;
uint8_t ledTimerMs = 12; // This can't be less than 3ms, it's impossible

// This swadge's MAC, in string form
char macStr[] = "00:00:00:00:00:00";
uint8_t otherMac[6] = {0};
bool otherMacReceived = false;

// Messages to send.
#define HDR_IDX 0
#define CMD_IDX 4
#define MAC_IDX 8
#define EXT_IDX 26
char connectionMsg[]     = "ref_con";
char ackMsg[]            = "ref_ack_00:00:00:00:00:00";
char gameStartMsg[]      = "ref_str_00:00:00:00:00:00";
char roundLossMsg[]      = "ref_los_00:00:00:00:00:00";
char roundContinueMsg[]  = "ref_cnt_00:00:00:00:00:00_xx";
char spdUp[] = "up";
char spdDn[] = "dn";
char spdNc[] = "nc";

// Timers
static os_timer_t refTxRetryTimer = {0};
static os_timer_t refConnectionTimer = {0};
static os_timer_t refStartPlayingTimer = {0};
static os_timer_t refConnLedTimer = {0};
static os_timer_t refShowConnectionLedTimer = {0};
static os_timer_t refGameLedTimer = {0};

// LED variables
uint8_t refLeds[6][3] = {{0}};
connLedState_t refConnLedState = LED_OFF;
sint16_t refDegree = 0;

/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * Initialize everything and start sending broadcast messages
 */
void ICACHE_FLASH_ATTR refInit(void)
{
    os_printf("%s\r\n", __func__);

    gameState = R_CONNECTING;

    // Variables to track acking messages
    isWaitingForAck = false;
    ets_memset(msgToAck, 0, sizeof(msgToAck));
    msgToAckLen = 0;
    refTxRetries = 0;
    ackSuccess = NULL;
    ackFailure = NULL;

    // Connection state variables
    broadcastReceived = false;
    rxGameStartMsg = false;
    rxGameStartAck = false;
    playOrder = GOING_SECOND;

    // Game state variables
    gameAction = ACT_CLOCKWISE;
    shouldTurnOnLeds = false;
    refWins = 0;
    refLosses = 0;
    ledTimerMs = 12;

    // The other swadge's MAC
    ets_memset(otherMac, 0, sizeof(otherMac));
    otherMacReceived = false;

    // Timers
    ets_memset(&refConnectionTimer, 0, sizeof(os_timer_t));
    ets_memset(&refTxRetryTimer, 0, sizeof(os_timer_t));
    ets_memset(&refConnLedTimer, 0, sizeof(os_timer_t));
    ets_memset(&refStartPlayingTimer, 0, sizeof(os_timer_t));
    ets_memset(&refShowConnectionLedTimer, 0, sizeof(os_timer_t));
    ets_memset(&refGameLedTimer, 0, sizeof(os_timer_t));

    // LED variables
    ets_memset(&refLeds[0][0], 0, sizeof(refLeds));
    refConnLedState = LED_OFF;
    refDegree = 0;

    // Get and save the string form of our MAC address
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

    // Set up a timer for showing a successful connection, don't start it
    os_timer_disarm(&refShowConnectionLedTimer);
    os_timer_setfn(&refShowConnectionLedTimer, refShowConnectionLedTimeout, NULL);

    // Set up a timer for showing the game, don't start it
    os_timer_disarm(&refGameLedTimer);
    os_timer_setfn(&refGameLedTimer, refGameLedTimeout, NULL);

    // Set up a timer for starting the next round, don't start it
    os_timer_disarm(&refStartPlayingTimer);
    os_timer_setfn(&refStartPlayingTimer, refStartPlaying, NULL);

    // Start a timer to do an initial connection, start it
    os_timer_disarm(&refConnectionTimer);
    os_timer_setfn(&refConnectionTimer, refConnectionTimeout, NULL);

    // Start a timer to update LEDs, start it
    os_timer_disarm(&refConnLedTimer);
    os_timer_setfn(&refConnLedTimer, refConnLedTimeout, NULL);

#ifdef DEBUGGING_GAME
    playOrder = GOING_FIRST;
    refStartPlaying(NULL);
#else
    os_timer_arm(&refConnectionTimer, 1, false);
    os_timer_arm(&refConnLedTimer, 1, true);
#endif
}

/**
 * Clean up all timers
 */
void ICACHE_FLASH_ATTR refDeinit(void)
{
    os_printf("%s\r\n", __func__);

    os_timer_disarm(&refConnectionTimer);
    os_timer_disarm(&refTxRetryTimer);
    os_timer_disarm(&refStartPlayingTimer);
    refDisarmAllLedTimers();
}

/**
 * Restart by deiniting then initing
 */
void ICACHE_FLASH_ATTR refRestart(void)
{
    refDeinit();
    refInit();
}

/**
 * Disarm any timers which control LEDs
 */
void ICACHE_FLASH_ATTR refDisarmAllLedTimers(void)
{
    os_timer_disarm(&refConnLedTimer);
    os_timer_disarm(&refShowConnectionLedTimer);
    os_timer_disarm(&refGameLedTimer);
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
    os_timer_arm(&refConnectionTimer, timeoutMs, false);
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

    // Check if this is a "ref" message
    if(len < CMD_IDX ||
            (0 != ets_memcmp(data, connectionMsg, CMD_IDX)))
    {
        // This message is too short, or not a "ref" message
        os_printf("Not a ref message\r\n");
        return;
    }

    // If this message has a MAC, check it
    if(len >= ets_strlen(ackMsg) &&
            0 != ets_memcmp(&data[MAC_IDX], macStr, ets_strlen(macStr)))
    {
        // This MAC isn't for us
        os_printf("Not for our MAC\r\n");
        return;
    }

    // If this is anything besides a broadcast, check the other MAC
    if(otherMacReceived &&
            len > ets_strlen(connectionMsg) &&
            0 != ets_memcmp(mac_addr, otherMac, sizeof(otherMac)))
    {
        // This isn't from the other known swadge
        os_printf("Not from the other MAC\r\n");
        return;
    }

    // By here, we know the received message was a "ref" message, either a
    // broadcast or for us. If this isn't an ack message, ack it
    if(len >= MAC_IDX &&
            0 != ets_memcmp(data, ackMsg, MAC_IDX))
    {
        refSendAckToMac(mac_addr);
    }

    // ACKs can be received in any state
    if(isWaitingForAck)
    {
        // Check if this is an ACK
        if(ets_strlen(ackMsg) == len &&
                0 == ets_memcmp(data, ackMsg, MAC_IDX))
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
                otherMacReceived = true;

                // Send a message to that ESP to start the game.
                ets_sprintf(&gameStartMsg[MAC_IDX], "%02X:%02X:%02X:%02X:%02X:%02X",
                            mac_addr[0],
                            mac_addr[1],
                            mac_addr[2],
                            mac_addr[3],
                            mac_addr[4],
                            mac_addr[5]);

                // If it's acked, call refGameStartAckRecv(), if not reinit with refInit()
                refSendMsg(gameStartMsg, ets_strlen(gameStartMsg), true, refGameStartAckRecv, refRestart);
            }
            // Received a response to our broadcast
            else if (!rxGameStartMsg &&
                     ets_strlen(gameStartMsg) == len &&
                     0 == ets_memcmp(data, gameStartMsg, MAC_IDX))
            {
                os_printf("Game start message received, ACKing\r\n");

                // This is another swadge trying to start a game, which means
                // they received our connectionMsg. First disable our connectionMsg
                os_timer_disarm(&refConnectionTimer);

                // And process this connection event
                refProcConnectionEvt(RX_GAME_START_MSG);
            }

            break;
        }
        case R_WAITING:
        {
            // Received a message that the other swadge lost
            if(ets_strlen(roundLossMsg) == len &&
                    0 == ets_memcmp(data, roundLossMsg, MAC_IDX))
            {
                // The other swadge lost, so chalk a win!
                refWins++;

                // Display the win
                refRoundResultLed(true);
            }
            else if(ets_strlen(roundContinueMsg) == len &&
                    0 == ets_memcmp(data, roundContinueMsg, MAC_IDX))
            {
                // Get faster or slower based on the other swadge's timing
                if(0 == ets_memcmp(&data[EXT_IDX], spdUp, ets_strlen(spdUp)))
                {
                    ledTimerMs--;
                    // Anything less than a 3ms period is impossible...
                    if(ledTimerMs < 3)
                    {
                        ledTimerMs = 3;
                    }
                }
                else if(0 == ets_memcmp(&data[EXT_IDX], spdDn, ets_strlen(spdDn)))
                {
                    ledTimerMs++;
                }

                refStartRound();
            }
            break;
        }
        case R_PLAYING:
        {
            // Currently playing a game, shouldn't do anything with messages
            break;
        }
        case R_SHOW_CONNECTION:
        case R_SHOW_GAME_RESULT:
        {
            // Just LED animations, don't do anything with messages
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
                playOrder = GOING_SECOND;
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
                playOrder = GOING_FIRST;
            }
            // Mark this event
            rxGameStartAck = true;
            break;
        }
    }

    // If both the game start messages are good, start the game
    if(rxGameStartMsg && rxGameStartAck)
    {
        gameState = R_SHOW_CONNECTION;

        ets_memset(&refLeds[0][0], 0, sizeof(refLeds));
        refConnLedState = LED_CONNECTED_BRIGHT;

        refDisarmAllLedTimers();
        // 6ms * ~500 steps == 3s animation
        os_timer_arm(&refShowConnectionLedTimer, 6, true);
    }
}

/**
 * This LED handling timer fades in and fades out white LEDs to indicate
 * a successful connection. After the animation, the game will start
 *
 * @param arg unused
 */
void ICACHE_FLASH_ATTR refShowConnectionLedTimeout(void* arg __attribute__((unused)) )
{
    uint8_t currBrightness = refLeds[0][0];
    switch(refConnLedState)
    {
        case LED_CONNECTED_BRIGHT:
        {
            currBrightness++;
            if(currBrightness == 0xFF)
            {
                refConnLedState = LED_CONNECTED_DIM;
            }
            break;
        }
        case LED_CONNECTED_DIM:
        {
            currBrightness--;
            if(currBrightness == 0x00)
            {
                refStartPlaying(NULL);
            }
            break;
        }
        default:
        {
            // No other cases handled
            break;
        }
    }
    ets_memset(&refLeds[0][0], currBrightness, sizeof(refLeds));
    setLeds(&refLeds[0][0], sizeof(refLeds));
}

/**
 * This is called after connection is all done. Start the game!
 *
 * @param arg unused
 */
void ICACHE_FLASH_ATTR refStartPlaying(void* arg __attribute__((unused)))
{
    os_printf("%s\r\n", __func__);

    // Turn off the LEDs
    refDisarmAllLedTimers();
    ets_memset(&refLeds[0][0], 0, sizeof(refLeds));
    setLeds(&refLeds[0][0], sizeof(refLeds));

    // Check for match end
    os_printf("wins: %d, losses %d\r\n", refWins, refLosses);
    if(refWins == 3 || refLosses == 3)
    {
        // TODO tally match wins in SPI flash?

        // Match over, reset everything
        refRestart();
    }
    else if(GOING_FIRST == playOrder)
    {
        gameState = R_PLAYING;

        // Start playing
        refStartRound();
    }
    else if(GOING_SECOND == playOrder)
    {
        gameState = R_WAITING;
    }
}

/**
 * Start a round of the game by picking a random action and starting
 * refGameLedTimeout()
 */
void ICACHE_FLASH_ATTR refStartRound(void)
{
    gameState = R_PLAYING;

    // pick a random game action
    gameAction = os_random() % 3;

    // Set the LED's starting angle
    switch(gameAction)
    {
        case ACT_CLOCKWISE:
        {
            os_printf("ACT_CLOCKWISE\r\n");
            refDegree = 300;
            break;
        }
        case ACT_COUNTERCLOCKWISE:
        {
            os_printf("ACT_COUNTERCLOCKWISE\r\n");
            refDegree = 60;
            break;
        }
        case ACT_BOTH:
        {
            os_printf("ACT_BOTH\r\n");
            refDegree = 0;
            break;
        }
    }
    shouldTurnOnLeds = true;

    // Set the LEDs spinning
    refDisarmAllLedTimers();
#ifdef DEBUGGING_GAME
    static uint8_t ledPeriodMs = 100;
    os_printf("led period %d\r\n", ledPeriodMs / 10);
    ledPeriodMs--;
    os_timer_arm(&refGameLedTimer, ledPeriodMs / 10, true);
#else
    os_timer_arm(&refGameLedTimer, ledTimerMs, true);
#endif
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
        os_printf("Retrying message \"%s\"\r\n", msgToAck);
        refTxRetries--;
        refSendMsg(msgToAck, msgToAckLen, true, ackSuccess, ackFailure);
    }
    else
    {
        os_printf("Message totally failed \"%s\"\r\n", msgToAck);
        if(NULL != ackFailure)
        {
            ackFailure();
        }
    }
}

/**
 * Called every 100ms, this updates the LEDs during connection
 */
void ICACHE_FLASH_ATTR refConnLedTimeout(void* arg __attribute__((unused)))
{
    switch(refConnLedState)
    {
        case LED_OFF:
        {
            // Reset this timer to LED_PERIOD_MS
            refDisarmAllLedTimers();
            os_timer_arm(&refConnLedTimer, LED_PERIOD_MS, true);

            ets_memset(&refLeds[0][0], 0, sizeof(refLeds));

            refConnLedState = LED_ON_1;
            break;
        }
        case LED_ON_1:
        {
            // Turn on blue
            refLeds[0][2] = 250;
            // Prepare the first dimming
            refConnLedState = LED_DIM_1;
            break;
        }
        case LED_DIM_1:
        {
            // Dim blue
            refLeds[0][2] -= 25;
            // If its kind of dim, turn it on again
            if(refLeds[0][2] == 25)
            {
                refConnLedState = LED_ON_2;
            }
            break;
        }
        case LED_ON_2:
        {
            // Turn on blue
            refLeds[0][2] = 250;
            // Prepare the second dimming
            refConnLedState = LED_DIM_2;
            break;
        }
        case LED_DIM_2:
        {
            // Dim blue
            refLeds[0][2] -= 25;
            // If its off, start waiting
            if(refLeds[0][2] == 0)
            {
                refConnLedState = LED_OFF_WAIT;
            }
            break;
        }
        case LED_OFF_WAIT:
        {
            // Start a timer to update LEDs
            refDisarmAllLedTimers();
            os_timer_arm(&refConnLedTimer, 1000, true);

            // When it fires, start all over again
            refConnLedState = LED_OFF;

            // And dont update the LED state this time
            return;
        }
        case LED_CONNECTED_BRIGHT:
        case LED_CONNECTED_DIM:
        {
            // Handled in refShowConnectionLedTimeout()
            break;
        }
    }

    // Copy the color value to all LEDs
    uint8_t i;
    for(i = 1; i < 6; i ++)
    {
        refLeds[i][0] = refLeds[0][0];
        refLeds[i][1] = refLeds[0][1];
        refLeds[i][2] = refLeds[0][2];
    }

    // Overwrite two LEDs based on the connection status
    if(rxGameStartAck)
    {
        refLeds[2][0] = 25;
        refLeds[2][1] = 0;
        refLeds[2][2] = 0;
    }
    if(rxGameStartMsg)
    {
        refLeds[4][0] = 25;
        refLeds[4][1] = 0;
        refLeds[4][2] = 0;
    }

    // Physically set the LEDs
    setLeds(&refLeds[0][0], sizeof(refLeds));
}

/**
 * Called every 100ms, this updates the LEDs during the game
 */
void ICACHE_FLASH_ATTR refGameLedTimeout(void* arg __attribute__((unused)))
{
    // Decay all LEDs
    uint8_t i;
    for(i = 0; i < 6; i++)
    {
        if(refLeds[i][0] > 0)
        {
            refLeds[i][0] -= 4;
        }
        if(refLeds[i][1] > 0)
        {
            refLeds[i][1] -= 4;
        }
        if(refLeds[i][2] > 0)
        {
            refLeds[i][2] -= 4;
        }
    }

    // Sed LEDs according to the mode
    if (shouldTurnOnLeds && refDegree % DEG_PER_LED == 0)
    {
        switch(gameAction)
        {
            case ACT_BOTH:
            {
                // Make sure this value decays to exactly zero above
                refLeds[refDegree / DEG_PER_LED][0] = 0;
                refLeds[refDegree / DEG_PER_LED][1] = 252;
                refLeds[refDegree / DEG_PER_LED][2] = 0;

                refLeds[(360 - refDegree) / DEG_PER_LED][0] = 0;
                refLeds[(360 - refDegree) / DEG_PER_LED][1] = 252;
                refLeds[(360 - refDegree) / DEG_PER_LED][2] = 0;
                break;
            }
            case ACT_COUNTERCLOCKWISE:
            case ACT_CLOCKWISE:
            {
                refLeds[refDegree / DEG_PER_LED][0] = 0;
                refLeds[refDegree / DEG_PER_LED][1] = 252;
                refLeds[refDegree / DEG_PER_LED][2] = 0;
                break;
            }
        }

        // Don't turn on LEDs past 180 degrees
        if(180 == refDegree)
        {
            os_printf("end of pattern\r\n");
            shouldTurnOnLeds = false;
        }
    }

    // Move the exciter according to the mode
    switch(gameAction)
    {
        case ACT_BOTH:
        case ACT_CLOCKWISE:
        {
            refDegree += 2;
            if(refDegree > 359)
            {
                refDegree -= 360;
            }

            break;
        }
        case ACT_COUNTERCLOCKWISE:
        {
            refDegree -= 2;
            if(refDegree < 0)
            {
                refDegree += 360;
            }

            break;
        }
    }

    // Physically set the LEDs
    setLeds(&refLeds[0][0], sizeof(refLeds));

    uint8_t blankLeds[6][3] = {{0}};
    if(false == shouldTurnOnLeds &&
            0 == ets_memcmp(&refLeds[0][0], &blankLeds[0][0], sizeof(blankLeds)))
    {
        // If the last LED is off, the user missed the window of opportunity
        refSendRoundLossMsg();
    }
}

/**
 * This is called whenever a button is pressed
 *
 * If a game is being played, check for button down events and either succeed
 * or fail the round and pass the result to the other swadge
 *
 * @param state  A bitmask of all button states
 * @param button The button which triggered this action
 * @param down   true if the button was pressed, false if it was released
 */
void ICACHE_FLASH_ATTR refButton(uint8_t state, int button, int down)
{
    // If this was a down button press (ignore other states and ups)
    if(R_PLAYING == gameState && true == down)
    {
        bool success = false;
        bool failed = false;

        // And the final LED is lit
        if(refLeds[3][1] > 0)
        {
            // If it's the right button for a single button mode
            if ((ACT_COUNTERCLOCKWISE == gameAction && 2 == button) ||
                    (ACT_CLOCKWISE == gameAction && 1 == button))
            {
                success = true;
            }
            // Or both buttons for both
            else if(ACT_BOTH == gameAction && ((0b110 & state) == 0b110))
            {
                success = true;
            }
        }
        else
        {
            // If the final LED isn't lit, it's always a failure
            failed = true;
        }

        if(success)
        {
            os_printf("Won the round, continue the game\r\n");
#ifdef DEBUGGING_GAME
            refStartRound();
#else
            // Now waiting for a result from the other swadge
            gameState = R_WAITING;

            char* spdPtr;
            // Add information about the timing
            if(refLeds[3][1] >= 192)
            {
                // Speed up if the button is pressed when the LED is brightest
                spdPtr = spdUp;
            }
            else if(refLeds[3][1] >= 64)
            {
                // No change for the middle range
                spdPtr = spdNc;
            }
            else
            {
                // Slow down if button is pressed when the LED is dimmest
                spdPtr = spdDn;
            }

            // Clear the LEDs and stop the timer
            refDisarmAllLedTimers();
            ets_memset(&refLeds[0][0], 0, sizeof(refLeds));
            setLeds(&refLeds[0][0], sizeof(refLeds));

            // Send a message to the other swadge that this round was a success
            ets_sprintf(&roundContinueMsg[MAC_IDX], "%02X:%02X:%02X:%02X:%02X:%02X_%s",
                        otherMac[0],
                        otherMac[1],
                        otherMac[2],
                        otherMac[3],
                        otherMac[4],
                        otherMac[5],
                        spdPtr);

            // If it's not acked, reinit with refRestart()
            refSendMsg(roundContinueMsg, ets_strlen(roundContinueMsg), true, NULL, refRestart);
#endif
        }
        else if(failed)
        {
            // Tell the other swadge
            refSendRoundLossMsg();
        }
        else
        {
            os_printf("Neither won nor lost the round\r\n");
        }
    }
}

/**
 * This is called when a round is lost. It tallies the loss, calls
 * refRoundResultLed() to display the wins/losses and set up the
 * potential next round, and sends a message to the other swadge
 * that the round was lost and
 *
 * Send a round loss message to the other swadge
 */
void ICACHE_FLASH_ATTR refSendRoundLossMsg(void)
{
    os_printf("Lost the round\r\n");
#ifdef DEBUGGING_GAME
    refStartRound();
#else

    // Tally the loss
    refLosses++;

    // Show the current wins & losses
    refRoundResultLed(false);

    // Send a message to that ESP that we lost the round
    ets_sprintf(&roundLossMsg[MAC_IDX], "%02X:%02X:%02X:%02X:%02X:%02X",
                otherMac[0],
                otherMac[1],
                otherMac[2],
                otherMac[3],
                otherMac[4],
                otherMac[5]);
    // If it's not acked, reinit with refRestart()
    refSendMsg(roundLossMsg, ets_strlen(roundLossMsg), true, NULL, refRestart);
#endif
}

/**
 * Show the wins and losses
 *
 * @param roundWinner true if this swadge was a winner, false if the other
 *                    swadge won
 */
void ICACHE_FLASH_ATTR refRoundResultLed(bool roundWinner)
{
    sint8_t i;

    // Light green for wins
    for(i = 4; i < 4 + refWins; i++)
    {
        // Green
        refLeds[i % 6][0] = 255;
        refLeds[i % 6][1] = 0;
        refLeds[i % 6][2] = 0;
    }

    // Light reds for losses
    for(i = 2; i >= (3 - refLosses); i--)
    {
        // Red
        refLeds[i][0] = 0;
        refLeds[i][1] = 255;
        refLeds[i][2] = 0;
    }

    // Push out LED data
    refDisarmAllLedTimers();
    setLeds(&refLeds[0][0], sizeof(refLeds));

    // Set up the next round based on the winner
    if(roundWinner)
    {
        gameState = R_SHOW_GAME_RESULT;
        playOrder = GOING_FIRST;
    }
    else
    {
        // Set gameState here to R_WAITING to make sure a message isn't missed
        gameState = R_WAITING;
        playOrder = GOING_SECOND;
    }

    // Call refStartPlaying in 3 seconds
    os_timer_arm(&refStartPlayingTimer, 3000, false);
}
