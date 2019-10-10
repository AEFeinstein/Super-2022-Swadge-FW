/*
 * mode_joust_game.c
 *
 *  Created on: Sep 1, 2019
 *      Author: aaron
 *
 */

/*============================================================================
 * Includes
 *==========================================================================*/

#include <osapi.h>
#include <user_interface.h>
#include <p2pConnection.h>
#include <math.h>
#include <stdlib.h>

#include "user_main.h"
#include "mode_joust_game.h"
#include "custom_commands.h"
#include "buttons.h"
#include "oled.h"
#include "font.h"
#include "embeddedout.h"

/*============================================================================
 * Defines
 *==========================================================================*/

#define JOUST_DEBUG_PRINT
#ifdef JOUST_DEBUG_PRINT
    #define joust_printf(...) os_printf(__VA_ARGS__)
#else
    #define joust_printf(...)
#endif

/*============================================================================
 * Enums
 *==========================================================================*/

typedef enum
{
    R_MENU,
    R_SEARCHING,
    R_CONNECTING,
    R_SHOW_CONNECTION,
    R_PLAYING,
    R_WAITING,
    R_SHOW_GAME_RESULT
} joustGameState_t;

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

/*============================================================================
 * Prototypes
 *==========================================================================*/

// SwadgeMode Callbacks
void ICACHE_FLASH_ATTR joustInit(void);
void ICACHE_FLASH_ATTR joustDeinit(void);
void ICACHE_FLASH_ATTR joustButton(uint8_t state __attribute__((unused)),
                                   int button, int down);
void ICACHE_FLASH_ATTR joustRecvCb(uint8_t* mac_addr, uint8_t* data, uint8_t len, uint8_t rssi);
void ICACHE_FLASH_ATTR joustSendCb(uint8_t* mac_addr, mt_tx_status status);

// Helper function
void ICACHE_FLASH_ATTR joustRestart(void* arg __attribute__((unused)));
void ICACHE_FLASH_ATTR joustConnectionCallback(p2pInfo* p2p, connectionEvt_t event);
void ICACHE_FLASH_ATTR joustMsgCallbackFn(p2pInfo* p2p, char* msg, uint8_t* payload, uint8_t len);
void ICACHE_FLASH_ATTR joustMsgTxCbFn(p2pInfo* p2p, messageStatus_t status);
uint32_t ICACHE_FLASH_ATTR joust_rand(uint32_t upperBound);
// Transmission Functions
void ICACHE_FLASH_ATTR joustSendMsg(char* msg, uint16_t len, bool shouldAck, void (*success)(void*),
                                    void (*failure)(void*));
void ICACHE_FLASH_ATTR joustTxAllRetriesTimeout(void* arg __attribute__((unused)) );
void ICACHE_FLASH_ATTR joustTxRetryTimeout(void* arg);

// Connection functions
void ICACHE_FLASH_ATTR joustConnectionTimeout(void* arg __attribute__((unused)));

// Game functions
void ICACHE_FLASH_ATTR joustStartPlaying(void* arg __attribute__((unused)));
void ICACHE_FLASH_ATTR joustStartRound(void);
void ICACHE_FLASH_ATTR joustSendRoundLossMsg(void);
void ICACHE_FLASH_ATTR joustAccelerometerHandler(accel_t* accel);

// LED Functions
void ICACHE_FLASH_ATTR joustDisarmAllLedTimers(void);
void ICACHE_FLASH_ATTR joustConnLedTimeout(void* arg __attribute__((unused)));
void ICACHE_FLASH_ATTR joustShowConnectionLedTimeout(void* arg __attribute__((unused)));
void ICACHE_FLASH_ATTR joustGameLedTimeout(void* arg __attribute__((unused)));
void ICACHE_FLASH_ATTR joustRoundResultLed(void* arg __attribute__((unused)));
void ICACHE_FLASH_ATTR joustRoundResult(bool);

void ICACHE_FLASH_ATTR joustUpdateDisplay(void);

/*============================================================================
 * Variables
 *==========================================================================*/

swadgeMode joustGameMode =
{
    .modeName = "magjoust",
    .fnEnterMode = joustInit,
    .fnExitMode = joustDeinit,
    .fnButtonCallback = joustButton,
    .wifiMode = ESP_NOW,
    .fnEspNowRecvCb = joustRecvCb,
    .fnEspNowSendCb = joustSendCb,
    .fnAccelerometerCallback = joustAccelerometerHandler
};

