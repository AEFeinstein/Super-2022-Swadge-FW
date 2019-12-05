/*
 * mode_joust_game.c
 *
 *  Created on: Sep 1, 2019
 *      Author: Aaron Angert
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
#include "buzzer.h" // music and sfx
#include "hpatimer.h" // buzzer functions
#include "bresenham.h"

/*============================================================================
 * Defines
 *==========================================================================*/

#define JOUST_DEBUG_PRINT
#ifdef JOUST_DEBUG_PRINT
    #define joust_printf(...) os_printf(__VA_ARGS__)
#else
    #define joust_printf(...)
#endif
#define SPRITE_DIM 4
#define JOUST_FIELD_OFFSET_X 24
#define JOUST_FIELD_OFFSET_Y 14
#define JOUST_FIELD_WIDTH  SPRITE_DIM * 20
#define JOUST_FIELD_HEIGHT SPRITE_DIM * 11
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
    R_PLAYINGFFA,
    R_WAITING,
    R_SHOW_GAME_RESULT,
    R_GAME_OVER
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
void ICACHE_FLASH_ATTR joustFFACounter(void* arg __attribute__((unused)));
// Transmission Functions
void ICACHE_FLASH_ATTR joustSendMsg(char* msg, uint16_t len, bool shouldAck, void (*success)(void*),
                                    void (*failure)(void*));
void ICACHE_FLASH_ATTR joustTxAllRetriesTimeout(void* arg __attribute__((unused)) );
void ICACHE_FLASH_ATTR joustTxRetryTimeout(void* arg);

// Connection functions
void ICACHE_FLASH_ATTR joustConnectionTimeout(void* arg __attribute__((unused)));

// Game functions
void ICACHE_FLASH_ATTR joustStartPlaying(void* arg __attribute__((unused)));
void ICACHE_FLASH_ATTR joustStartPlayingFFA(void* arg __attribute__((unused)));
void ICACHE_FLASH_ATTR joustStartRound(void);
void ICACHE_FLASH_ATTR joustSendRoundLossMsg(void);
void ICACHE_FLASH_ATTR joustAccelerometerHandler(accel_t* accel);

// LED Functions
void ICACHE_FLASH_ATTR joustDisarmAllLedTimers(void);
void ICACHE_FLASH_ATTR joustConnLedTimeout(void* arg __attribute__((unused)));
void ICACHE_FLASH_ATTR joustShowConnectionLedTimeout(void* arg __attribute__((unused)));
void ICACHE_FLASH_ATTR joustShowConnectionLedTimeoutFFA(void* arg __attribute__((unused)));
void ICACHE_FLASH_ATTR joustGameLedTimeout(void* arg __attribute__((unused)));
void ICACHE_FLASH_ATTR joustRoundResultLed(void* arg __attribute__((unused)));
void ICACHE_FLASH_ATTR joustRoundResult(int);
void ICACHE_FLASH_ATTR joustRoundResultFFA(void);

void ICACHE_FLASH_ATTR joustUpdateDisplay(void);

/*============================================================================
 * Variables
 *==========================================================================*/

swadgeMode joustGameMode =
{
    .modeName = "joust",
    .fnEnterMode = joustInit,
    .fnExitMode = joustDeinit,
    .fnButtonCallback = joustButton,
    .wifiMode = ESP_NOW,
    .fnEspNowRecvCb = joustRecvCb,
    .fnEspNowSendCb = joustSendCb,
    .fnAccelerometerCallback = joustAccelerometerHandler,
    .menuImageData = mnu_joust_0,
    .menuImageLen = sizeof(mnu_joust_0)

};

struct
{
    joustGameState_t gameState;
    accel_t joustAccel;
    uint16_t rolling_average;
    uint32_t con_color;
    uint32_t FFACounter;
    // Game state variables
    struct
    {
        bool shouldTurnOnLeds;
        bool round_winner;
        uint32_t joustWins;
        uint32_t win_score;
        uint32_t lose_score;
    } gam;

