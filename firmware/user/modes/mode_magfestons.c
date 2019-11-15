/*
 * mode_magfestons.c
 *  bbkiwi
 *
 * modified from mode_reflector_game
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
#include "mode_magfestons.h"
#include "custom_commands.h"
#include "buttons.h"
#include "p2pConnection.h"

/*============================================================================
 * Defines
 *==========================================================================*/

#define MFT_DEBUG_PRINT
#ifdef MFT_DEBUG_PRINT
    #include <stdlib.h>
    #define mft_printf(...) os_printf(__VA_ARGS__)
#else
    #define mft_printf(...)
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
} mftlectorGameState_t;

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
void ICACHE_FLASH_ATTR mftInit(void);
void ICACHE_FLASH_ATTR mftDeinit(void);
void ICACHE_FLASH_ATTR mftButton(uint8_t state, int button, int down);
void ICACHE_FLASH_ATTR mftRecvCb(uint8_t* mac_addr, uint8_t* data, uint8_t len, uint8_t rssi);
void ICACHE_FLASH_ATTR mftSendCb(uint8_t* mac_addr, mt_tx_status status);

// Helper function
void ICACHE_FLASH_ATTR mftSinglePlayerRestart(void* arg __attribute__((unused)));
void ICACHE_FLASH_ATTR mftSinglePlayerScoreLed(uint8_t ledToLight, led_t* colorPrimary, led_t* colorSecondary);
void ICACHE_FLASH_ATTR mftConnectionCallback(p2pInfo* p2p, connectionEvt_t event);
void ICACHE_FLASH_ATTR mftMsgCallbackFn(p2pInfo* p2p, char* msg, uint8_t* payload, uint8_t len);
void ICACHE_FLASH_ATTR mftMsgTxCbFn(p2pInfo* p2p, messageStatus_t status);

// Game functions
void ICACHE_FLASH_ATTR mftStartPlaying(void* arg __attribute__((unused)));
void ICACHE_FLASH_ATTR mftStartRound(void);
void ICACHE_FLASH_ATTR mftSendRoundLossMsg(void);
void ICACHE_FLASH_ATTR mftAdjustledSpeed(bool reset, bool up);

// LED Functions
void ICACHE_FLASH_ATTR mftDisarmAllLedTimers(void);
void ICACHE_FLASH_ATTR mftConnLedTimeout(void* arg __attribute__((unused)));
void ICACHE_FLASH_ATTR mftShowConnectionLedTimeout(void* arg __attribute__((unused)));
void ICACHE_FLASH_ATTR mftGameLedTimeout(void* arg __attribute__((unused)));
void ICACHE_FLASH_ATTR mftRoundResultLed(bool);

/*============================================================================
 * Variables
 *==========================================================================*/

swadgeMode magfestonsMode =
{
    .modeName = "magfestons",
    .fnEnterMode = mftInit,
    .fnExitMode = mftDeinit,
    .fnButtonCallback = mftButton,
    .wifiMode = ESP_NOW,
    .fnEspNowRecvCb = mftRecvCb,
    .fnEspNowSendCb = mftSendCb,
};

const char spdUp[] = "up";
const char spdDn[] = "dn";
const char spdNc[] = "nc";

struct
{
    mftlectorGameState_t gameState;

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

    p2pInfo p2pMft;
} mft;

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

void ICACHE_FLASH_ATTR mftConnectionCallback(p2pInfo* p2p __attribute__((unused)), connectionEvt_t event)
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
            mft.gameState = R_SHOW_CONNECTION;

            ets_memset(mft.led.Leds, 0, sizeof(mft.led.Leds));
            mft.led.ConnLedState = LED_CONNECTED_BRIGHT;

            mftDisarmAllLedTimers();
            // 6ms * ~500 steps == 3s animation
            os_timer_arm(&mft.tmr.ShowConnectionLed, 6, true);
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
void ICACHE_FLASH_ATTR mftInit(void)
{
    mft_printf("%s\r\n", __func__);

    // Enable button debounce for consistent 1p/2p and difficulty config
    enableDebounce(true);

    // Make sure everything is zero!
    ets_memset(&mft, 0, sizeof(mft));

    p2pInitialize(&mft.p2pMft, "mft", mftConnectionCallback, mftMsgCallbackFn, 10);

    // Set up a timer for showing a successful connection, don't start it
    os_timer_disarm(&mft.tmr.ShowConnectionLed);
    os_timer_setfn(&mft.tmr.ShowConnectionLed, mftShowConnectionLedTimeout, NULL);

    // Set up a timer for showing the game, don't start it
    os_timer_disarm(&mft.tmr.GameLed);
    os_timer_setfn(&mft.tmr.GameLed, mftGameLedTimeout, NULL);

    // Set up a timer for starting the next round, don't start it
    os_timer_disarm(&mft.tmr.StartPlaying);
    os_timer_setfn(&mft.tmr.StartPlaying, mftStartPlaying, NULL);

    // Set up a timer to update LEDs, start it
    os_timer_disarm(&mft.tmr.ConnLed);
    os_timer_setfn(&mft.tmr.ConnLed, mftConnLedTimeout, NULL);

    // Set up a timer to restart after failure. don't start it
    os_timer_disarm(&mft.tmr.SinglePlayerRestart);
    os_timer_setfn(&mft.tmr.SinglePlayerRestart, mftSinglePlayerRestart, NULL);

    p2pStartConnection(&mft.p2pMft);
    os_timer_arm(&mft.tmr.ConnLed, 1, true);
}