struct
{
    joustGameState_t gameState;
    accel_t joustAccel;
    uint16_t rolling_average;
    uint32_t con_color;
    // Game state variables
    struct
    {
        bool shouldTurnOnLeds;
        bool round_winner;
        uint32_t joustElo;
        uint32_t otherJoustElo;
        uint32_t win_score;
        uint32_t lose_score;
    } gam;

    // Timers
    struct
    {
        os_timer_t StartPlaying;
        os_timer_t ConnLed;
        os_timer_t ShowConnectionLed;
        os_timer_t GameLed;
        os_timer_t RoundResultLed;
        os_timer_t RestartJoust;
    } tmr;

    // LED variables
    struct
    {
        led_t Leds[6];
        connLedState_t ConnLedState;
        sint16_t Degree;
        uint8_t connectionDim;
        uint8_t digitToDisplay;
        uint8_t ledsLit;
        uint8_t currBrightness;
    } led;

    p2pInfo p2pJoust;
} joust;

/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * Get a random number from a range.
 *
 * This isn't true-random, unless bound is a power of 2. But it's close enough?
 * The problem is that os_random() returns a number between [0, 2^64), and the
 * size of the range may not be even divisible by bound
 *
 * For what it's worth, this is what Arduino's random() does. It lies!
 *
 * @param bound An upper bound of the random range to return
 * @return A number in the range [0,bound), which does not include bound
 */
uint32_t ICACHE_FLASH_ATTR joust_rand(uint32_t bound)
{
    if(bound == 0)
    {
        return 0;
    }
    return os_random() % bound;
}

void ICACHE_FLASH_ATTR joustConnectionCallback(p2pInfo* p2p __attribute__((unused)), connectionEvt_t event)
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

            if(GOING_FIRST == p2pGetPlayOrder(&joust.p2pJoust))
            {
                char color_string[32] = {0};
                joust.con_color =  joust_rand(255);
                ets_snprintf(color_string, sizeof(color_string), "%d", joust.con_color);
                p2pSendMsg(&joust.p2pJoust, "col", color_string, sizeof(color_string), joustMsgTxCbFn);
            }
            joust_printf("connection established\n");
            joust.gameState = R_SHOW_CONNECTION;

            // ets_memset(joust.led.Leds, 0, sizeof(joust.led.Leds));

            joustDisarmAllLedTimers();
            // 6ms * ~500 steps == 3s animation
            //This is the start of the game
            joust.led.currBrightness = 0;
            joust.led.ConnLedState = LED_CONNECTED_BRIGHT;
            os_timer_arm(&joust.tmr.ShowConnectionLed, 6, true);

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
 * @brief
 *
 * @param msg
 * @param payload
 * @param len
 */
