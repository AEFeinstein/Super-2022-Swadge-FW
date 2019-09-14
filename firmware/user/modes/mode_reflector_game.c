/*
 * mode_espnow_test.c
 *
 *  Created on: Oct 27, 2018
 *      Author: adam
 *
 */

/*============================================================================
 * Includes
 *==========================================================================*/

#include <osapi.h>
#include <user_interface.h>

#include "user_main.h"
#include "mode_reflector_game.h"
#include "custom_commands.h"
#include "buttons.h"
#include "p2pConnection.h"

/*============================================================================
 * Defines
 *==========================================================================*/

// #define REF_DEBUG_PRINT
#ifdef REF_DEBUG_PRINT
    #define ref_printf(...) os_printf(__VA_ARGS__)
#else
    #define ref_printf(...)
#endif

// Degrees between each LED
#define DEG_PER_LED 60

// This can't be less than 3ms, it's impossible
#define LED_TIMER_MS_STARTING_EASY   13
#define LED_TIMER_MS_STARTING_MEDIUM 11
#define LED_TIMER_MS_STARTING_HARD    9

#define RESTART_COUNT_PERIOD_MS       250
#define RESTART_COUNT_BLINK_PERIOD_MS 750

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

typedef enum
{
    EASY   = 0,
    MEDIUM = 1,
    HARD   = 2
} difficulty_t;

typedef enum
{
    NOT_DISPLAYING,
    FAIL_DISPLAY_ON_1,
    FAIL_DISPLAY_OFF_2,
    FAIL_DISPLAY_ON_3,
    FAIL_DISPLAY_OFF_4,
    SCORE_DISPLAY_INIT,
    FIRST_DIGIT_INC,
    FIRST_DIGIT_OFF,
    FIRST_DIGIT_ON,
    FIRST_DIGIT_OFF_2,
    SECOND_DIGIT_INC,
    SECOND_DIGIT_OFF,
    SECOND_DIGIT_ON,
    SECOND_DIGIT_OFF_2,
    SCORE_DISPLAY_FINISH
} singlePlayerScoreDisplayState_t;

/*============================================================================
 * Prototypes
 *==========================================================================*/

// SwadgeMode Callbacks
void ICACHE_FLASH_ATTR refInit(void);
void ICACHE_FLASH_ATTR refDeinit(void);
void ICACHE_FLASH_ATTR refButton(uint8_t state, int button, int down);
void ICACHE_FLASH_ATTR refRecvCb(uint8_t* mac_addr, uint8_t* data, uint8_t len, uint8_t rssi);
void ICACHE_FLASH_ATTR refSendCb(uint8_t* mac_addr, mt_tx_status status);

// Helper function
void ICACHE_FLASH_ATTR refSinglePlayerRestart(void* arg __attribute__((unused)));
void ICACHE_FLASH_ATTR refSinglePlayerScoreLed(uint8_t ledToLight, led_t* colorPrimary, led_t* colorSecondary);
void ICACHE_FLASH_ATTR refConnectionCallback(p2pInfo* p2p, connectionEvt_t event);
void ICACHE_FLASH_ATTR refMsgCallbackFn(p2pInfo* p2p, char* msg, uint8_t* payload, uint8_t len);
void ICACHE_FLASH_ATTR refMsgTxCbFn(p2pInfo* p2p, messageStatus_t status);

// Game functions
void ICACHE_FLASH_ATTR refStartPlaying(void* arg __attribute__((unused)));
void ICACHE_FLASH_ATTR refStartRound(void);
void ICACHE_FLASH_ATTR refSendRoundLossMsg(void);
void ICACHE_FLASH_ATTR refAdjustledSpeed(bool reset, bool up);

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
    .modeName = "reflector game",
    .fnEnterMode = refInit,
    .fnExitMode = refDeinit,
    .fnButtonCallback = refButton,
    .fnAudioCallback = NULL,
    .wifiMode = ESP_NOW,
    .fnEspNowRecvCb = refRecvCb,
    .fnEspNowSendCb = refSendCb,
};

const char spdUp[] = "up";
const char spdDn[] = "dn";
const char spdNc[] = "nc";

struct
{
    reflectorGameState_t gameState;

    // Game state variables
    struct
    {
        gameAction_t Action;
        bool shouldTurnOnLeds;
        uint8_t Wins;
        uint8_t Losses;
        uint8_t ledPeriodMs;
        bool singlePlayer;
        uint16_t singlePlayerRounds;
        uint8_t singlePlayerTopHits;
        difficulty_t difficulty;
        bool receiveFirstMsg;
    } gam;

    // Timers
    struct
    {
        os_timer_t StartPlaying;
        os_timer_t ConnLed;
        os_timer_t ShowConnectionLed;
        os_timer_t GameLed;
        os_timer_t SinglePlayerRestart;
    } tmr;