/**
 * Clean up all timers
 */
void ICACHE_FLASH_ATTR mftDeinit(void)
{
    mft_printf("%s\r\n", __func__);

    p2pDeinit(&mft.p2pMft);
    os_timer_disarm(&mft.tmr.StartPlaying);
    mftDisarmAllLedTimers();
}

/**
 * Disarm any timers which control LEDs
 */
void ICACHE_FLASH_ATTR mftDisarmAllLedTimers(void)
{
    os_timer_disarm(&mft.tmr.ConnLed);
    os_timer_disarm(&mft.tmr.ShowConnectionLed);
    os_timer_disarm(&mft.tmr.GameLed);
    os_timer_disarm(&mft.tmr.SinglePlayerRestart);
}

/**
 * This is called after an attempted transmission. If it was successful, and the
 * message should be acked, start a retry timer. If it wasn't successful, just
 * try again
 *
 * @param mac_addr
 * @param status
 */
void ICACHE_FLASH_ATTR mftSendCb(uint8_t* mac_addr __attribute__((unused)),
                                 mt_tx_status status)
{
    p2pSendCb(&mft.p2pMft, mac_addr, status);
}

/**
 * This is called whenever an ESP NOW packet is received
 *
 * @param mac_addr The MAC of the swadge that sent the data
 * @param data     The data
 * @param len      The length of the data
 */
void ICACHE_FLASH_ATTR mftRecvCb(uint8_t* mac_addr, uint8_t* data, uint8_t len, uint8_t rssi)
{
    p2pRecvCb(&mft.p2pMft, mac_addr, data, len, rssi);
}

/**
 * @brief
 *
 * @param msg
 * @param payload
 * @param len
 */