void ICACHE_FLASH_ATTR joustMsgCallbackFn(p2pInfo* p2p __attribute__((unused)), char* msg, uint8_t* payload,
        uint8_t len __attribute__((unused)))
{
    if(len > 0)
    {
        joust_printf("%s %s %s\n", __func__, msg, payload);
    }
    else
    {
        joust_printf("%s %s\n", __func__, msg);
    }


    //send color: col First
    //then send our elo: elo
    //then send their elo: elt
    if(0 == ets_memcmp(msg, "col", 3))
    {
        joust.con_color =  atoi((const char*)payload);
        char elo_string[32] = {0};
        ets_snprintf(elo_string, sizeof(elo_string), "%d", joust.gam.joustElo);
        p2pSendMsg(&joust.p2pJoust, "elo", elo_string, sizeof(elo_string), joustMsgTxCbFn);
    }
    else if(0 == ets_memcmp(msg, "elo", 3))
    {
        joust.gam.otherJoustElo =  atoi((const char*)payload);
        char elo_string[32] = {0};
        ets_snprintf(elo_string, sizeof(elo_string), "%d", joust.gam.joustElo);
        p2pSendMsg(&joust.p2pJoust, "elt", elo_string, sizeof(elo_string), joustMsgTxCbFn);
    }
    else if(0 == ets_memcmp(msg, "elt", 3))
    {
        joust.gam.otherJoustElo =  atoi((const char*)payload);
    }

    switch(joust.gameState)
    {
        case R_CONNECTING:
        {
            break;
        }
        case R_WAITING:
        {
            break;
        }
        case R_PLAYING:
        {
            if(0 == ets_memcmp(msg, "los", 3))
            {
                joustRoundResult(true);
            }
            // Currently playing a game, if a message is sent, then update score
            break;
        }
        case R_MENU:
        case R_SEARCHING:
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

//were gonna need a lobby with lots of players
/**
 * Initialize everything and start sending broadcast messages
 */
void ICACHE_FLASH_ATTR joustInit(void)
{
    uint8_t i;
    for(i = 0; i < 6; i++)
    {
        joust.led.Leds[i].r = 0;
        joust.led.Leds[i].g = 0;
        joust.led.Leds[i].b = 0;
    }

    setLeds(joust.led.Leds, sizeof(joust.led.Leds));

    ets_memset(&joust, 0, sizeof(joust));
    //just print for now
    joust_printf("%s\r\n", __func__);
    // setJoustElo(150);
    joust.gam.joustElo = getJoustElo();
    if(joust.gam.joustElo > 10000)
    {
        setJoustElo(1000);
    }

    clearDisplay();
    plotText(0, 0, "MagJoust", IBM_VGA_8, WHITE);
    char menuStr[32] = {0};
    // plotText(0, OLED_HEIGHT - (4 * (FONT_HEIGHT_IBMVGA8 + 1)), "ELO:", IBM_VGA_8);
    ets_snprintf(menuStr, sizeof(menuStr), "level: %d", joust.gam.joustElo);
    plotText(0, OLED_HEIGHT - (4 * (FONT_HEIGHT_IBMVGA8 + 1)), menuStr, IBM_VGA_8, WHITE);

    if(joust.gam.joustElo < 200)
    {
        plotText(0, OLED_HEIGHT - (3 * (FONT_HEIGHT_IBMVGA8 + 1)), "Etruscan", IBM_VGA_8, WHITE);
        plotText(0, OLED_HEIGHT - (2 * (FONT_HEIGHT_IBMVGA8 + 1)), "shrew", IBM_VGA_8, WHITE);
        plotText(0, OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)), "Next level: 200", IBM_VGA_8, WHITE);
    }
    else if(joust.gam.joustElo < 300)
    {
        plotText(0, OLED_HEIGHT - (3 * (FONT_HEIGHT_IBMVGA8 + 1)), "Bumblebee", IBM_VGA_8, WHITE);
        plotText(0, OLED_HEIGHT - (2 * (FONT_HEIGHT_IBMVGA8 + 1)), "bat", IBM_VGA_8, WHITE);
        plotText(0, OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)), "Next level: 300", IBM_VGA_8, WHITE);
    }
    else if(joust.gam.joustElo < 400)
    {
        plotText(0, OLED_HEIGHT - (3 * (FONT_HEIGHT_IBMVGA8 + 1)), "Pygmy", IBM_VGA_8, WHITE);
        plotText(0, OLED_HEIGHT - (2 * (FONT_HEIGHT_IBMVGA8 + 1)), "possum", IBM_VGA_8, WHITE);
        plotText(0, OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)), "Next level: 400", IBM_VGA_8, WHITE);
    }
    else if(joust.gam.joustElo < 500)
    {
        plotText(0, OLED_HEIGHT - (3 * (FONT_HEIGHT_IBMVGA8 + 1)), "petite", IBM_VGA_8, WHITE);
        plotText(0, OLED_HEIGHT - (2 * (FONT_HEIGHT_IBMVGA8 + 1)), "squirrel", IBM_VGA_8, WHITE);
        plotText(0, OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)), "Next level: 500", IBM_VGA_8, WHITE);
    }
    else if(joust.gam.joustElo < 600)
    {
        plotText(0, OLED_HEIGHT - (3 * (FONT_HEIGHT_IBMVGA8 + 1)), "small learner", IBM_VGA_8, WHITE);
        plotText(0, OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)), "Next level: 600", IBM_VGA_8, WHITE);
    }
    else if(joust.gam.joustElo < 700)
    {
        plotText(0, OLED_HEIGHT - (3 * (FONT_HEIGHT_IBMVGA8 + 1)), "apprentice", IBM_VGA_8, WHITE);
        plotText(0, OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)), "Next level: 700", IBM_VGA_8, WHITE);
    }
    else if(joust.gam.joustElo < 800)
    {
        plotText(0, OLED_HEIGHT - (3 * (FONT_HEIGHT_IBMVGA8 + 1)), "Bright Learner", IBM_VGA_8, WHITE);
        plotText(0, OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)), "Next level: 800", IBM_VGA_8, WHITE);
    }
    else if(joust.gam.joustElo < 1000)
    {
        plotText(0, OLED_HEIGHT - (3 * (FONT_HEIGHT_IBMVGA8 + 1)), "Near Beginner", IBM_VGA_8, WHITE);
        plotText(0, OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)), "Next level: 1000", IBM_VGA_8, WHITE);
    }
    else if(joust.gam.joustElo < 1100)
    {
        plotText(0, OLED_HEIGHT - (3 * (FONT_HEIGHT_IBMVGA8 + 1)), "Beginner", IBM_VGA_8, WHITE);
        plotText(0, OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)), "Next level: 1100", IBM_VGA_8, WHITE);
    }
    else if(joust.gam.joustElo < 1200)
    {
        plotText(0, OLED_HEIGHT - (3 * (FONT_HEIGHT_IBMVGA8 + 1)), "Astute", IBM_VGA_8, WHITE);
        plotText(0, OLED_HEIGHT - (2 * (FONT_HEIGHT_IBMVGA8 + 1)), "Beginner", IBM_VGA_8, WHITE);
        plotText(0, OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)), "Next level: 1200", IBM_VGA_8, WHITE);
    }
    else if(joust.gam.joustElo < 1300)
    {
        plotText(0, OLED_HEIGHT - (3 * (FONT_HEIGHT_IBMVGA8 + 1)), "Almost", IBM_VGA_8, WHITE);
        plotText(0, OLED_HEIGHT - (2 * (FONT_HEIGHT_IBMVGA8 + 1)), "Intermediate", IBM_VGA_8, WHITE);
        plotText(0, OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)), "Next level: 1300", IBM_VGA_8, WHITE);
    }
    else if(joust.gam.joustElo < 1400)
    {
        plotText(0, OLED_HEIGHT - (3 * (FONT_HEIGHT_IBMVGA8 + 1)), "Intermediate", IBM_VGA_8, WHITE);
        plotText(0, OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)), "Next level: 1400", IBM_VGA_8, WHITE);
    }
    else if(joust.gam.joustElo < 1500)
    {
        plotText(0, OLED_HEIGHT - (3 * (FONT_HEIGHT_IBMVGA8 + 1)), "Expert", IBM_VGA_8, WHITE);
        plotText(0, OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)), "Next level: 1500", IBM_VGA_8, WHITE);
    }
    else if(joust.gam.joustElo < 1600)
    {
        plotText(0, OLED_HEIGHT - (3 * (FONT_HEIGHT_IBMVGA8 + 1)), "Master", IBM_VGA_8, WHITE);
        plotText(0, OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)), "Next level: 1600", IBM_VGA_8, WHITE);
    }
    else if(joust.gam.joustElo < 1700)
    {
        plotText(0, OLED_HEIGHT - (3 * (FONT_HEIGHT_IBMVGA8 + 1)), "GrandMaster", IBM_VGA_8, WHITE);
        plotText(0, OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)), "Next level: 1700", IBM_VGA_8, WHITE);
    }
    else if(joust.gam.joustElo < 1800)
    {
        plotText(0, OLED_HEIGHT - (3 * (FONT_HEIGHT_IBMVGA8 + 1)), "Joust Slayer", IBM_VGA_8, WHITE);
        plotText(0, OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)), "Next level: 1800", IBM_VGA_8, WHITE);
    }
    else if(joust.gam.joustElo < 1900)
    {
        plotText(0, OLED_HEIGHT - (3 * (FONT_HEIGHT_IBMVGA8 + 1)), "Joust", IBM_VGA_8, WHITE);
        plotText(0, OLED_HEIGHT - (2 * (FONT_HEIGHT_IBMVGA8 + 1)), "NimbleMaster", IBM_VGA_8, WHITE);
        plotText(0, OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)), "Next level: 1900", IBM_VGA_8, WHITE);
    }
    else if(joust.gam.joustElo < 2000)
    {
        plotText(0, OLED_HEIGHT - (3 * (FONT_HEIGHT_IBMVGA8 + 1)), "Joust", IBM_VGA_8, WHITE);
        plotText(0, OLED_HEIGHT - (2 * (FONT_HEIGHT_IBMVGA8 + 1)), "sorcerer", IBM_VGA_8, WHITE);
        plotText(0, OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)), "Next level: 2000", IBM_VGA_8, WHITE);
    }
    else if(joust.gam.joustElo < 2100)
    {
        plotText(0, OLED_HEIGHT - (3 * (FONT_HEIGHT_IBMVGA8 + 1)), "Joust", IBM_VGA_8, WHITE);
        plotText(0, OLED_HEIGHT - (2 * (FONT_HEIGHT_IBMVGA8 + 1)), "Obliterator", IBM_VGA_8, WHITE);
        plotText(0, OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)), "Next level: 2100", IBM_VGA_8, WHITE);
    }
    else if(joust.gam.joustElo < 2200)
    {
        plotText(0, OLED_HEIGHT - (3 * (FONT_HEIGHT_IBMVGA8 + 1)), "Joust", IBM_VGA_8, WHITE);
        plotText(0, OLED_HEIGHT - (3 * (FONT_HEIGHT_IBMVGA8 + 1)), "MindBender", IBM_VGA_8, WHITE);
        plotText(0, OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)), "Next level: 2200", IBM_VGA_8, WHITE);
    }
    else if(joust.gam.joustElo < 2400)
    {
        plotText(0, OLED_HEIGHT - (3 * (FONT_HEIGHT_IBMVGA8 + 1)), "Joust", IBM_VGA_8, WHITE);
        plotText(0, OLED_HEIGHT - (2 * (FONT_HEIGHT_IBMVGA8 + 1)), "fate sealer", IBM_VGA_8, WHITE);
        plotText(0, OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)), "Next level: 2400", IBM_VGA_8, WHITE);
    }
    else
    {
        plotText(0, OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)), "Joust God", IBM_VGA_8, WHITE);
    }

    // Enable button debounce for consistent 1p/2p and difficulty config
    enableDebounce(true);

    p2pInitialize(&joust.p2pJoust, "jou", joustConnectionCallback, joustMsgCallbackFn, 10);

    //we don't need a timer to show a successful connection, but we do need
    //to start the game eventually
    // Set up a timer for showing a successful connection, don't start it
    os_timer_disarm(&joust.tmr.ShowConnectionLed);
    os_timer_setfn(&joust.tmr.ShowConnectionLed, joustShowConnectionLedTimeout, NULL);

    // Set up a timer for starting the next round, don't start it
    os_timer_disarm(&joust.tmr.StartPlaying);
    os_timer_setfn(&joust.tmr.StartPlaying, joustStartPlaying, NULL);

    //some is specific to reflector game, but we can still use some for
    //setting leds
    // Set up a timer to update LEDs, start it
    os_timer_disarm(&joust.tmr.ConnLed);
    os_timer_setfn(&joust.tmr.ConnLed, joustConnLedTimeout, NULL);

    os_timer_disarm(&joust.tmr.GameLed);
    os_timer_setfn(&joust.tmr.GameLed, joustGameLedTimeout, NULL);

    os_timer_disarm(&joust.tmr.RestartJoust);
    os_timer_setfn(&joust.tmr.RestartJoust, joustRestart, NULL);

    os_timer_disarm(&joust.tmr.RoundResultLed);
    os_timer_setfn(&joust.tmr.RoundResultLed, joustRoundResultLed, NULL);

    joust.gameState = R_MENU;
}