    // LED variables
    struct
    {
        led_t Leds[6];
        connLedState_t ConnLedState;
        sint16_t Degree;
        uint8_t connectionDim;
        singlePlayerScoreDisplayState_t singlePlayerDisplayState;
        uint8_t digitToDisplay;
        uint8_t ledsLit;
    } led;

    p2pInfo p2pRef;
} ref;

// Colors
static led_t digitCountFirstPrimary =
{
    .r = 0xFE,
    .g = 0x87,
    .b = 0x1D,
};
static led_t digitCountFirstSecondary =
{
    .r = 0x11,
    .g = 0x39,
    .b = 0xFE,
};
static led_t digitCountSecondPrimary =
{
    .r = 0xCB,
    .g = 0x1F,
    .b = 0xFF,
};
static led_t digitCountSecondSecondary =
{
    .r = 0x00,
    .g = 0xFC,
    .b = 0x0F,
};

/*============================================================================
 * Functions
 *==========================================================================*/

void ICACHE_FLASH_ATTR refConnectionCallback(p2pInfo* p2p __attribute__((unused)), connectionEvt_t event)
{
    os_printf("%s %d\n", __func__, event);
    switch(event)
    {
        case CON_STARTED:
        {
            break;
        }
        case RX_GAME_START_ACK:
        {
            break;
        }
        case RX_GAME_START_MSG:
        {
            break;
        }
        case CON_ESTABLISHED:
        {
            // Connection was successful, so disarm the failure timer
            ref.gameState = R_SHOW_CONNECTION;

            ets_memset(ref.led.Leds, 0, sizeof(ref.led.Leds));
            ref.led.ConnLedState = LED_CONNECTED_BRIGHT;

            refDisarmAllLedTimers();
            // 6ms * ~500 steps == 3s animation
            os_timer_arm(&ref.tmr.ShowConnectionLed, 6, true);
            break;
        }
        default:
        case CON_LOST:
        {
            break;
        }
    }
}

/**
 * Initialize everything and start sending broadcast messages
 */
void ICACHE_FLASH_ATTR refInit(void)
{
    ref_printf("%s\r\n", __func__);

    // Enable button debounce for consistent 1p/2p and difficulty config
    enableDebounce(true);

    // Make sure everything is zero!
    ets_memset(&ref, 0, sizeof(ref));

    p2pInitialize(&ref.p2pRef, "ref", refConnectionCallback, refMsgCallbackFn, 55);

    // Set up a timer for showing a successful connection, don't start it
    os_timer_disarm(&ref.tmr.ShowConnectionLed);
    os_timer_setfn(&ref.tmr.ShowConnectionLed, refShowConnectionLedTimeout, NULL);

    // Set up a timer for showing the game, don't start it
    os_timer_disarm(&ref.tmr.GameLed);
    os_timer_setfn(&ref.tmr.GameLed, refGameLedTimeout, NULL);

    // Set up a timer for starting the next round, don't start it
    os_timer_disarm(&ref.tmr.StartPlaying);
    os_timer_setfn(&ref.tmr.StartPlaying, refStartPlaying, NULL);

    // Set up a timer to update LEDs, start it
    os_timer_disarm(&ref.tmr.ConnLed);
    os_timer_setfn(&ref.tmr.ConnLed, refConnLedTimeout, NULL);

    // Set up a timer to restart after failure. don't start it
    os_timer_disarm(&ref.tmr.SinglePlayerRestart);
    os_timer_setfn(&ref.tmr.SinglePlayerRestart, refSinglePlayerRestart, NULL);
    p2pStartConnection(&ref.p2pRef);
    os_timer_arm(&ref.tmr.ConnLed, 1, true);
}

/**
 * Clean up all timers
 */
void ICACHE_FLASH_ATTR refDeinit(void)
{
    ref_printf("%s\r\n", __func__);

    p2pDeinit(&ref.p2pRef);
    os_timer_disarm(&ref.tmr.StartPlaying);
    refDisarmAllLedTimers();
}

/**
 * Disarm any timers which control LEDs
 */
void ICACHE_FLASH_ATTR refDisarmAllLedTimers(void)
{
    os_timer_disarm(&ref.tmr.ConnLed);
    os_timer_disarm(&ref.tmr.ShowConnectionLed);
    os_timer_disarm(&ref.tmr.GameLed);
    os_timer_disarm(&ref.tmr.SinglePlayerRestart);
}

/**
 * This is called after an attempted transmission. If it was successful, and the
 * message should be acked, start a retry timer. If it wasn't successful, just
 * try again
 *
 * @param mac_addr
 * @param status
 */
void ICACHE_FLASH_ATTR refSendCb(uint8_t* mac_addr __attribute__((unused)),
                                 mt_tx_status status)
{
    p2pSendCb(&ref.p2pRef, mac_addr, status);
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
    p2pRecvCb(&ref.p2pRef, mac_addr, data, len, rssi);
}