void ICACHE_FLASH_ATTR mftMsgCallbackFn(p2pInfo* p2p __attribute__((unused)), char* msg, uint8_t* payload,
                                        uint8_t len __attribute__((unused)))
{
    if(len > 0)
    {
        mft_printf("%s %s %s\n", __func__, msg, payload);
    }
    else
    {
        mft_printf("%s %s\n", __func__, msg);
    }

    switch(mft.gameState)
    {
        case R_CONNECTING:
            break;
        case R_WAITING:
        {
            // Received a message that the other swadge lost
            if(0 == ets_memcmp(msg, "los", 3))
            {
                // The other swadge lost, so chalk a win!
                mft.gam.Wins++;

                // Display the win
                mftRoundResultLed(true);
            }
            if(0 == ets_memcmp(msg, "cnt", 3))
            {
                // Get faster or slower based on the other swadge's timing
                if(0 == ets_memcmp(payload, spdUp, ets_strlen(spdUp)))
                {
                    mftAdjustledSpeed(false, true);
                }
                else if(0 == ets_memcmp(payload, spdDn, ets_strlen(spdDn)))
                {
                    mftAdjustledSpeed(false, false);
                }

                mftStartRound();
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
void ICACHE_FLASH_ATTR mftShowConnectionLedTimeout(void* arg __attribute__((unused)) )
{
    uint8_t currBrightness = mft.led.Leds[0].r;
    switch(mft.led.ConnLedState)
    {
        case LED_CONNECTED_BRIGHT:
        {
            currBrightness++;
            if(currBrightness == 0xFF)
            {
                mft.led.ConnLedState = LED_CONNECTED_DIM;
            }
            break;
        }
        case LED_CONNECTED_DIM:
        {
            currBrightness--;
            if(currBrightness == 0x00)
            {
                mftStartPlaying(NULL);
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
    ets_memset(mft.led.Leds, currBrightness, sizeof(mft.led.Leds));
    setLeds(mft.led.Leds, sizeof(mft.led.Leds));
}

/**
 * This is called after connection is all done. Start the game!
 *
 * @param arg unused
 */
void ICACHE_FLASH_ATTR mftStartPlaying(void* arg __attribute__((unused)))
{
    mft_printf("%s\r\n", __func__);

    // Disable button debounce for minimum latency
    enableDebounce(false);

    // Turn off the LEDs
    mftDisarmAllLedTimers();
    ets_memset(mft.led.Leds, 0, sizeof(mft.led.Leds));
    setLeds(mft.led.Leds, sizeof(mft.led.Leds));

    // Reset the LED timer to the default speed
    mftAdjustledSpeed(true, true);

    // Check for match end
    mft_printf("wins: %d, losses %d\r\n", mft.gam.Wins, mft.gam.Losses);
    if(mft.gam.Wins == 3 || mft.gam.Losses == 3)
    {
        // Tally match win in SPI flash
        if(mft.gam.Wins == 3)
        {
            //incrementMftGameWins();
        }

        // Match over, reset everything
        mftDeinit();
        mftInit();
    }
    else if((GOING_FIRST == p2pGetPlayOrder(&mft.p2pMft)) || mft.gam.singlePlayer)
    {
        mft.gameState = R_PLAYING;
        os_printf("GOING FIRST %s line %d\n", __func__, __LINE__);
        // Start playing
        mftStartRound();
    }
    else if(GOING_SECOND == p2pGetPlayOrder(&mft.p2pMft))
    {
        os_printf("GOING SECOND %s line %d\n", __func__, __LINE__);
        mft.gameState = R_WAITING;

        mft.gam.receiveFirstMsg = false;

        // TODO Start a timer to reinit if we never receive a result (disconnect)
        // mftStartRestartTimer(NULL);
    }
}

/**
 * Start a round of the game by picking a random action and starting
 * mftGameLedTimeout()
 */
void ICACHE_FLASH_ATTR mftStartRound(void)
{
    os_printf("%s line %d\n", __func__, __LINE__);
    mft.gameState = R_PLAYING;

    // pick a random game action
    mft.gam.Action = os_random() % 3;

    // Set the LED's starting angle
    switch(mft.gam.Action)
    {
        case ACT_CLOCKWISE:
        {
            mft_printf("ACT_CLOCKWISE\r\n");
            mft.led.Degree = 300;
            break;
        }
        case ACT_COUNTERCLOCKWISE:
        {
            mft_printf("ACT_COUNTERCLOCKWISE\r\n");
            mft.led.Degree = 60;
            break;
        }
        case ACT_BOTH:
        {
            mft_printf("ACT_BOTH\r\n");
            mft.led.Degree = 0;
            break;
        }
        default:
        {
            break;
        }
    }
    mft.gam.shouldTurnOnLeds = true;

    // Clear the LEDs first
    mftDisarmAllLedTimers();
    ets_memset(mft.led.Leds, 0, sizeof(mft.led.Leds));
    setLeds(mft.led.Leds, sizeof(mft.led.Leds));
    // Then set the game in motion
    os_timer_arm(&mft.tmr.GameLed, mft.gam.ledPeriodMs, true);
}

/**

 * Called every 4ms, this updates the LEDs during connection
 */
void ICACHE_FLASH_ATTR mftConnLedTimeout(void* arg __attribute__((unused)))
{
    switch(mft.led.ConnLedState)
    {
        case LED_OFF:
        {
            // Reset this timer to LED_PERIOD_MS
            mftDisarmAllLedTimers();
            os_timer_arm(&mft.tmr.ConnLed, 4, true);

            mft.led.connectionDim = 0;
            ets_memset(mft.led.Leds, 0, sizeof(mft.led.Leds));

            mft.led.ConnLedState = LED_ON_1;
            break;
        }
        case LED_ON_1:
        {
            // Turn LEDs on
            mft.led.connectionDim = 255;

            // Prepare the first dimming
            mft.led.ConnLedState = LED_DIM_1;
            break;
        }
        case LED_DIM_1:
        {
            // Dim leds
            mft.led.connectionDim--;
            // If its kind of dim, turn it on again
            if(mft.led.connectionDim == 1)
            {
                mft.led.ConnLedState = LED_ON_2;
            }
            break;
        }
        case LED_ON_2:
        {
            // Turn LEDs on
            mft.led.connectionDim = 255;
            // Prepare the second dimming
            mft.led.ConnLedState = LED_DIM_2;
            break;
        }
        case LED_DIM_2:
        {
            // Dim leds
            mft.led.connectionDim -= 1;
            // If its off, start waiting
            if(mft.led.connectionDim == 0)
            {
                mft.led.ConnLedState = LED_OFF_WAIT;
            }
            break;
        }
        case LED_OFF_WAIT:
        {
            // Start a timer to update LEDs
            mftDisarmAllLedTimers();
            os_timer_arm(&mft.tmr.ConnLed, 1000, true);

            // When it fires, start all over again
            mft.led.ConnLedState = LED_OFF;

            // And dont update the LED state this time
            return;
        }
        case LED_CONNECTED_BRIGHT:
        case LED_CONNECTED_DIM:
        {
            // Handled in mftShowConnectionLedTimeout()
            break;
        }
        default:
        {
            break;
        }
    }

    // Copy the color value to all LEDs
    ets_memset(mft.led.Leds, 0, sizeof(mft.led.Leds));
    uint8_t i;
    for(i = 0; i < 6; i ++)
    {
        switch(mft.gam.difficulty)
        {
            case EASY:
            {
                // Turn on blue
                mft.led.Leds[i].b = mft.led.connectionDim;
                break;
            }
            case MEDIUM:
            {
                // Turn on green
                mft.led.Leds[i].g = mft.led.connectionDim;
                break;
            }
            case HARD:
            {
                // Turn on red
                mft.led.Leds[i].r = mft.led.connectionDim;
                break;
            }
            default:
            {
                break;
            }
        }
    }

    // Overwrite two LEDs based on the connection status
    if(mft.p2pMft.cnc.rxGameStartAck)
    {
        switch(mft.gam.difficulty)
        {
            case EASY:
            {
                // Green on blue
                mft.led.Leds[2].g = 25;
                mft.led.Leds[2].r = 0;
                mft.led.Leds[2].b = 0;
                break;
            }
            case MEDIUM:
            case HARD:
            {
                // Blue on green and red
                mft.led.Leds[2].g = 0;
                mft.led.Leds[2].r = 0;
                mft.led.Leds[2].b = 25;
                break;
            }
            default:
            {
                break;
            }
        }
    }
    if(mft.p2pMft.cnc.rxGameStartMsg)
    {
        switch(mft.gam.difficulty)
        {
            case EASY:
            {
                // Green on blue
                mft.led.Leds[4].g = 25;
                mft.led.Leds[4].r = 0;
                mft.led.Leds[4].b = 0;
                break;
            }
            case MEDIUM:
            case HARD:
            {
                // Blue on green and red
                mft.led.Leds[4].g = 0;
                mft.led.Leds[4].r = 0;
                mft.led.Leds[4].b = 25;
                break;
            }
            default:
            {
                break;
            }
        }
    }

    // Physically set the LEDs
    setLeds(mft.led.Leds, sizeof(mft.led.Leds));
}

/**
 * Called every 100ms, this updates the LEDs during the game
 */
void ICACHE_FLASH_ATTR mftGameLedTimeout(void* arg __attribute__((unused)))
{
    // Decay all LEDs
    uint8_t i;
    for(i = 0; i < 6; i++)
    {
        if(mft.led.Leds[i].r > 0)
        {
            mft.led.Leds[i].r -= 4;
        }
        mft.led.Leds[i].g = 0;
        mft.led.Leds[i].b = mft.led.Leds[i].r / 4;

    }

    // Sed LEDs according to the mode
    if (mft.gam.shouldTurnOnLeds && mft.led.Degree % DEG_PER_LED == 0)
    {
        switch(mft.gam.Action)
        {
            case ACT_BOTH:
            {
                // Make sure this value decays to exactly zero above
                mft.led.Leds[mft.led.Degree / DEG_PER_LED].r = 252;
                mft.led.Leds[mft.led.Degree / DEG_PER_LED].g = 0;
                mft.led.Leds[mft.led.Degree / DEG_PER_LED].b = 252 / 4;

                mft.led.Leds[(360 - mft.led.Degree) / DEG_PER_LED].r = 252;
                mft.led.Leds[(360 - mft.led.Degree) / DEG_PER_LED].g = 0;
                mft.led.Leds[(360 - mft.led.Degree) / DEG_PER_LED].b = 252 / 4;
                break;
            }
            case ACT_COUNTERCLOCKWISE:
            case ACT_CLOCKWISE:
            {
                mft.led.Leds[mft.led.Degree / DEG_PER_LED].r = 252;
                mft.led.Leds[mft.led.Degree / DEG_PER_LED].g = 0;
                mft.led.Leds[mft.led.Degree / DEG_PER_LED].b = 252 / 4;
                break;
            }
            default:
            {
                break;
            }
        }

        // Don't turn on LEDs past 180 degrees
        if(180 == mft.led.Degree)
        {
            mft_printf("end of pattern\r\n");
            mft.gam.shouldTurnOnLeds = false;
        }
    }

    // Move the exciter according to the mode
    switch(mft.gam.Action)
    {
        case ACT_BOTH:
        case ACT_CLOCKWISE:
        {
            mft.led.Degree += 2;
            if(mft.led.Degree > 359)
            {
                mft.led.Degree -= 360;
            }

            break;
        }
        case ACT_COUNTERCLOCKWISE:
        {
            mft.led.Degree -= 2;
            if(mft.led.Degree < 0)
            {
                mft.led.Degree += 360;
            }

            break;
        }
        default:
        {
            break;
        }
    }

    // Physically set the LEDs
    setLeds(mft.led.Leds, sizeof(mft.led.Leds));

    led_t blankLeds[6] = {{0}};
    if(false == mft.gam.shouldTurnOnLeds &&
            0 == ets_memcmp(mft.led.Leds, &blankLeds[0], sizeof(blankLeds)))
    {
        // If the last LED is off, the user missed the window of opportunity
        mftSendRoundLossMsg();
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
void ICACHE_FLASH_ATTR mftButton(uint8_t state, int button, int down)
{
    if(!down)
    {
        // Ignore all button releases
        return;
    }

    if(true == mft.gam.singlePlayer &&
            NOT_DISPLAYING != mft.led.singlePlayerDisplayState)
    {
        // Single player score display, ignore button input
        return;
    }

    // If we're still connecting and no connection has started yet
    if(R_CONNECTING == mft.gameState && !mft.p2pMft.cnc.rxGameStartAck && !mft.p2pMft.cnc.rxGameStartMsg)
    {
        //LEFT
        if(1 == button)
        {
            // Start single player mode
            os_printf("left connecting but none started %s line %d\n", __func__, __LINE__);
            mft.gam.singlePlayer = true;
            p2pSetPlayOrder(&mft.p2pMft, GOING_FIRST);
            mftStartPlaying(NULL);
        }
        else if(2 == button)
        {
            // Adjust difficulty
            mft.gam.difficulty = (mft.gam.difficulty + 1) % 3;
            os_printf("right connecting but none started change difficulty to %d %s line %d\n", mft.gam.difficulty, __func__,
                      __LINE__);
        }
    }
    // If we're playing the game
    else if(R_PLAYING == mft.gameState && true == down)
    {
        bool success = false;
        bool failed = false;

        // And the final LED is lit
        if(mft.led.Leds[3].r > 0)
        {
            // If it's the right button for a single button mode
            if ((ACT_COUNTERCLOCKWISE == mft.gam.Action && 2 == button) ||
                    (ACT_CLOCKWISE == mft.gam.Action && 1 == button))
            {
                success = true;
            }
            // If it's the wrong button for a single button mode
            else if ((ACT_COUNTERCLOCKWISE == mft.gam.Action && 1 == button) ||
                     (ACT_CLOCKWISE == mft.gam.Action && 2 == button))
            {
                failed = true;
            }
            // Or both buttons for both
            else if(ACT_BOTH == mft.gam.Action && ((0b110 & state) == 0b110))
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
            mft_printf("Won the round, continue the game\r\n");

            const char* spdPtr;
            // Add information about the timing
            if(mft.led.Leds[3].r >= 192)
            {
                // Speed up if the button is pressed when the LED is brightest
                spdPtr = spdUp;
            }
            else if(mft.led.Leds[3].r >= 64)
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
            if(mft.gam.singlePlayer)
            {
                mft.gam.singlePlayerRounds++;
                if(spdPtr == spdUp)
                {
                    // If the hit is in the first 25%, increment the speed every third hit
                    // adjust speed up after three consecutive hits
                    mft.gam.singlePlayerTopHits++;
                    if(3 == mft.gam.singlePlayerTopHits)
                    {
                        mftAdjustledSpeed(false, true);
                        mft.gam.singlePlayerTopHits = 0;
                    }
                }
                if(spdPtr == spdNc || spdPtr == spdDn)
                {
                    // If the hit is in the second 75%, increment the speed immediately
                    mftAdjustledSpeed(false, true);
                    // Reset the top hit count too because it just sped up
                    mft.gam.singlePlayerTopHits = 0;
                }
                mftStartRound();
            }
            else
            {
                // Now waiting for a result from the other swadge
                mft.gameState = R_WAITING;

                // Clear the LEDs and stop the timer
                mftDisarmAllLedTimers();
                ets_memset(mft.led.Leds, 0, sizeof(mft.led.Leds));
                setLeds(mft.led.Leds, sizeof(mft.led.Leds));

                // Send a message to the other swadge that this round was a success
                p2pSendMsg(&mft.p2pMft, "cnt", (char*)spdPtr, strlen(spdPtr), mftMsgTxCbFn);
            }
        }
        else if(failed)
        {
            // Tell the other swadge
            mftSendRoundLossMsg();
        }
        else
        {
            mft_printf("Neither won nor lost the round\r\n");
        }
    }
}

/**
 * @brief TODO
 *
 * @param p2p
 * @param status
 */
void ICACHE_FLASH_ATTR mftMsgTxCbFn(p2pInfo* p2p __attribute__((unused)),
                                    messageStatus_t status)
{
    switch(status)
    {
        case MSG_ACKED:
        {
            mft_printf("%s MSG_ACKED\n", __func__);
            break;
        }
        case MSG_FAILED:
        {
            mft_printf("%s MSG_FAILED\n", __func__);
            break;
        }
        default:
        {
            mft_printf("%s UNKNOWN\n", __func__);
            break;
        }
    }
}

/**
 * This is called when a round is lost. It tallies the loss, calls
 * mftRoundResultLed() to display the wins/losses and set up the
 * potential next round, and sends a message to the other swadge
 * that the round was lost and
 *
 * Send a round loss message to the other swadge
 */
void ICACHE_FLASH_ATTR mftSendRoundLossMsg(void)
{
    mft_printf("Lost the round\r\n");
    if(mft.gam.singlePlayer)
    {
        if(mft.led.singlePlayerDisplayState == NOT_DISPLAYING)
        {
            mftDisarmAllLedTimers();

            mft.led.singlePlayerDisplayState = FAIL_DISPLAY_ON_1;
            os_timer_arm(&mft.tmr.SinglePlayerRestart, RESTART_COUNT_PERIOD_MS, true);
        }
    }
    else
    {
        // Tally the loss
        mft.gam.Losses++;

        // Show the current wins & losses
        mftRoundResultLed(false);

        // Send a message to that ESP that we lost the round
        // If it's acked, start a timer to reinit if another message is never received
        // If it's not acked, reinit with mftRestart()
        p2pSendMsg(&mft.p2pMft, "los", NULL, 0, mftMsgTxCbFn);
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
void ICACHE_FLASH_ATTR mftSinglePlayerRestart(void* arg __attribute__((unused)))
{
    switch(mft.led.singlePlayerDisplayState)
    {
        case NOT_DISPLAYING:
        {
            // Not supposed to be here
            break;
        }
        case FAIL_DISPLAY_ON_1:
        {
            ets_memset(mft.led.Leds, 0, sizeof(mft.led.Leds));
            uint8_t i;
            for(i = 0; i < 6; i++)
            {
                mft.led.Leds[i].r = 0xFF;
            }

            mft.led.singlePlayerDisplayState = FAIL_DISPLAY_OFF_2;
            break;
        }
        case FAIL_DISPLAY_OFF_2:
        {
            ets_memset(mft.led.Leds, 0, sizeof(mft.led.Leds));
            mft.led.singlePlayerDisplayState = FAIL_DISPLAY_ON_3;
            break;
        }
        case FAIL_DISPLAY_ON_3:
        {
            uint8_t i;
            for(i = 0; i < 6; i++)
            {
                mft.led.Leds[i].r = 0xFF;
            }
            mft.led.singlePlayerDisplayState = FAIL_DISPLAY_OFF_4;
            break;
        }
        case FAIL_DISPLAY_OFF_4:
        {
            ets_memset(mft.led.Leds, 0, sizeof(mft.led.Leds));
            mft.led.singlePlayerDisplayState = SCORE_DISPLAY_INIT;
            break;
        }
        case SCORE_DISPLAY_INIT:
        {
            // Clear the LEDs
            ets_memset(mft.led.Leds, 0, sizeof(mft.led.Leds));
            mft.led.ledsLit = 0;

            if(mft.gam.singlePlayerRounds > 9)
            {
                // For two digit numbers, start with the tens digit
                mft.led.singlePlayerDisplayState = FIRST_DIGIT_INC;
                mft.led.digitToDisplay = mft.gam.singlePlayerRounds / 10;
            }
            else
            {
                // Otherwise just go to the ones digit
                mft.led.singlePlayerDisplayState = SECOND_DIGIT_INC;
                mft.led.digitToDisplay = mft.gam.singlePlayerRounds % 10;
            }
            break;
        }
        case FIRST_DIGIT_INC:
        {
            // Light each LED one at a time
            if(mft.led.ledsLit < mft.led.digitToDisplay)
            {
                // Light the LED
                mftSinglePlayerScoreLed(mft.led.ledsLit, &digitCountFirstPrimary, &digitCountFirstSecondary);

                // keep track of how many LEDs are lit
                mft.led.ledsLit++;
            }
            else
            {
                // All LEDs are lit, blink this number
                mft.led.singlePlayerDisplayState = FIRST_DIGIT_OFF;
            }
            break;
        }
        case FIRST_DIGIT_OFF:
        {
            // Turn everything off
            ets_memset(mft.led.Leds, 0, sizeof(mft.led.Leds));
            mft.led.ledsLit = 0;

            // Then set it up to turn on
            mft.led.singlePlayerDisplayState = FIRST_DIGIT_ON;
            break;
        }
        case FIRST_DIGIT_ON:
        {
            // Reset the timer to show the final number a little longer
            os_timer_disarm(&mft.tmr.SinglePlayerRestart);
            os_timer_arm(&mft.tmr.SinglePlayerRestart, RESTART_COUNT_BLINK_PERIOD_MS, false);

            // Light the full number all at once
            while(mft.led.ledsLit < mft.led.digitToDisplay)
            {
                mftSinglePlayerScoreLed(mft.led.ledsLit, &digitCountFirstPrimary, &digitCountFirstSecondary);
                // keep track of how many LEDs are lit
                mft.led.ledsLit++;
            }
            // Then turn everything off again
            mft.led.singlePlayerDisplayState = FIRST_DIGIT_OFF_2;
            break;
        }
        case FIRST_DIGIT_OFF_2:
        {
            // Reset the timer to normal speed
            os_timer_disarm(&mft.tmr.SinglePlayerRestart);
            os_timer_arm(&mft.tmr.SinglePlayerRestart, RESTART_COUNT_PERIOD_MS, true);

            // turn all LEDs off
            ets_memset(mft.led.Leds, 0, sizeof(mft.led.Leds));
            mft.led.ledsLit = 0;

            mft.led.digitToDisplay = mft.gam.singlePlayerRounds % 10;
            mft.led.singlePlayerDisplayState = SECOND_DIGIT_INC;

            break;
        }
        case SECOND_DIGIT_INC:
        {
            // Light each LED one at a time
            if(mft.led.ledsLit < mft.led.digitToDisplay)
            {
                // Light the LED
                mftSinglePlayerScoreLed(mft.led.ledsLit, &digitCountSecondPrimary, &digitCountSecondSecondary);

                // keep track of how many LEDs are lit
                mft.led.ledsLit++;
            }
            else
            {
                // All LEDs are lit, blink this number
                mft.led.singlePlayerDisplayState = SECOND_DIGIT_OFF;
            }
            break;
        }
        case SECOND_DIGIT_OFF:
        {
            // Turn everything off
            ets_memset(mft.led.Leds, 0, sizeof(mft.led.Leds));
            mft.led.ledsLit = 0;

            // Then set it up to turn on
            mft.led.singlePlayerDisplayState = SECOND_DIGIT_ON;
            break;
        }
        case SECOND_DIGIT_ON:
        {
            // Reset the timer to show the final number a little longer
            os_timer_disarm(&mft.tmr.SinglePlayerRestart);
            os_timer_arm(&mft.tmr.SinglePlayerRestart, RESTART_COUNT_BLINK_PERIOD_MS, false);

            // Light the full number all at once
            while(mft.led.ledsLit < mft.led.digitToDisplay)
            {
                mftSinglePlayerScoreLed(mft.led.ledsLit, &digitCountSecondPrimary, &digitCountSecondSecondary);
                // keep track of how many LEDs are lit
                mft.led.ledsLit++;
            }
            // Then turn everything off again
            mft.led.singlePlayerDisplayState = SECOND_DIGIT_OFF_2;
            break;
        }
        case SECOND_DIGIT_OFF_2:
        {
            // Reset the timer to normal speed
            os_timer_disarm(&mft.tmr.SinglePlayerRestart);
            os_timer_arm(&mft.tmr.SinglePlayerRestart, RESTART_COUNT_PERIOD_MS, true);

            // turn all LEDs off
            ets_memset(mft.led.Leds, 0, sizeof(mft.led.Leds));
            mft.led.ledsLit = 0;

            mft.led.singlePlayerDisplayState = SCORE_DISPLAY_FINISH;

            break;
        }
        case SCORE_DISPLAY_FINISH:
        {
            // Disarm the timer
            os_timer_disarm(&mft.tmr.SinglePlayerRestart);

            // For next time
            mft.led.singlePlayerDisplayState = NOT_DISPLAYING;

            // Reset and start another round
            mft.gam.singlePlayerRounds = 0;
            mft.gam.singlePlayerTopHits = 0;
            mftAdjustledSpeed(true, true);
            mftStartRound();
            break;
        }
        default:
        {
            break;
        }
    }

    setLeds(mft.led.Leds, sizeof(mft.led.Leds));
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
void ICACHE_FLASH_ATTR mftSinglePlayerScoreLed(uint8_t ledToLight, led_t* colorPrimary, led_t* colorSecondary)
{
    if(ledToLight < 6)
    {
        if(ledToLight == 0)
        {
            ets_memcpy(&mft.led.Leds[0], colorPrimary, sizeof(led_t));
        }
        else
        {
            ets_memcpy(&mft.led.Leds[6 - ledToLight], colorPrimary, sizeof(led_t));
        }
    }
    else
    {
        if(ledToLight % 6 == 0)
        {
            ets_memcpy(&mft.led.Leds[0], colorSecondary, sizeof(led_t));
        }
        else
        {
            ets_memcpy(&mft.led.Leds[12 - ledToLight], colorSecondary, sizeof(led_t));
        }
    }
}

/**
 * Show the wins and losses
 *
 * @param roundWinner true if this swadge was a winner, false if the other
 *                    swadge won
 */
void ICACHE_FLASH_ATTR mftRoundResultLed(bool roundWinner)
{
    sint8_t i;

    // Clear the LEDs
    ets_memset(mft.led.Leds, 0, sizeof(mft.led.Leds));

    // Light green for wins
    for(i = 4; i < 4 + mft.gam.Wins; i++)
    {
        // Green
        mft.led.Leds[i % 6].g = 255;
        mft.led.Leds[i % 6].r = 0;
        mft.led.Leds[i % 6].b = 0;
    }

    // Light reds for losses
    for(i = 2; i >= (3 - mft.gam.Losses); i--)
    {
        // Red
        mft.led.Leds[i].g = 0;
        mft.led.Leds[i].r = 255;
        mft.led.Leds[i].b = 0;
    }

    // Push out LED data
    mftDisarmAllLedTimers();
    setLeds(mft.led.Leds, sizeof(mft.led.Leds));

    // Set up the next round based on the winner
    if(roundWinner)
    {
        mft.gameState = R_SHOW_GAME_RESULT;
        p2pSetPlayOrder(&mft.p2pMft, GOING_FIRST);
    }
    else
    {
        // Set mft.gameState here to R_WAITING to make sure a message isn't missed
        mft.gameState = R_WAITING;
        p2pSetPlayOrder(&mft.p2pMft, GOING_SECOND);
        mft.gam.receiveFirstMsg = false;
    }

    // Call mftStartPlaying in 3 seconds
    os_timer_arm(&mft.tmr.StartPlaying, 3000, false);
}

/**
 * Adjust the speed of the game, or reset it to the default value for this
 * difficulty
 *
 * @param reset true to reset to the starting value, up will be ignored
 * @param up    if reset is false, if this is true, speed up, otherwise slow down
 */
void ICACHE_FLASH_ATTR mftAdjustledSpeed(bool reset, bool up)
{
    // If you're in single player, ignore any speed downs
    if(mft.gam.singlePlayer && up == false)
    {
        return;
    }

    if(reset)
    {
        switch(mft.gam.difficulty)
        {
            case EASY:
            {
                mft.gam.ledPeriodMs = LED_TIMER_MS_STARTING_EASY;
                break;
            }
            case MEDIUM:
            {
                mft.gam.ledPeriodMs = LED_TIMER_MS_STARTING_MEDIUM;
                break;
            }
            case HARD:
            {
                mft.gam.ledPeriodMs = LED_TIMER_MS_STARTING_HARD;
                break;
            }
            default:
            {
                break;
            }
        }
    }
    else if (GOING_SECOND == p2pGetPlayOrder(&mft.p2pMft) && false == mft.gam.receiveFirstMsg)
    {
        // If going second, ignore the first up/dn from the first player
        mft.gam.receiveFirstMsg = true;
    }
    else if(up)
    {
        switch(mft.gam.difficulty)
        {
            case EASY:
            case MEDIUM:
            {
                mft.gam.ledPeriodMs--;
                break;
            }
            case HARD:
            {
                mft.gam.ledPeriodMs -= 2;
                break;
            }
            default:
            {
                break;
            }
        }
        // Anything less than a 3ms period is impossible...
        if(mft.gam.ledPeriodMs < 3)
        {
            mft.gam.ledPeriodMs = 3;
        }
    }
    else
    {
        switch(mft.gam.difficulty)
        {
            case EASY:
            case MEDIUM:
            {
                mft.gam.ledPeriodMs++;
                break;
            }
            case HARD:
            {
                mft.gam.ledPeriodMs += 2;
                break;
            }
            default:
            {
                break;
            }
        }
    }
}