/**
 * Clean up all timers
 */
void ICACHE_FLASH_ATTR joustDeinit(void)
{
    joust_printf("%s\r\n", __func__);
    p2pDeinit(&joust.p2pJoust);
    os_timer_disarm(&joust.tmr.StartPlaying);
    joustDisarmAllLedTimers();
}

/**
 * Restart by deiniting then initing
 *
 * @param arg unused
 */
void ICACHE_FLASH_ATTR joustRestart(void* arg __attribute__((unused)))
{
    joustDeinit();
    joustInit();
}

/**
 * Disarm any timers which control LEDs
 */
void ICACHE_FLASH_ATTR joustDisarmAllLedTimers(void)
{
    os_timer_disarm(&joust.tmr.ConnLed);
    os_timer_disarm(&joust.tmr.ShowConnectionLed);
    os_timer_disarm(&joust.tmr.GameLed);
    os_timer_disarm(&joust.tmr.RoundResultLed);
}

/**
 * This is called after an attempted transmission. If it was successful, and the
 * message should be acked, start a retry timer. If it wasn't successful, just
 * try again
 *
 * @param mac_addr
 * @param status
 */
void ICACHE_FLASH_ATTR joustSendCb(uint8_t* mac_addr __attribute__((unused)),
                                   mt_tx_status status)
{
    p2pSendCb(&joust.p2pJoust, mac_addr, status);
}