/**
 * @brief
 *
 * @param msg
 * @param payload
 * @param len
 */
void ICACHE_FLASH_ATTR refMsgCallbackFn(p2pInfo* p2p __attribute__((unused)), char* msg, uint8_t* payload,
                                        uint8_t len __attribute__((unused)))
{
    if(len > 0)
    {
        ref_printf("%s %s %s\n", __func__, msg, payload);
    }
    else
    {
        ref_printf("%s %s\n", __func__, msg);
    }

    switch(ref.gameState)
    {
        case R_CONNECTING:
            break;
        case R_WAITING:
        {
            // Received a message that the other swadge lost
            if(0 == ets_memcmp(msg, "los", 3))
            {
                // The other swadge lost, so chalk a win!
                ref.gam.Wins++;

                // Display the win
                refRoundResultLed(true);
            }
            if(0 == ets_memcmp(msg, "cnt", 3))
            {
                // Get faster or slower based on the other swadge's timing
                if(0 == ets_memcmp(payload, spdUp, ets_strlen(spdUp)))
                {
                    refAdjustledSpeed(false, true);
                }
                else if(0 == ets_memcmp(payload, spdDn, ets_strlen(spdDn)))
                {
                    refAdjustledSpeed(false, false);
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
        default:
        {
            break;
        }
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
    uint8_t currBrightness = ref.led.Leds[0].r;
    switch(ref.led.ConnLedState)
    {
        case LED_CONNECTED_BRIGHT:
        {
            currBrightness++;
            if(currBrightness == 0xFF)
            {
                ref.led.ConnLedState = LED_CONNECTED_DIM;
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
        case LED_OFF:
        case LED_ON_1:
        case LED_DIM_1:
        case LED_ON_2:
        case LED_DIM_2:
        case LED_OFF_WAIT:
        default:
        {
            // No other cases handled
            break;
        }
    }
    ets_memset(ref.led.Leds, currBrightness, sizeof(ref.led.Leds));
    setLeds(ref.led.Leds, sizeof(ref.led.Leds));
}

/**
 * This is called after connection is all done. Start the game!
 *
 * @param arg unused
 */
void ICACHE_FLASH_ATTR refStartPlaying(void* arg __attribute__((unused)))
{
    ref_printf("%s\r\n", __func__);

    // Disable button debounce for minimum latency
    enableDebounce(false);

    // Turn off the LEDs
    refDisarmAllLedTimers();
    ets_memset(ref.led.Leds, 0, sizeof(ref.led.Leds));
    setLeds(ref.led.Leds, sizeof(ref.led.Leds));

    // Reset the LED timer to the default speed
    refAdjustledSpeed(true, true);

    // Check for match end
    ref_printf("wins: %d, losses %d\r\n", ref.gam.Wins, ref.gam.Losses);
    if(ref.gam.Wins == 3 || ref.gam.Losses == 3)
    {
        // Tally match win in SPI flash
        if(ref.gam.Wins == 3)
        {
            incrementRefGameWins();
        }

        // Match over, reset everything
        refDeinit();
        refInit();
    }
    else if(GOING_FIRST == p2pGetPlayOrder(&ref.p2pRef))
    {
        ref.gameState = R_PLAYING;

        // Start playing
        refStartRound();
    }
    else if(GOING_SECOND == p2pGetPlayOrder(&ref.p2pRef))
    {
        ref.gameState = R_WAITING;

        ref.gam.receiveFirstMsg = false;

        // TODO Start a timer to reinit if we never receive a result (disconnect)
        // refStartRestartTimer(NULL);
    }
}

/**
 * Start a round of the game by picking a random action and starting
 * refGameLedTimeout()
 */
void ICACHE_FLASH_ATTR refStartRound(void)
{
    ref.gameState = R_PLAYING;

    // pick a random game action
    ref.gam.Action = os_random() % 3;

    // Set the LED's starting angle
    switch(ref.gam.Action)
    {
        case ACT_CLOCKWISE:
        {
            ref_printf("ACT_CLOCKWISE\r\n");
            ref.led.Degree = 300;
            break;
        }
        case ACT_COUNTERCLOCKWISE:
        {
            ref_printf("ACT_COUNTERCLOCKWISE\r\n");
            ref.led.Degree = 60;
            break;
        }
        case ACT_BOTH:
        {
            ref_printf("ACT_BOTH\r\n");
            ref.led.Degree = 0;
            break;
        }
        default:
        {
            break;
        }
    }
    ref.gam.shouldTurnOnLeds = true;

    // Clear the LEDs first
    refDisarmAllLedTimers();
    ets_memset(ref.led.Leds, 0, sizeof(ref.led.Leds));
    setLeds(ref.led.Leds, sizeof(ref.led.Leds));
    // Then set the game in motion
    os_timer_arm(&ref.tmr.GameLed, ref.gam.ledPeriodMs, true);
}

/**
 * Called every 4ms, this updates the LEDs during connection
 */
void ICACHE_FLASH_ATTR refConnLedTimeout(void* arg __attribute__((unused)))
{
    switch(ref.led.ConnLedState)
    {
        case LED_OFF:
        {
            // Reset this timer to LED_PERIOD_MS
            refDisarmAllLedTimers();
            os_timer_arm(&ref.tmr.ConnLed, 4, true);

            ref.led.connectionDim = 0;
            ets_memset(ref.led.Leds, 0, sizeof(ref.led.Leds));

            ref.led.ConnLedState = LED_ON_1;
            break;
        }
        case LED_ON_1:
        {
            // Turn LEDs on
            ref.led.connectionDim = 255;

            // Prepare the first dimming
            ref.led.ConnLedState = LED_DIM_1;
            break;
        }
        case LED_DIM_1:
        {
            // Dim leds
            ref.led.connectionDim--;
            // If its kind of dim, turn it on again
            if(ref.led.connectionDim == 1)
            {
                ref.led.ConnLedState = LED_ON_2;
            }
            break;
        }
        case LED_ON_2:
        {
            // Turn LEDs on
            ref.led.connectionDim = 255;
            // Prepare the second dimming
            ref.led.ConnLedState = LED_DIM_2;
            break;
        }
        case LED_DIM_2:
        {
            // Dim leds
            ref.led.connectionDim -= 1;
            // If its off, start waiting
            if(ref.led.connectionDim == 0)
            {
                ref.led.ConnLedState = LED_OFF_WAIT;
            }
            break;
        }
        case LED_OFF_WAIT:
        {
            // Start a timer to update LEDs
            refDisarmAllLedTimers();
            os_timer_arm(&ref.tmr.ConnLed, 1000, true);

            // When it fires, start all over again
            ref.led.ConnLedState = LED_OFF;

            // And dont update the LED state this time
            return;
        }
        case LED_CONNECTED_BRIGHT:
        case LED_CONNECTED_DIM:
        {
            // Handled in refShowConnectionLedTimeout()
            break;
        }
        default:
        {
            break;
        }
    }

    // Copy the color value to all LEDs
    ets_memset(ref.led.Leds, 0, sizeof(ref.led.Leds));
    uint8_t i;
    for(i = 0; i < 6; i ++)
    {
        switch(ref.gam.difficulty)
        {
            case EASY:
            {
                // Turn on blue
                ref.led.Leds[i].b = ref.led.connectionDim;
                break;
            }
            case MEDIUM:
            {
                // Turn on green
                ref.led.Leds[i].g = ref.led.connectionDim;
                break;
            }
            case HARD:
            {
                // Turn on red
                ref.led.Leds[i].r = ref.led.connectionDim;
                break;
            }
            default:
            {
                break;
            }
        }
    }

    // Overwrite two LEDs based on the connection status
    if(ref.p2pRef.cnc.rxGameStartAck)
    {
        switch(ref.gam.difficulty)
        {
            case EASY:
            {
                // Green on blue
                ref.led.Leds[2].g = 25;
                ref.led.Leds[2].r = 0;
                ref.led.Leds[2].b = 0;
                break;
            }
            case MEDIUM:
            case HARD:
            {
                // Blue on green and red
                ref.led.Leds[2].g = 0;
                ref.led.Leds[2].r = 0;
                ref.led.Leds[2].b = 25;
                break;
            }
            default:
            {
                break;
            }
        }
    }
    if(ref.p2pRef.cnc.rxGameStartMsg)
    {
        switch(ref.gam.difficulty)
        {
            case EASY:
            {
                // Green on blue
                ref.led.Leds[4].g = 25;
                ref.led.Leds[4].r = 0;
                ref.led.Leds[4].b = 0;
                break;
            }
            case MEDIUM:
            case HARD:
            {
                // Blue on green and red
                ref.led.Leds[4].g = 0;
                ref.led.Leds[4].r = 0;
                ref.led.Leds[4].b = 25;
                break;
            }
            default:
            {
                break;
            }
        }
    }

    // Physically set the LEDs
    setLeds(ref.led.Leds, sizeof(ref.led.Leds));
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
        if(ref.led.Leds[i].r > 0)
        {
            ref.led.Leds[i].r -= 4;
        }
        ref.led.Leds[i].g = 0;
        ref.led.Leds[i].b = ref.led.Leds[i].r / 4;

    }

    // Sed LEDs according to the mode
    if (ref.gam.shouldTurnOnLeds && ref.led.Degree % DEG_PER_LED == 0)
    {
        switch(ref.gam.Action)
        {
            case ACT_BOTH:
            {
                // Make sure this value decays to exactly zero above
                ref.led.Leds[ref.led.Degree / DEG_PER_LED].r = 252;
                ref.led.Leds[ref.led.Degree / DEG_PER_LED].g = 0;
                ref.led.Leds[ref.led.Degree / DEG_PER_LED].b = 252 / 4;

                ref.led.Leds[(360 - ref.led.Degree) / DEG_PER_LED].r = 252;
                ref.led.Leds[(360 - ref.led.Degree) / DEG_PER_LED].g = 0;
                ref.led.Leds[(360 - ref.led.Degree) / DEG_PER_LED].b = 252 / 4;
                break;
            }
            case ACT_COUNTERCLOCKWISE:
            case ACT_CLOCKWISE:
            {
                ref.led.Leds[ref.led.Degree / DEG_PER_LED].r = 252;
                ref.led.Leds[ref.led.Degree / DEG_PER_LED].g = 0;
                ref.led.Leds[ref.led.Degree / DEG_PER_LED].b = 252 / 4;
                break;
            }
            default:
            {
                break;
            }
        }

        // Don't turn on LEDs past 180 degrees
        if(180 == ref.led.Degree)
        {
            ref_printf("end of pattern\r\n");
            ref.gam.shouldTurnOnLeds = false;
        }
    }

    // Move the exciter according to the mode
    switch(ref.gam.Action)
    {
        case ACT_BOTH:
        case ACT_CLOCKWISE:
        {
            ref.led.Degree += 2;
            if(ref.led.Degree > 359)
            {
                ref.led.Degree -= 360;
            }

            break;
        }
        case ACT_COUNTERCLOCKWISE:
        {
            ref.led.Degree -= 2;
            if(ref.led.Degree < 0)
            {
                ref.led.Degree += 360;
            }

            break;
        }
        default:
        {
            break;
        }
    }

    // Physically set the LEDs
    setLeds(ref.led.Leds, sizeof(ref.led.Leds));

    led_t blankLeds[6] = {{0}};
    if(false == ref.gam.shouldTurnOnLeds &&
            0 == ets_memcmp(ref.led.Leds, &blankLeds[0], sizeof(blankLeds)))
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
    if(!down)
    {
        // Ignore all button releases
        return;
    }

    if(true == ref.gam.singlePlayer &&
            NOT_DISPLAYING != ref.led.singlePlayerDisplayState)
    {
        // Single player score display, ignore button input
        return;
    }

    // If we're still connecting and no connection has started yet
    if(R_CONNECTING == ref.gameState && !ref.p2pRef.cnc.rxGameStartAck && !ref.p2pRef.cnc.rxGameStartMsg)
    {
        if(1 == button)
        {
            // Start single player mode
            ref.gam.singlePlayer = true;
            p2pSetPlayOrder(&ref.p2pRef, GOING_FIRST);
            refStartPlaying(NULL);
        }
        else if(2 == button)
        {
            // Adjust difficulty
            ref.gam.difficulty = (ref.gam.difficulty + 1) % 3;
        }
    }
    // If we're playing the game
    else if(R_PLAYING == ref.gameState && true == down)
    {
        bool success = false;
        bool failed = false;

        // And the final LED is lit
        if(ref.led.Leds[3].r > 0)
        {
            // If it's the right button for a single button mode
            if ((ACT_COUNTERCLOCKWISE == ref.gam.Action && 2 == button) ||
                    (ACT_CLOCKWISE == ref.gam.Action && 1 == button))
            {
                success = true;
            }
            // If it's the wrong button for a single button mode
            else if ((ACT_COUNTERCLOCKWISE == ref.gam.Action && 1 == button) ||
                     (ACT_CLOCKWISE == ref.gam.Action && 2 == button))
            {
                failed = true;
            }
            // Or both buttons for both
            else if(ACT_BOTH == ref.gam.Action && ((0b110 & state) == 0b110))
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
            ref_printf("Won the round, continue the game\r\n");

            const char* spdPtr;
            // Add information about the timing
            if(ref.led.Leds[3].r >= 192)
            {
                // Speed up if the button is pressed when the LED is brightest
                spdPtr = spdUp;
            }
            else if(ref.led.Leds[3].r >= 64)
            {
                // No change for the middle range
                spdPtr = spdNc;
            }
            else
            {
                // Slow down if button is pressed when the LED is dimmest
                spdPtr = spdDn;
            }

            // Single player follows different speed up/down logic
            if(ref.gam.singlePlayer)
            {
                ref.gam.singlePlayerRounds++;
                if(spdPtr == spdUp)
                {
                    // If the hit is in the first 25%, increment the speed every third hit
                    // adjust speed up after three consecutive hits
                    ref.gam.singlePlayerTopHits++;
                    if(3 == ref.gam.singlePlayerTopHits)
                    {
                        refAdjustledSpeed(false, true);
                        ref.gam.singlePlayerTopHits = 0;
                    }
                }
                if(spdPtr == spdNc || spdPtr == spdDn)
                {
                    // If the hit is in the second 75%, increment the speed immediately
                    refAdjustledSpeed(false, true);
                    // Reset the top hit count too because it just sped up
                    ref.gam.singlePlayerTopHits = 0;
                }
                refStartRound();
            }
            else
            {
                // Now waiting for a result from the other swadge
                ref.gameState = R_WAITING;

                // Clear the LEDs and stop the timer
                refDisarmAllLedTimers();
                ets_memset(ref.led.Leds, 0, sizeof(ref.led.Leds));
                setLeds(ref.led.Leds, sizeof(ref.led.Leds));

                // Send a message to the other swadge that this round was a success
                p2pSendMsg(&ref.p2pRef, "cnt", (char*)spdPtr, strlen(spdPtr), refMsgTxCbFn);
            }
        }
        else if(failed)
        {
            // Tell the other swadge
            refSendRoundLossMsg();
        }
        else
        {
            ref_printf("Neither won nor lost the round\r\n");
        }
    }
}

/**
 * @brief TODO
 *
 * @param p2p
 * @param status
 */
void ICACHE_FLASH_ATTR refMsgTxCbFn(p2pInfo* p2p __attribute__((unused)),
                                    messageStatus_t status)
{
    switch(status)
    {
        case MSG_ACKED:
        {
            ref_printf("%s MSG_ACKED\n", __func__);
            break;
        }
        case MSG_FAILED:
        {
            ref_printf("%s MSG_FAILED\n", __func__);
            break;
        }
        default:
        {
            ref_printf("%s UNKNOWN\n", __func__);
            break;
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
    ref_printf("Lost the round\r\n");
    if(ref.gam.singlePlayer)
    {
        if(ref.led.singlePlayerDisplayState == NOT_DISPLAYING)
        {
            refDisarmAllLedTimers();

            ref.led.singlePlayerDisplayState = FAIL_DISPLAY_ON_1;
            os_timer_arm(&ref.tmr.SinglePlayerRestart, RESTART_COUNT_PERIOD_MS, true);
        }
    }
    else
    {
        // Tally the loss
        ref.gam.Losses++;

        // Show the current wins & losses
        refRoundResultLed(false);

        // Send a message to that ESP that we lost the round
        // If it's acked, start a timer to reinit if another message is never received
        // If it's not acked, reinit with refRestart()
        p2pSendMsg(&ref.p2pRef, "los", NULL, 0, refMsgTxCbFn);
    }
}

/**
 * This animation displays the single player score by first drawing the tens
 * digit around the hexagon, blinking it, then drawing the ones digit around
 * the hexagon
 *
 * Once the animation is finished, the next game is started
 *
 * @param arg unused
 */
void ICACHE_FLASH_ATTR refSinglePlayerRestart(void* arg __attribute__((unused)))
{
    switch(ref.led.singlePlayerDisplayState)
    {
        case NOT_DISPLAYING:
        {
            // Not supposed to be here
            break;
        }
        case FAIL_DISPLAY_ON_1:
        {
            ets_memset(ref.led.Leds, 0, sizeof(ref.led.Leds));
            uint8_t i;
            for(i = 0; i < 6; i++)
            {
                ref.led.Leds[i].r = 0xFF;
            }

            ref.led.singlePlayerDisplayState = FAIL_DISPLAY_OFF_2;
            break;
        }
        case FAIL_DISPLAY_OFF_2:
        {
            ets_memset(ref.led.Leds, 0, sizeof(ref.led.Leds));
            ref.led.singlePlayerDisplayState = FAIL_DISPLAY_ON_3;
            break;
        }
        case FAIL_DISPLAY_ON_3:
        {
            uint8_t i;
            for(i = 0; i < 6; i++)
            {
                ref.led.Leds[i].r = 0xFF;
            }
            ref.led.singlePlayerDisplayState = FAIL_DISPLAY_OFF_4;
            break;
        }
        case FAIL_DISPLAY_OFF_4:
        {
            ets_memset(ref.led.Leds, 0, sizeof(ref.led.Leds));
            ref.led.singlePlayerDisplayState = SCORE_DISPLAY_INIT;
            break;
        }
        case SCORE_DISPLAY_INIT:
        {
            // Clear the LEDs
            ets_memset(ref.led.Leds, 0, sizeof(ref.led.Leds));
            ref.led.ledsLit = 0;

            if(ref.gam.singlePlayerRounds > 9)
            {
                // For two digit numbers, start with the tens digit
                ref.led.singlePlayerDisplayState = FIRST_DIGIT_INC;
                ref.led.digitToDisplay = ref.gam.singlePlayerRounds / 10;
            }
            else
            {
                // Otherwise just go to the ones digit
                ref.led.singlePlayerDisplayState = SECOND_DIGIT_INC;
                ref.led.digitToDisplay = ref.gam.singlePlayerRounds % 10;
            }
            break;
        }
        case FIRST_DIGIT_INC:
        {
            // Light each LED one at a time
            if(ref.led.ledsLit < ref.led.digitToDisplay)
            {
                // Light the LED
                refSinglePlayerScoreLed(ref.led.ledsLit, &digitCountFirstPrimary, &digitCountFirstSecondary);

                // keep track of how many LEDs are lit
                ref.led.ledsLit++;
            }
            else
            {
                // All LEDs are lit, blink this number
                ref.led.singlePlayerDisplayState = FIRST_DIGIT_OFF;
            }
            break;
        }
        case FIRST_DIGIT_OFF:
        {
            // Turn everything off
            ets_memset(ref.led.Leds, 0, sizeof(ref.led.Leds));
            ref.led.ledsLit = 0;

            // Then set it up to turn on
            ref.led.singlePlayerDisplayState = FIRST_DIGIT_ON;
            break;
        }
        case FIRST_DIGIT_ON:
        {
            // Reset the timer to show the final number a little longer
            os_timer_disarm(&ref.tmr.SinglePlayerRestart);
            os_timer_arm(&ref.tmr.SinglePlayerRestart, RESTART_COUNT_BLINK_PERIOD_MS, false);

            // Light the full number all at once
            while(ref.led.ledsLit < ref.led.digitToDisplay)
            {
                refSinglePlayerScoreLed(ref.led.ledsLit, &digitCountFirstPrimary, &digitCountFirstSecondary);
                // keep track of how many LEDs are lit
                ref.led.ledsLit++;
            }
            // Then turn everything off again
            ref.led.singlePlayerDisplayState = FIRST_DIGIT_OFF_2;
            break;
        }
        case FIRST_DIGIT_OFF_2:
        {
            // Reset the timer to normal speed
            os_timer_disarm(&ref.tmr.SinglePlayerRestart);
            os_timer_arm(&ref.tmr.SinglePlayerRestart, RESTART_COUNT_PERIOD_MS, true);

            // turn all LEDs off
            ets_memset(ref.led.Leds, 0, sizeof(ref.led.Leds));
            ref.led.ledsLit = 0;

            ref.led.digitToDisplay = ref.gam.singlePlayerRounds % 10;
            ref.led.singlePlayerDisplayState = SECOND_DIGIT_INC;

            break;
        }
        case SECOND_DIGIT_INC:
        {
            // Light each LED one at a time
            if(ref.led.ledsLit < ref.led.digitToDisplay)
            {
                // Light the LED
                refSinglePlayerScoreLed(ref.led.ledsLit, &digitCountSecondPrimary, &digitCountSecondSecondary);

                // keep track of how many LEDs are lit
                ref.led.ledsLit++;
            }
            else
            {
                // All LEDs are lit, blink this number
                ref.led.singlePlayerDisplayState = SECOND_DIGIT_OFF;
            }
            break;
        }
        case SECOND_DIGIT_OFF:
        {
            // Turn everything off
            ets_memset(ref.led.Leds, 0, sizeof(ref.led.Leds));
            ref.led.ledsLit = 0;

            // Then set it up to turn on
            ref.led.singlePlayerDisplayState = SECOND_DIGIT_ON;
            break;
        }
        case SECOND_DIGIT_ON:
        {
            // Reset the timer to show the final number a little longer
            os_timer_disarm(&ref.tmr.SinglePlayerRestart);
            os_timer_arm(&ref.tmr.SinglePlayerRestart, RESTART_COUNT_BLINK_PERIOD_MS, false);

            // Light the full number all at once
            while(ref.led.ledsLit < ref.led.digitToDisplay)
            {
                refSinglePlayerScoreLed(ref.led.ledsLit, &digitCountSecondPrimary, &digitCountSecondSecondary);
                // keep track of how many LEDs are lit
                ref.led.ledsLit++;
            }
            // Then turn everything off again
            ref.led.singlePlayerDisplayState = SECOND_DIGIT_OFF_2;
            break;
        }
        case SECOND_DIGIT_OFF_2:
        {
            // Reset the timer to normal speed
            os_timer_disarm(&ref.tmr.SinglePlayerRestart);
            os_timer_arm(&ref.tmr.SinglePlayerRestart, RESTART_COUNT_PERIOD_MS, true);

            // turn all LEDs off
            ets_memset(ref.led.Leds, 0, sizeof(ref.led.Leds));
            ref.led.ledsLit = 0;

            ref.led.singlePlayerDisplayState = SCORE_DISPLAY_FINISH;

            break;
        }
        case SCORE_DISPLAY_FINISH:
        {
            // Disarm the timer
            os_timer_disarm(&ref.tmr.SinglePlayerRestart);

            // For next time
            ref.led.singlePlayerDisplayState = NOT_DISPLAYING;

            // Reset and start another round
            ref.gam.singlePlayerRounds = 0;
            ref.gam.singlePlayerTopHits = 0;
            refAdjustledSpeed(true, true);
            refStartRound();
            break;
        }
        default:
        {
            break;
        }
    }

    setLeds(ref.led.Leds, sizeof(ref.led.Leds));
}

/**
 * Helper function to set LEDs around the ring when displaying the single player
 * score. Given a number 0-9, light that many LEDs around the hexagon, and
 * overdraw with the secondary color when it wraps around
 *
 * @param ledToLight     The number LED to light, maybe > 5, which wrap around with colorSecondary
 * @param colorPrimary   The color to draw [1-6]
 * @param colorSecondary The color to overdraw [7-9]
 */
void ICACHE_FLASH_ATTR refSinglePlayerScoreLed(uint8_t ledToLight, led_t* colorPrimary, led_t* colorSecondary)
{
    if(ledToLight < 6)
    {
        if(ledToLight == 0)
        {
            ets_memcpy(&ref.led.Leds[0], colorPrimary, sizeof(led_t));
        }
        else
        {
            ets_memcpy(&ref.led.Leds[6 - ledToLight], colorPrimary, sizeof(led_t));
        }
    }
    else
    {
        if(ledToLight % 6 == 0)
        {
            ets_memcpy(&ref.led.Leds[0], colorSecondary, sizeof(led_t));
        }
        else
        {
            ets_memcpy(&ref.led.Leds[12 - ledToLight], colorSecondary, sizeof(led_t));
        }
    }
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

    // Clear the LEDs
    ets_memset(ref.led.Leds, 0, sizeof(ref.led.Leds));

    // Light green for wins
    for(i = 4; i < 4 + ref.gam.Wins; i++)
    {
        // Green
        ref.led.Leds[i % 6].g = 255;
        ref.led.Leds[i % 6].r = 0;
        ref.led.Leds[i % 6].b = 0;
    }

    // Light reds for losses
    for(i = 2; i >= (3 - ref.gam.Losses); i--)
    {
        // Red
        ref.led.Leds[i].g = 0;
        ref.led.Leds[i].r = 255;
        ref.led.Leds[i].b = 0;
    }

    // Push out LED data
    refDisarmAllLedTimers();
    setLeds(ref.led.Leds, sizeof(ref.led.Leds));

    // Set up the next round based on the winner
    if(roundWinner)
    {
        ref.gameState = R_SHOW_GAME_RESULT;
        p2pSetPlayOrder(&ref.p2pRef, GOING_FIRST);
    }
    else
    {
        // Set ref.gameState here to R_WAITING to make sure a message isn't missed
        ref.gameState = R_WAITING;
        p2pSetPlayOrder(&ref.p2pRef, GOING_SECOND);
        ref.gam.receiveFirstMsg = false;
    }

    // Call refStartPlaying in 3 seconds
    os_timer_arm(&ref.tmr.StartPlaying, 3000, false);
}

/**
 * Adjust the speed of the game, or reset it to the default value for this
 * difficulty
 *
 * @param reset true to reset to the starting value, up will be ignored
 * @param up    if reset is false, if this is true, speed up, otherwise slow down
 */
void ICACHE_FLASH_ATTR refAdjustledSpeed(bool reset, bool up)
{
    // If you're in single player, ignore any speed downs
    if(ref.gam.singlePlayer && up == false)
    {
        return;
    }

    if(reset)
    {
        switch(ref.gam.difficulty)
        {
            case EASY:
            {
                ref.gam.ledPeriodMs = LED_TIMER_MS_STARTING_EASY;
                break;
            }
            case MEDIUM:
            {
                ref.gam.ledPeriodMs = LED_TIMER_MS_STARTING_MEDIUM;
                break;
            }
            case HARD:
            {
                ref.gam.ledPeriodMs = LED_TIMER_MS_STARTING_HARD;
                break;
            }
            default:
            {
                break;
            }
        }
    }
    else if (GOING_SECOND == p2pGetPlayOrder(&ref.p2pRef) && false == ref.gam.receiveFirstMsg)
    {
        // If going second, ignore the first up/dn from the first player
        ref.gam.receiveFirstMsg = true;
    }
    else if(up)
    {
        switch(ref.gam.difficulty)
        {
            case EASY:
            case MEDIUM:
            {
                ref.gam.ledPeriodMs--;
                break;
            }
            case HARD:
            {
                ref.gam.ledPeriodMs -= 2;
                break;
            }
            default:
            {
                break;
            }
        }
        // Anything less than a 3ms period is impossible...
        if(ref.gam.ledPeriodMs < 3)
        {
            ref.gam.ledPeriodMs = 3;
        }
    }
    else
    {
        switch(ref.gam.difficulty)
        {
            case EASY:
            case MEDIUM:
            {
                ref.gam.ledPeriodMs++;
                break;
            }
            case HARD:
            {
                ref.gam.ledPeriodMs += 2;
                break;
            }
            default:
            {
                break;
            }
        }
    }
}