    // Timers
    struct
    {
        os_timer_t StartPlaying;
        os_timer_t StartPlayingFFA;
        os_timer_t ConnLed;
        os_timer_t ShowConnectionLed;
        os_timer_t ShowConnectionLedFFA;
        os_timer_t FFACounter;
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

// Music / SFX

const song_t endGameSFX RODATA_ATTR =
{
    .notes = {
        {.note = C_4, .timeMs = 200},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_5, .timeMs = 200},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_4, .timeMs = 200},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_5, .timeMs = 200},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_4, .timeMs = 200},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_5, .timeMs = 200},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_4, .timeMs = 200},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_5, .timeMs = 200},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_4, .timeMs = 200},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_5, .timeMs = 200},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_4, .timeMs = 200},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_5, .timeMs = 200},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_4, .timeMs = 200},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_5, .timeMs = 200},
        {.note = SILENCE, .timeMs = 1},
    },
    .numNotes = 28,
    .shouldLoop = false
};

const song_t tieGameSFX RODATA_ATTR =
{
    .notes = {
        {.note = C_4, .timeMs = 200},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_5, .timeMs = 200},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_4, .timeMs = 200},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_5, .timeMs = 200},
        {.note = SILENCE, .timeMs = 1},
        {.note = E_4, .timeMs = 200},
        {.note = SILENCE, .timeMs = 1},
        {.note = E_5, .timeMs = 200},
        {.note = SILENCE, .timeMs = 1},
        {.note = E_4, .timeMs = 200},
        {.note = SILENCE, .timeMs = 1},
        {.note = E_5, .timeMs = 200},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_4, .timeMs = 200},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_5, .timeMs = 200},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_4, .timeMs = 200},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_5, .timeMs = 200},
        {.note = SILENCE, .timeMs = 300},
        {.note = C_4, .timeMs = 300},
        {.note = SILENCE, .timeMs = 1},
    },
    .numNotes = 26,
    .shouldLoop = false
};

const song_t endGameWin2SFX RODATA_ATTR =
{
    .notes = {
        {.note = C_4, .timeMs = 200},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_5, .timeMs = 200},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_4, .timeMs = 200},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_5, .timeMs = 200},
        {.note = SILENCE, .timeMs = 1},
        {.note = E_4, .timeMs = 200},
        {.note = SILENCE, .timeMs = 1},
        {.note = E_5, .timeMs = 200},
        {.note = SILENCE, .timeMs = 1},
        {.note = E_4, .timeMs = 200},
        {.note = SILENCE, .timeMs = 1},
        {.note = E_5, .timeMs = 200},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_4, .timeMs = 200},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_5, .timeMs = 200},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_4, .timeMs = 200},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_5, .timeMs = 200},
        {.note = SILENCE, .timeMs = 300},
        {.note = C_6, .timeMs = 300},
        {.note = SILENCE, .timeMs = 1},
    },
    .numNotes = 26,
    .shouldLoop = false
};