/**
 * This is called whenever an ESP NOW packet is received
 *
 * @param mac_addr The MAC of the swadge that sent the data
 * @param data     The data
 * @param len      The length of the data
 */
void ICACHE_FLASH_ATTR joustRecvCb(uint8_t* mac_addr, uint8_t* data, uint8_t len, uint8_t rssi)
{
    p2pRecvCb(&joust.p2pJoust, mac_addr, data, len, rssi);
}

/**
 * This LED handling timer fades in and fades out white LEDs to indicate
 * a successful connection. After the animation, the game will start
 *
 * @param arg unused
 */
void ICACHE_FLASH_ATTR joustShowConnectionLedTimeout(void* arg __attribute__((unused)) )
{

    clearDisplay();
    char accelStr[32] = {0};
    plotText(0, 0, "Found Player", IBM_VGA_8, WHITE);

    ets_snprintf(accelStr, sizeof(accelStr), "level: %d", joust.gam.otherJoustElo);
    plotText(0, OLED_HEIGHT - (4 * (FONT_HEIGHT_IBMVGA8 + 1)), accelStr, IBM_VGA_8, WHITE);

    double P1 = ( 1.0f / (1.0f + pow(10.0f, ((double)joust.gam.otherJoustElo - (double)joust.gam.joustElo) / 400.0f)));

    joust.gam.win_score = (uint32_t)  200.0f * (1.0f - P1);
    joust.gam.lose_score = (uint32_t) 200.0f * ( P1);
    ets_snprintf(accelStr, sizeof(accelStr), "win :+%d", (int)joust.gam.win_score);
    plotText(0, OLED_HEIGHT - (3 * (FONT_HEIGHT_IBMVGA8 + 1)), accelStr, IBM_VGA_8,WHITE);

    ets_snprintf(accelStr, sizeof(accelStr), "lose: -%d", (int)joust.gam.lose_score);
    plotText(0, OLED_HEIGHT - (2 * (FONT_HEIGHT_IBMVGA8 + 1)), accelStr, IBM_VGA_8,WHITE);

    switch(joust.led.ConnLedState)
    {
        case LED_CONNECTED_BRIGHT:
        {
            joust.led.currBrightness;
            if(joust.led.currBrightness == 255)
            {
                joust.led.ConnLedState = LED_CONNECTED_DIM;
            }
            break;
        }
        case LED_CONNECTED_DIM:
        {
            joust.led.currBrightness--;
            if(joust.led.currBrightness == 0x00)
            {
                joustStartPlaying(NULL);
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
    uint8_t i;
    for(i = 0; i < 6; i++)
    {
        joust.led.Leds[i].r = (EHSVtoHEX(joust.con_color, 255,  joust.led.currBrightness) >>  0) & 0xFF;
        joust.led.Leds[i].g = (EHSVtoHEX(joust.con_color, 255,  joust.led.currBrightness) >>  8) & 0xFF;
        joust.led.Leds[i].b = (EHSVtoHEX(joust.con_color, 255,  joust.led.currBrightness) >> 16) & 0xFF;
    }

    setLeds(joust.led.Leds, sizeof(joust.led.Leds));
}

/**
 * This LED handling timer fades in and fades out white LEDs to indicate
 * a successful connection. After the animation, the game will start
 *
 * @param arg unused
 */
void ICACHE_FLASH_ATTR joustGameLedTimeout(void* arg __attribute__((unused)) )
{
    switch(joust.led.ConnLedState)
    {
        case LED_CONNECTED_BRIGHT:
        {
            joust.led.currBrightness++;
            if(joust.led.currBrightness == 30)
            {
                joust.led.ConnLedState = LED_CONNECTED_DIM;
            }
            break;
        }
        case LED_CONNECTED_DIM:
        {
            joust.led.currBrightness--;
            if(joust.led.currBrightness == 0)
            {
                joust.led.ConnLedState = LED_CONNECTED_BRIGHT;
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
    uint8_t i;
    for(i = 0; i < 6; i++)
    {
        joust.led.Leds[i].r = (EHSVtoHEX(joust.con_color, 255,  joust.led.currBrightness) >>  0) & 0xFF;
        joust.led.Leds[i].g = (EHSVtoHEX(joust.con_color, 255,  joust.led.currBrightness) >>  8) & 0xFF;
        joust.led.Leds[i].b = (EHSVtoHEX(joust.con_color, 255,  joust.led.currBrightness) >> 16) & 0xFF;
    }

    setLeds(joust.led.Leds, sizeof(joust.led.Leds));
}

/**
 * This is called after connection is all done. Start the game!
 *
 * @param arg unused
 */
void ICACHE_FLASH_ATTR joustStartPlaying(void* arg __attribute__((unused)))
{
    joust_printf("%s\r\n", __func__);
    joust_printf("\nstarting the game\n");

    // Disable button debounce for minimum latency
    enableDebounce(false);

    // Turn off the LEDs
    joustDisarmAllLedTimers();
    joust.led.currBrightness = 0;
    joust.led.ConnLedState = LED_CONNECTED_BRIGHT;
    os_timer_arm(&joust.tmr.GameLed, 6, true);
    joust.gameState = R_PLAYING;
}

/**
 * Start a round of the game by picking a random action and starting
 * refGameLedTimeout()
 */
void ICACHE_FLASH_ATTR joustStartRound(void)
{
    joust.gameState = R_PLAYING;
}

void ICACHE_FLASH_ATTR joustUpdateDisplay(void)
{
    // Clear the display
    clearDisplay();
    // Draw a title
    plotText(0, 0, "JOUST", RADIOSTARS,WHITE);
    // Display the acceleration on the display
    char accelStr[32] = {0};

    ets_snprintf(accelStr, sizeof(accelStr), "Acc: %d", joust.rolling_average);
    plotText(0, OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)), accelStr, IBM_VGA_8,WHITE);
}

/**
 * Update the acceleration for the Joust mode
 */
void ICACHE_FLASH_ATTR joustAccelerometerHandler(accel_t* accel)
{
    joust.joustAccel.x = accel->x;
    joust.joustAccel.y = accel->y;
    joust.joustAccel.z = accel->z;
    int mov = (int) sqrt(pow(joust.joustAccel.x, 2) + pow(joust.joustAccel.y, 2) + pow(joust.joustAccel.z, 2));
    joust.rolling_average = (joust.rolling_average * 2 + mov) / 3;
    if (joust.gameState == R_PLAYING)
    {
        if(mov > joust.rolling_average + 50)
        {
            joustSendRoundLossMsg();
        }
        else
        {
            joustUpdateDisplay();
        }
    }
}

/**
 * Called every 4ms, this updates the LEDs during connection
 */
void ICACHE_FLASH_ATTR joustConnLedTimeout(void* arg __attribute__((unused)))
{
    switch(joust.led.ConnLedState)
    {
        case LED_OFF:
        {
            // Reset this timer to LED_PERIOD_MS
            joustDisarmAllLedTimers();
            os_timer_arm(&joust.tmr.ConnLed, 4, true);

            joust.led.connectionDim = 0;
            ets_memset(joust.led.Leds, 0, sizeof(joust.led.Leds));

            joust.led.ConnLedState = LED_ON_1;
            break;
        }
        case LED_ON_1:
        {
            // Turn LEDs on
            joust.led.connectionDim = 50;

            // Prepare the first dimming
            joust.led.ConnLedState = LED_DIM_1;
            break;
        }
        case LED_DIM_1:
        {
            // Dim leds
            joust.led.connectionDim--;
            // If its kind of dim, turn it on again
            if(joust.led.connectionDim == 1)
            {
                joust.led.ConnLedState = LED_ON_2;
            }
            break;
        }
        case LED_ON_2:
        {
            // Turn LEDs on
            joust.led.connectionDim = 50;
            // Prepare the second dimming
            joust.led.ConnLedState = LED_DIM_2;
            break;
        }
        case LED_DIM_2:
        {
            // Dim leds
            joust.led.connectionDim -= 1;
            // If its off, start waiting
            if(joust.led.connectionDim == 0)
            {
                joust.led.ConnLedState = LED_OFF_WAIT;
            }
            break;
        }
        case LED_OFF_WAIT:
        {
            // Start a timer to update LEDs
            joustDisarmAllLedTimers();
            os_timer_arm(&joust.tmr.ConnLed, 1000, true);

            // When it fires, start all over again
            joust.led.ConnLedState = LED_OFF;

            // And dont update the LED state this time
            return;
        }
        case LED_CONNECTED_BRIGHT:
        case LED_CONNECTED_DIM:
        {
            // Handled in joustShowConnectionLedTimeout()
            break;
        }
        default:
        {
            break;
        }
    }

    // Copy the color value to all LEDs
    ets_memset(joust.led.Leds, 0, sizeof(joust.led.Leds));
    //just do blue for now
    uint8_t i;
    for(i = 0; i < 6; i ++)
    {
        joust.led.Leds[i].b = joust.led.connectionDim;
    }

    // Physically set the LEDs
    setLeds(joust.led.Leds, sizeof(joust.led.Leds));
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
void ICACHE_FLASH_ATTR joustButton( uint8_t state __attribute__((unused)),
                                    int button __attribute__((unused)), int down __attribute__((unused)))
{
    if(!down)
    {
        // Ignore all button releases
        return;
    }

    if(joust.gameState ==  R_MENU)
    {
        if(1 == button || 2 == button)
        {
            joust.gameState =  R_SEARCHING;
            joustDisarmAllLedTimers();
            os_timer_arm(&joust.tmr.ConnLed, 1, true);
            p2pStartConnection(&joust.p2pJoust);
            clearDisplay();
            plotText(0, 0, "Searching", IBM_VGA_8,WHITE);
        }
    }
}

/**
 * This is called when a round is lost. It tallies the loss, calls
 * joustRoundResultLed() to display the wins/losses and set up the
 * potential next round, and sends a message to the other swadge
 * that the round was lost and
 *
 * Send a round loss message to the other swadge
 */
void ICACHE_FLASH_ATTR joustSendRoundLossMsg(void)
{
    joust_printf("Lost the round\r\n");

    // Send a message to that ESP that we lost the round
    // If it's acked, start a timer to reinit if another message is never received
    // If it's not acked, reinit with refRestart()
    p2pSendMsg(&joust.p2pJoust, "los", NULL, 0, joustMsgTxCbFn);
    // Show the current wins & losses
    joustRoundResult(false);
}

/**
 * @brief TODO
 *
 * @param p2p
 * @param status
 */
void ICACHE_FLASH_ATTR joustMsgTxCbFn(p2pInfo* p2p __attribute__((unused)),
                                      messageStatus_t status)
{
    switch(status)
    {
        case MSG_ACKED:
        {
            joust_printf("%s MSG_ACKED\n", __func__);
            break;
        }
        case MSG_FAILED:
        {
            joust_printf("%s MSG_FAILED\n", __func__);
            break;
        }
        default:
        {
            joust_printf("%s UNKNOWN\n", __func__);
            break;
        }
    }
}

void ICACHE_FLASH_ATTR joustRoundResultLed(void* arg __attribute__((unused)))
{
    joust.led.connectionDim = 255;
    uint8_t i;
    if(joust.gam.round_winner)
    {
        for(i = 0; i < 6; i++)
        {
            joust.led.Leds[i].g = 40;
            joust.led.Leds[i].r = 0;
            joust.led.Leds[i].b = 0;
        }
    }
    else
    {
        for(i = 0; i < 6; i++)
        {
            joust.led.Leds[i].r = 40;
            joust.led.Leds[i].g = 0;
            joust.led.Leds[i].b = 0;
        }
    }

    // ets_memset(joust.led.Leds, currBrightness, sizeof(joust.led.Leds));
    setLeds(joust.led.Leds, sizeof(joust.led.Leds));
}

/**
 * Show the wins and losses
 *
 * @param roundWinner true if this swadge was a winner, false if the other
 *                    swadge won
 */
void ICACHE_FLASH_ATTR joustRoundResult(bool roundWinner)
{

    joustDisarmAllLedTimers();
    joust.gam.round_winner = roundWinner;
    os_timer_arm(&joust.tmr.RoundResultLed, 6, true);
    joust.gameState = R_SHOW_GAME_RESULT;
    if(roundWinner)
    {
        clearDisplay();
        plotText(0, 0, "Winner", IBM_VGA_8,WHITE);
        joust.gam.joustElo = joust.gam.joustElo + joust.gam.win_score;
        char menuStr[32] = {0};
        ets_snprintf(menuStr, sizeof(menuStr), "level: +%d", joust.gam.win_score);
        plotText(0, OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)), menuStr, IBM_VGA_8, WHITE);
    }
    else
    {
        clearDisplay();
        plotText(0, 0, "Loser", IBM_VGA_8,WHITE);
        char menuStr[32] = {0};
        ets_snprintf(menuStr, sizeof(menuStr), "level: -%d", joust.gam.lose_score);
        plotText(0, OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)), menuStr, IBM_VGA_8, WHITE);
        if(((float)joust.gam.joustElo - (float)joust.gam.lose_score) < 100.0f)
        {
            joust.gam.joustElo = 100;
        }
        else
        {
            joust.gam.joustElo = joust.gam.joustElo - joust.gam.lose_score;
        }
    }
    setJoustElo(joust.gam.joustElo);
    os_timer_arm(&joust.tmr.RestartJoust, 6000, false);
}