const song_t endGameWinSFX RODATA_ATTR =
{
    .notes = {
        {.note = C_4, .timeMs = 100},
        {.note = SILENCE, .timeMs = 1},
        {.note = D_4, .timeMs = 100},
        {.note = SILENCE, .timeMs = 1},
        {.note = E_4, .timeMs = 100},
        {.note = SILENCE, .timeMs = 1},
        {.note = F_4, .timeMs = 100},
        {.note = SILENCE, .timeMs = 200},
        {.note = C_4, .timeMs = 100},
        {.note = SILENCE, .timeMs = 1},
        {.note = D_4, .timeMs = 100},
        {.note = SILENCE, .timeMs = 1},
        {.note = E_4, .timeMs = 100},
        {.note = SILENCE, .timeMs = 1},
        {.note = F_4, .timeMs = 100},
        {.note = SILENCE, .timeMs = 200},
        {.note = D_4, .timeMs = 100},
        {.note = SILENCE, .timeMs = 1},
        {.note = E_4, .timeMs = 100},
        {.note = SILENCE, .timeMs = 1},
        {.note = F_4, .timeMs = 100},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_4, .timeMs = 100},
        {.note = SILENCE, .timeMs = 400},
        {.note = C_5, .timeMs = 200},
        {.note = SILENCE, .timeMs = 1},
    },
    .numNotes = 26,
    .shouldLoop = false
};

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
            clearDisplay();
            plotText(0, 0, "Found Player", IBM_VGA_8, WHITE);
            plotText(0, OLED_HEIGHT - (4 * (FONT_HEIGHT_IBMVGA8 + 1)), "Move theirs", IBM_VGA_8, WHITE);
            plotText(0, OLED_HEIGHT - (3 * (FONT_HEIGHT_IBMVGA8 + 1)), "Not yours!", IBM_VGA_8, WHITE);

            joustDisarmAllLedTimers();
            // 6ms * ~500 steps == 3s animation
            //This is the start of the game
            joust.led.currBrightness = 0;
            joust.led.ConnLedState = LED_CONNECTED_BRIGHT;
            os_timer_arm(&joust.tmr.ShowConnectionLed, 50, true);

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
                p2pSendMsg(&joust.p2pJoust, "win", NULL, 0, joustMsgTxCbFn);
                joustRoundResult(true);
            }
            // Currently playing a game, if a message is sent, then update score
            break;
        }
        case R_GAME_OVER:
        {
            if(0 == ets_memcmp(msg, "los", 3))
            {
                p2pSendMsg(&joust.p2pJoust, "tie", NULL, 0, joustMsgTxCbFn);
                joustRoundResult(2);
            }
            else if(0 == ets_memcmp(msg, "tie", 3))
            {
                joustRoundResult(2);
            }
            else if(0 == ets_memcmp(msg, "win", 3))
            {
                joustRoundResult(false);
            }
            // Currently playing a game, if a message is sent, then update score
            break;
        }
        case R_MENU:
        case R_SEARCHING:
        {
            //other sends color and elo first
            //then send our elo
            if(0 == ets_memcmp(msg, "col", 3))
            {
                joust.con_color =  atoi((const char*)payload);
                clearDisplay();
                plotText(0, 0, "Found Player", IBM_VGA_8, WHITE);
                plotText(0, OLED_HEIGHT - (4 * (FONT_HEIGHT_IBMVGA8 + 1)), "Move theirs", IBM_VGA_8, WHITE);
                plotText(0, OLED_HEIGHT - (3 * (FONT_HEIGHT_IBMVGA8 + 1)), "Not yours!", IBM_VGA_8, WHITE);
            }
        }
        case R_PLAYINGFFA:
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
    joust_printf("%s\r\n", __func__);
    joust.gam.joustWins = getJoustWins();
    if(joust.gam.joustWins > 10000)
    {
        setJoustWins(0);
    }

    clearDisplay();
    if(joust.gam.joustWins < 4)
    {
        plotText(0, 0, "Joust is a movement game where", TOM_THUMB, WHITE);
        plotText(0, FONT_HEIGHT_TOMTHUMB + 1, "you try to jostle your opponents", TOM_THUMB, WHITE);
        plotText(0, 2 * FONT_HEIGHT_TOMTHUMB + 2, "swadge while keeping yours still.", TOM_THUMB, WHITE);
        plotText(0, 4 * FONT_HEIGHT_TOMTHUMB, "There are two modes Free For all", TOM_THUMB, WHITE);
        plotText(0, 5 * FONT_HEIGHT_TOMTHUMB + 1, "and 2 Player, which tracks wins", TOM_THUMB, WHITE);
        plotText(0, 7 * FONT_HEIGHT_TOMTHUMB, "Press the left or right button", TOM_THUMB, WHITE);
        plotText(0, 8 * FONT_HEIGHT_TOMTHUMB + 1, "to select a game type. enjoy!", TOM_THUMB, WHITE);
        char menuStr[32] = {0};
        ets_snprintf(menuStr, sizeof(menuStr), "wins: %d", joust.gam.joustWins);
        plotText(0, 9 * FONT_HEIGHT_TOMTHUMB + 3, menuStr, TOM_THUMB, WHITE);
    }
    else
    {
        plotText(32, 0, "Joust", RADIOSTARS, WHITE);
        char menuStr[32] = {0};
        ets_snprintf(menuStr, sizeof(menuStr), "wins: %d", joust.gam.joustWins);
        plotText(0, OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)) - FONT_HEIGHT_TOMTHUMB - 6, menuStr, TOM_THUMB, WHITE);
    }

    uint8_t scoresAreaX0 = 0;
    uint8_t scoresAreaY0 = OLED_HEIGHT - (FONT_HEIGHT_TOMTHUMB + 3);
    uint8_t scoresAreaX1 = 23;
    uint8_t scoresAreaY1 = OLED_HEIGHT - 1;
    fillDisplayArea(scoresAreaX0, scoresAreaY0, scoresAreaX1, scoresAreaY1, BLACK);
    uint8_t scoresTextX = 0;
    uint8_t scoresTextY = OLED_HEIGHT - (FONT_HEIGHT_TOMTHUMB + 1);
    plotText(scoresTextX, scoresTextY, "Free For All", TOM_THUMB, WHITE);

    uint8_t startAreaX0 = OLED_WIDTH - 20;//39;
    uint8_t startAreaY0 = OLED_HEIGHT - (FONT_HEIGHT_TOMTHUMB + 3);
    uint8_t startAreaX1 = OLED_WIDTH - 1;
    uint8_t startAreaY1 = OLED_HEIGHT - 1;
    fillDisplayArea(startAreaX0, startAreaY0, startAreaX1, startAreaY1, BLACK);
    uint8_t startTextX = OLED_WIDTH - 38;//38;
    uint8_t startTextY = OLED_HEIGHT - (FONT_HEIGHT_TOMTHUMB + 1);
    plotText(startTextX, startTextY, "2 Player", TOM_THUMB, WHITE);

    if(joust.gam.joustWins < 4)
    {
        // plotText(0, OLED_HEIGHT - (4 * (FONT_HEIGHT_IBMVGA8 + 1)), "Serf Simian", IBM_VGA_8, WHITE);
        // plotText(0, OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1))-FONT_HEIGHT_TOMTHUMB, "Next level: 4", TOM_THUMB, WHITE);
    }
    else if(joust.gam.joustWins < 8)
    {
        plotText(0, OLED_HEIGHT - (4 * (FONT_HEIGHT_IBMVGA8 + 1)), "Peasant Primate", IBM_VGA_8, WHITE);
        plotText(0, OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)) - FONT_HEIGHT_TOMTHUMB, "Next level: 8", TOM_THUMB, WHITE);
    }
    else if(joust.gam.joustWins < 12)
    {
        plotText(0, OLED_HEIGHT - (4 * (FONT_HEIGHT_IBMVGA8 + 1)), "Page Probocsis", IBM_VGA_8, WHITE);
        plotText(0, OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)) - FONT_HEIGHT_TOMTHUMB, "Next level: 12", TOM_THUMB, WHITE);
    }
    else if(joust.gam.joustWins < 16)
    {
        plotText(0, OLED_HEIGHT - (4 * (FONT_HEIGHT_IBMVGA8 + 1)), "Squire Saki", IBM_VGA_8, WHITE);
        plotText(0, OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)) - FONT_HEIGHT_TOMTHUMB, "Next level: 16", TOM_THUMB, WHITE);
    }
    else if(joust.gam.joustWins < 22)
    {
        plotText(0, OLED_HEIGHT - (4 * (FONT_HEIGHT_IBMVGA8 + 1)), "Apprentice Ape", IBM_VGA_8, WHITE);
        plotText(0, OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)) - FONT_HEIGHT_TOMTHUMB, "Next level: 22", TOM_THUMB, WHITE);
    }
    else if(joust.gam.joustWins < 28)
    {
        plotText(0, OLED_HEIGHT - (4 * (FONT_HEIGHT_IBMVGA8 + 1)), "Maester Mandrill", IBM_VGA_8, WHITE);
        plotText(0, OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)) - FONT_HEIGHT_TOMTHUMB, "Next level: 28", TOM_THUMB, WHITE);
    }
    else if(joust.gam.joustWins < 36)
    {
        plotText(0, OLED_HEIGHT - (4 * (FONT_HEIGHT_IBMVGA8 + 1)), "Thane Tamarin", IBM_VGA_8, WHITE);
        plotText(0, OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)) - FONT_HEIGHT_TOMTHUMB, "Next level: 36", TOM_THUMB, WHITE);
    }
    else if(joust.gam.joustWins < 44)
    {
        plotText(0, OLED_HEIGHT - (4 * (FONT_HEIGHT_IBMVGA8 + 1)), "Lord Lemur", IBM_VGA_8, WHITE);
        plotText(0, OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)) - FONT_HEIGHT_TOMTHUMB, "Next level: 44", TOM_THUMB, WHITE);
    }
    else if(joust.gam.joustWins < 60)
    {
        plotText(0, OLED_HEIGHT - (4 * (FONT_HEIGHT_IBMVGA8 + 1)), "Baron Baboon", IBM_VGA_8, WHITE);
        plotText(0, OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)) - FONT_HEIGHT_TOMTHUMB, "Next level: 60", TOM_THUMB, WHITE);
    }
    else if(joust.gam.joustWins < 100)
    {
        plotText(0, OLED_HEIGHT - (4 * (FONT_HEIGHT_IBMVGA8 + 1)), "Viscount Vervet", IBM_VGA_8, WHITE);
    }
    else
    {
        plotText(0, OLED_HEIGHT - (4 * (FONT_HEIGHT_IBMVGA8 + 1)), "Grandmaster", IBM_VGA_8, WHITE);
        plotText(0, OLED_HEIGHT - (3 * (FONT_HEIGHT_IBMVGA8 + 1)), "Gorilla", IBM_VGA_8, WHITE);
    }

    // Enable button debounce for consistent 1p/2p and difficulty config
    enableDebounce(true);

    p2pInitialize(&joust.p2pJoust, "jou", joustConnectionCallback, joustMsgCallbackFn, 10);

    //we don't need a timer to show a successful connection, but we do need
    //to start the game eventually
    // Set up a timer for showing a successful connection, don't start it
    os_timer_disarm(&joust.tmr.ShowConnectionLed);
    os_timer_setfn(&joust.tmr.ShowConnectionLed, joustShowConnectionLedTimeout, NULL);

    os_timer_disarm(&joust.tmr.ShowConnectionLedFFA);
    os_timer_setfn(&joust.tmr.ShowConnectionLedFFA, joustShowConnectionLedTimeoutFFA, NULL);

    // Set up a timer for starting the next round, don't start it
    os_timer_disarm(&joust.tmr.StartPlaying);
    os_timer_setfn(&joust.tmr.StartPlaying, joustStartPlaying, NULL);

    // Set up a timer for starting the next round, don't start it
    os_timer_disarm(&joust.tmr.StartPlayingFFA);
    os_timer_setfn(&joust.tmr.StartPlayingFFA, joustStartPlayingFFA, NULL);

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

    os_timer_disarm(&joust.tmr.FFACounter);
    os_timer_setfn(&joust.tmr.FFACounter, joustFFACounter, NULL);


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
    os_timer_disarm(&joust.tmr.StartPlayingFFA);
    os_timer_disarm(&joust.tmr.RestartJoust);
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
    os_timer_disarm(&joust.tmr.ShowConnectionLedFFA);
    os_timer_disarm(&joust.tmr.GameLed);
    os_timer_disarm(&joust.tmr.RoundResultLed);
    os_timer_disarm(&joust.tmr.FFACounter);
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
    switch(joust.led.ConnLedState)
    {
        case LED_CONNECTED_BRIGHT:
        {
            joust.led.currBrightness = joust.led.currBrightness + 5;
            if(joust.led.currBrightness > 200)
            {
                joust.led.ConnLedState = LED_CONNECTED_DIM;
            }
            break;
        }
        case LED_CONNECTED_DIM:
        {
            joust.led.currBrightness = joust.led.currBrightness - 5;
            if(joust.led.currBrightness < 10 )
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
void ICACHE_FLASH_ATTR joustShowConnectionLedTimeoutFFA(void* arg __attribute__((unused)) )
{
    switch(joust.led.ConnLedState)
    {
        case LED_CONNECTED_BRIGHT:
        {
            joust.led.currBrightness = joust.led.currBrightness + 5;
            if(joust.led.currBrightness > 200)
            {
                joust.led.ConnLedState = LED_CONNECTED_DIM;
            }
            break;
        }
        case LED_CONNECTED_DIM:
        {
            joust.led.currBrightness = joust.led.currBrightness - 5;
            if(joust.led.currBrightness < 10 )
            {
                //need to start FFA playing here
                joustStartPlayingFFA(NULL);
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
 * This is called after connection is all done. Start the game!
 *
 * @param arg unused
 */
void ICACHE_FLASH_ATTR joustStartPlayingFFA(void* arg __attribute__((unused)))
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
    joust.FFACounter = 0;
    os_timer_arm(&joust.tmr.FFACounter, 50, true);
    joust.gameState = R_PLAYINGFFA;
}


void ICACHE_FLASH_ATTR joustUpdateDisplay(void)
{
    // Clear the display
    clearDisplay();
    // Draw a title
    plotText(0, 0, "JOUST", RADIOSTARS, WHITE);
    // Display the acceleration on the display
    char accelStr[32] = {0};

    ets_snprintf(accelStr, sizeof(accelStr), "Acc: %d", joust.rolling_average);
    plotText(0, OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)), accelStr, IBM_VGA_8, WHITE);
}

/**
 * Update the acceleration for the Joust mode
 */
void ICACHE_FLASH_ATTR joustAccelerometerHandler(accel_t* accel)
{

    if(joust.gameState != R_MENU)
    {
        joust.joustAccel.x = accel->x;
        joust.joustAccel.y = accel->y;
        joust.joustAccel.z = accel->z;
        uint16_t mov = (uint16_t) sqrt(pow(joust.joustAccel.x, 2) + pow(joust.joustAccel.y, 2) + pow(joust.joustAccel.z, 2));
        joust.rolling_average = (joust.rolling_average * 2 + mov) / 3;

        if (joust.gameState == R_PLAYING || joust.gameState == R_PLAYINGFFA)
        {
            if(mov > joust.rolling_average + 40)
            {
                if(joust.gameState == R_PLAYING)
                {
                    joust.gameState = R_GAME_OVER;
                    joustSendRoundLossMsg();
                }
                else
                {
                    joustRoundResultFFA();
                }
            }
            else
            {
                joustUpdateDisplay();
            }
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
        if(2 == button)
        {
            joust.gameState =  R_SEARCHING;
            joustDisarmAllLedTimers();
            os_timer_arm(&joust.tmr.ConnLed, 1, true);
            p2pStartConnection(&joust.p2pJoust);
            clearDisplay();
            plotText(0, 0, "Searching", IBM_VGA_8, WHITE);
        }
        else if(1 == button)
        {
            joust.gameState =  R_WAITING;
            joustDisarmAllLedTimers();
            joust.led.currBrightness = 0;
            joust.led.ConnLedState = LED_CONNECTED_BRIGHT;
            os_timer_arm(&joust.tmr.ShowConnectionLedFFA, 50, true);
            clearDisplay();
            plotText(0, 0, "GET READY", IBM_VGA_8, WHITE);
            plotText(0, OLED_HEIGHT - (5 * (FONT_HEIGHT_IBMVGA8 + 1)) + 3, "TO JOUST!", IBM_VGA_8, WHITE);
            plotText(0, OLED_HEIGHT - (3 * (FONT_HEIGHT_IBMVGA8 + 1)), "Move theirs", IBM_VGA_8, WHITE);
            plotText(0, OLED_HEIGHT - (2 * (FONT_HEIGHT_IBMVGA8 + 1)), "Not yours!", IBM_VGA_8, WHITE);
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
    // Send a message to that ESP that we lost the round
    // If it's acked, start a timer to reinit if another message is never received
    // If it's not acked, reinit with refRestart()
    p2pSendMsg(&joust.p2pJoust, "los", NULL, 0, joustMsgTxCbFn);
    // Show the current wins & losses
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
            joustDisarmAllLedTimers();
            os_timer_arm(&joust.tmr.RestartJoust, 100, false);
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

    setLeds(joust.led.Leds, sizeof(joust.led.Leds));
}

/**
 * Show the wins and losses
 *
 * @param roundWinner true if this swadge was a winner, false if the other
 *                    swadge won
 */
void ICACHE_FLASH_ATTR joustRoundResult(int roundWinner)
{
    joust.gameState = R_SHOW_GAME_RESULT;
    joustDisarmAllLedTimers();
    startBuzzerSong(&endGameSFX);
    joust.gam.round_winner = roundWinner;
    os_timer_arm(&joust.tmr.RoundResultLed, 6, true);
    joust.gameState = R_SHOW_GAME_RESULT;
    if(roundWinner == 2)
    {
        clearDisplay();
        plotText(0, 0, "Tie!!", IBM_VGA_8, WHITE);
        joust.gam.joustWins = joust.gam.joustWins + 1;
        startBuzzerSong(&tieGameSFX);
    }
    else if(roundWinner)
    {
        clearDisplay();
        plotText(0, 0, "Winner!!", IBM_VGA_8, WHITE);
        joust.gam.joustWins = joust.gam.joustWins + 1;
        if( joust.gam.joustWins >= 12)
        {

            // 0 means Bongos
            if(true == unlockGallery(0))
            {
                // Print gallery unlock
                fillDisplayArea(
                    JOUST_FIELD_OFFSET_X + 5,
                    JOUST_FIELD_OFFSET_Y + (JOUST_FIELD_HEIGHT / 2) - FONT_HEIGHT_IBMVGA8 - 2 - 2,
                    JOUST_FIELD_OFFSET_X + JOUST_FIELD_WIDTH - 1 - 5,
                    JOUST_FIELD_OFFSET_Y + (JOUST_FIELD_HEIGHT / 2) + FONT_HEIGHT_IBMVGA8 + 1 + 2,
                    BLACK);
                plotRect(
                    JOUST_FIELD_OFFSET_X + 5,
                    JOUST_FIELD_OFFSET_Y + (JOUST_FIELD_HEIGHT / 2) - FONT_HEIGHT_IBMVGA8 - 2 - 2,
                    JOUST_FIELD_OFFSET_X + JOUST_FIELD_WIDTH - 1 - 5,
                    JOUST_FIELD_OFFSET_Y + (JOUST_FIELD_HEIGHT / 2) + FONT_HEIGHT_IBMVGA8 + 1 + 2,
                    WHITE);
                plotText(
                    JOUST_FIELD_OFFSET_X + 8 + 4,
                    JOUST_FIELD_OFFSET_Y + (JOUST_FIELD_HEIGHT / 2) - (FONT_HEIGHT_IBMVGA8) - 1,
                    "Gallery",
                    IBM_VGA_8,
                    WHITE);
                plotText(
                    JOUST_FIELD_OFFSET_X + 8,
                    JOUST_FIELD_OFFSET_Y + (JOUST_FIELD_HEIGHT / 2) + 1,
                    "Unlocked",
                    IBM_VGA_8,
                    WHITE);
            }
        }
        if(joust.gam.joustWins % 2 == 0)
        {
            startBuzzerSong(&endGameWinSFX);

        }
        else
        {
            startBuzzerSong(&endGameWin2SFX);

        }
    }
    else
    {
        clearDisplay();

        plotText(0, 0, "Loser", IBM_VGA_8, WHITE);
        startBuzzerSong(&endGameSFX);
    }
    setJoustWins(joust.gam.joustWins);
    os_timer_arm(&joust.tmr.RestartJoust, 6000, false);
}

/**
 * Show the wins and losses
 *
 * @param roundWinner true if this swadge was a winner, false if the other
 *                    swadge won
 */
void ICACHE_FLASH_ATTR joustRoundResultFFA()
{
    joust.gameState = R_SHOW_GAME_RESULT;
    joustDisarmAllLedTimers();
    startBuzzerSong(&endGameSFX);
    os_timer_arm(&joust.tmr.RoundResultLed, 6, true);

    clearDisplay();
    plotText(0, 0, "GAME OVER", IBM_VGA_8, WHITE);
    char menuStr[32] = {0};
    ets_snprintf(menuStr, sizeof(menuStr), "SCORE: %d", joust.FFACounter);
    plotText(0, OLED_HEIGHT - (3 * (FONT_HEIGHT_IBMVGA8 + 1)), menuStr, IBM_VGA_8, WHITE);
    os_timer_arm(&joust.tmr.RestartJoust, 6000, false);
}


/**
 * count up for FFA
 */
void ICACHE_FLASH_ATTR joustFFACounter(void* arg __attribute__((unused)))
{
    joust.FFACounter += 1;
}
