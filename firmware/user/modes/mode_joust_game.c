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
#include <mem.h>

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
#include "fastlz.h"
#include "mode_tiltrads.h"

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

#define WARNING_THRESHOLD 20

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
    R_GAME_OVER,
    R_WARNING
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
void ICACHE_FLASH_ATTR joustDisplayWarning(void);
void ICACHE_FLASH_ATTR joustClearWarning(void* arg);
void ICACHE_FLASH_ATTR joustDrawMenu(void);
void ICACHE_FLASH_ATTR joustScrollInstructions(void* arg);

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
    uint16_t mov;
    uint16_t meterSize;
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
        os_timer_t ClearWarning;
        os_timer_t ScrollInstructions;
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

    int16_t instructionTextIdx;
} joust;

bool joustWarningShown = false;

/*============================================================================
 * Functions
 *==========================================================================*/

// Music / SFX

const song_t endGameSFX RODATA_ATTR =
{
    .notes = {
        {.note = C_5, .timeMs = 200},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_6, .timeMs = 200},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_5, .timeMs = 200},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_6, .timeMs = 200},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_5, .timeMs = 200},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_6, .timeMs = 200},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_5, .timeMs = 200},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_6, .timeMs = 200},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_5, .timeMs = 200},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_6, .timeMs = 200},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_5, .timeMs = 200},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_6, .timeMs = 200},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_5, .timeMs = 200},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_6, .timeMs = 200},
        {.note = SILENCE, .timeMs = 1},
    },
    .numNotes = 28,
    .shouldLoop = false
};

const song_t WinGameBachSFX RODATA_ATTR =
{
    .notes = {
        {.note = D_6, .timeMs = 300},
        {.note = G_5, .timeMs = 200},
        {.note = A_5, .timeMs = 200},
        {.note = B_5, .timeMs = 200},
        {.note = C_6, .timeMs = 200},
        {.note = D_6, .timeMs = 300},
        {.note = G_5, .timeMs = 300},
        {.note = SILENCE, .timeMs = 50},
        {.note = G_5, .timeMs = 300},
        {.note = E_6, .timeMs = 300},
        {.note = C_6, .timeMs = 200},
        {.note = D_6, .timeMs = 200},
        {.note = E_6, .timeMs = 200},
        {.note = F_SHARP_6, .timeMs = 200},
        {.note = G_6, .timeMs = 300},
        {.note = G_5, .timeMs = 300},
        {.note = SILENCE, .timeMs = 50},
        {.note = G_5, .timeMs = 500},

    },
    .numNotes = 18,
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

const song_t joustbeepSFX RODATA_ATTR =
{
    .notes = {
        {.note = C_8, .timeMs = 40},
        {.note = SILENCE, .timeMs = 40},
        {.note = C_8, .timeMs = 40},
        {.note = SILENCE, .timeMs = 40},
        {.note = C_8, .timeMs = 40},
        {.note = SILENCE, .timeMs = 1},
    },
    .numNotes = 6,
    .shouldLoop = false
};


const uint8_t JoustWarning[666] RODATA_ATTR =
{
    0x06, 0x00, 0x80, 0x00, 0x40, 0x00, 0x01, 0x00, 0xe0, 0x3e, 0x00, 0x01, 0x07, 0xc0, 0xa0, 0x48,
    0x00, 0x70, 0x80, 0x07, 0x01, 0x07, 0xe0, 0x80, 0x07, 0xe0, 0x00, 0x0f, 0xc0, 0x1f, 0xe0, 0x06,
    0x00, 0x01, 0x07, 0xf0, 0x80, 0x10, 0x03, 0x04, 0x20, 0x03, 0x83, 0x40, 0x22, 0x04, 0x07, 0xf0,
    0x07, 0xc7, 0xf8, 0x60, 0x12, 0x09, 0x07, 0xe7, 0xfc, 0x00, 0x3f, 0x03, 0x00, 0x00, 0x07, 0xff,
    0x20, 0x00, 0x04, 0x83, 0x04, 0x00, 0x07, 0xef, 0x20, 0x07, 0x16, 0xfe, 0x07, 0xf0, 0x02, 0xd7,
    0xff, 0xff, 0x80, 0xfe, 0x04, 0x00, 0x01, 0x17, 0x3f, 0xff, 0x00, 0x0c, 0x00, 0x00, 0x21, 0x16,
    0xb9, 0xfe, 0x40, 0x30, 0x03, 0x19, 0x95, 0x76, 0xff, 0x20, 0x2d, 0x15, 0xf0, 0x0d, 0xfa, 0xef,
    0x7f, 0xc0, 0x00, 0x04, 0x10, 0x07, 0xfe, 0x1f, 0xbf, 0xf0, 0x00, 0x06, 0xe0, 0x07, 0xff, 0xdf,
    0xbf, 0xfc, 0x20, 0x20, 0x03, 0x1f, 0xff, 0x33, 0xc7, 0x20, 0x20, 0x06, 0x00, 0x07, 0xfe, 0x21,
    0xc0, 0x07, 0xc0, 0x20, 0x67, 0x13, 0xfe, 0x20, 0xe0, 0x03, 0xe0, 0x00, 0x80, 0x0f, 0xff, 0x00,
    0xe0, 0x00, 0xfc, 0x87, 0xf0, 0x3f, 0xfe, 0xc0, 0xe0, 0x00, 0x20, 0x39, 0x09, 0x01, 0x64, 0x20,
    0x70, 0x00, 0x07, 0x80, 0x00, 0x02, 0x24, 0x80, 0xbb, 0x01, 0x04, 0x44, 0x80, 0x07, 0x21, 0x2f,
    0x00, 0x30, 0x20, 0x3d, 0x00, 0x00, 0x20, 0x00, 0x20, 0x07, 0x00, 0x00, 0x40, 0xb7, 0x20, 0x07,
    0x60, 0x0f, 0x00, 0x38, 0x20, 0x0f, 0x40, 0x00, 0x00, 0x18, 0x40, 0x04, 0x01, 0x00, 0x03, 0x40,
    0x07, 0x20, 0xd7, 0x01, 0x07, 0x80, 0x20, 0x0f, 0x03, 0x04, 0x10, 0x00, 0x00, 0x40, 0x07, 0x20,
    0x0f, 0x01, 0x00, 0xc0, 0x20, 0x0f, 0x40, 0x00, 0x00, 0x40, 0xa0, 0x07, 0x00, 0x60, 0x20, 0x07,
    0x40, 0x17, 0x40, 0x07, 0x16, 0x00, 0x10, 0x07, 0x00, 0xe0, 0x18, 0xf8, 0x06, 0x07, 0xf0, 0x0f,
    0xc3, 0xe0, 0x1b, 0xff, 0xcc, 0x00, 0x00, 0x0f, 0xe7, 0xff, 0xdb, 0xff, 0x20, 0xb8, 0x01, 0x0f,
    0xff, 0x40, 0x07, 0x20, 0xaf, 0x20, 0x07, 0x0b, 0xf0, 0x18, 0x04, 0x80, 0x00, 0x3f, 0xff, 0xdb,
    0x80, 0x00, 0x07, 0x70, 0x40, 0x07, 0x20, 0x37, 0x00, 0x00, 0xe0, 0x00, 0x07, 0x02, 0x1f, 0xff,
    0xd7, 0x41, 0x41, 0x20, 0x30, 0x00, 0xd7, 0x20, 0x6c, 0x20, 0x5a, 0x01, 0x8f, 0xd7, 0x20, 0x3e,
    0x20, 0x8f, 0x14, 0x80, 0x57, 0xff, 0x00, 0x07, 0x30, 0x00, 0x01, 0xc0, 0x10, 0x1f, 0xc0, 0x04,
    0x90, 0x00, 0x00, 0xf0, 0x10, 0x0f, 0xf0, 0x06, 0x20, 0x22, 0x02, 0x38, 0x00, 0x03, 0x40, 0x20,
    0x04, 0x02, 0x1c, 0x00, 0x00, 0x7e, 0x20, 0x27, 0x01, 0x02, 0x0e, 0x20, 0x43, 0x20, 0x8f, 0x00,
    0x02, 0x20, 0x0b, 0x04, 0x07, 0x00, 0x10, 0x00, 0x06, 0x20, 0x07, 0x00, 0x03, 0x20, 0x0f, 0x40,
    0x07, 0x40, 0x00, 0xe0, 0x04, 0x07, 0xc0, 0x17, 0x20, 0x8f, 0x60, 0x17, 0x20, 0x8f, 0x60, 0x07,
    0x20, 0x00, 0xe0, 0x04, 0x07, 0x60, 0x27, 0x20, 0xc2, 0x20, 0x27, 0x00, 0x03, 0x20, 0xa1, 0x06,
    0xff, 0x87, 0x70, 0x00, 0x03, 0x80, 0x3f, 0x21, 0xc1, 0x20, 0x5b, 0xc0, 0x07, 0x00, 0x84, 0x20,
    0x0f, 0x04, 0x87, 0xf0, 0x00, 0x83, 0x88, 0x20, 0x07, 0x04, 0x84, 0x10, 0x00, 0x47, 0xd0, 0x20,
    0x07, 0x20, 0x0f, 0x02, 0x3f, 0xe0, 0xbf, 0x60, 0x27, 0x01, 0x1f, 0xf3, 0x20, 0x0f, 0x20, 0x2f,
    0x01, 0x9f, 0xfc, 0x20, 0x07, 0x20, 0x17, 0x01, 0x7f, 0xfa, 0x20, 0x07, 0x04, 0x80, 0x10, 0x00,
    0x1f, 0xfb, 0x20, 0x07, 0x20, 0x0f, 0x01, 0x3f, 0xfb, 0x80, 0x27, 0x01, 0x7f, 0xe5, 0x22, 0x20,
    0x20, 0x27, 0x02, 0x8f, 0xeb, 0xdf, 0x60, 0x17, 0x04, 0x13, 0x2f, 0x9f, 0xff, 0xff, 0x22, 0xe0,
    0x01, 0x02, 0xdf, 0x20, 0x94, 0x21, 0xbd, 0x02, 0x05, 0xf8, 0x00, 0x60, 0x8f, 0x01, 0x0f, 0xe0,
    0x20, 0x07, 0x20, 0x00, 0x00, 0x1f, 0x61, 0x2d, 0x02, 0x00, 0x00, 0x0f, 0x20, 0x33, 0x40, 0xa7,
    0x00, 0x0f, 0x20, 0x06, 0x61, 0xaf, 0x40, 0x00, 0x22, 0x19, 0xe0, 0x0c, 0x00, 0x20, 0xcf, 0xa0,
    0x00, 0x00, 0x08, 0x20, 0x52, 0x40, 0x00, 0x01, 0x1c, 0x01, 0x40, 0x47, 0x02, 0x07, 0xf0, 0x3e,
    0x60, 0x32, 0x00, 0x01, 0x21, 0x42, 0x40, 0x0f, 0x22, 0x57, 0x00, 0x8f, 0x40, 0x1f, 0x21, 0xb7,
    0x60, 0x7e, 0x20, 0x51, 0x62, 0xa5, 0x20, 0x21, 0x41, 0x73, 0x20, 0x17, 0x00, 0x00, 0x40, 0x8e,
    0x02, 0x00, 0x04, 0x50, 0xa0, 0x0f, 0x00, 0x60, 0xc0, 0x1f, 0xc0, 0x2f, 0xa0, 0x3f, 0x00, 0x30,
    0x80, 0x4f, 0x01, 0x04, 0x90, 0x80, 0x5f, 0x01, 0x06, 0xf0, 0x80, 0x6f, 0xc0, 0x7f, 0x01, 0x07,
    0xd0, 0x20, 0x0a, 0xe0, 0x31, 0x00, 0x02, 0x00, 0x00, 0x00
};

/**
 * @brief Helper function to display the warning image
 */
void ICACHE_FLASH_ATTR joustDisplayWarning(void)
{
    /* Read the compressed image from ROM into RAM, and make sure to do a
     * 32 bit aligned read. The arrays are all __attribute__((aligned(4)))
     * so this is safe, not out of bounds
     */
    uint32_t alignedSize = sizeof(JoustWarning);
    while(alignedSize % 4 != 0)
    {
        alignedSize++;
    }
    uint8_t* compressedStagingSpace = (uint8_t*)os_malloc(alignedSize);
    memcpy(compressedStagingSpace, JoustWarning, alignedSize);

    // Decompress the image from one RAM area to another
    uint8_t* decompressedImage = (uint8_t*)os_malloc(1024 + 8);
    fastlz_decompress(compressedStagingSpace,
                      sizeof(JoustWarning),
                      decompressedImage,
                      1024 + 8);

    // Draw the decompressed image to the OLED
    for (int w = 0; w < OLED_WIDTH; w++)
    {
        for (int h = 0; h < OLED_HEIGHT; h++)
        {
            uint16_t linearIdx = (OLED_HEIGHT * w) + h;
            uint16_t byteIdx = linearIdx / 8;
            uint8_t bitIdx = linearIdx % 8;

            if (decompressedImage[8 + byteIdx] & (0x80 >> bitIdx))
            {
                drawPixel(w, h, WHITE);
            }
            else
            {
                drawPixel(w, h, BLACK);
            }
        }
    }

    // Free memory
    os_free(compressedStagingSpace);
    os_free(decompressedImage);
}

/**
 * @brief Switch the game mode to R_MENU from R_WARNING and draw the menu
 *
 * @param arg unused
 */
void ICACHE_FLASH_ATTR joustClearWarning(void* arg __attribute__((unused)) )
{
    joust.gameState = R_MENU;
    // Draw the  menu
    joust.instructionTextIdx = OLED_WIDTH;
    joustDrawMenu();
    // Start the timer to scroll text
    os_timer_arm(&joust.tmr.ScrollInstructions, 34, true);
}

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
        case R_WARNING:
        {
            break;
        }
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

    // Set up a timer to clear the warning
    os_timer_disarm(&joust.tmr.ClearWarning);
    os_timer_setfn(&joust.tmr.ClearWarning, joustClearWarning, NULL);

    // Set up a timer to scroll the instructions
    os_timer_disarm(&joust.tmr.ScrollInstructions);
    os_timer_setfn(&joust.tmr.ScrollInstructions, joustScrollInstructions, NULL);

    // If the warning wasn't shown yet
    if(false == joustWarningShown)
    {
        // Display a warning and start a timer to clear it
        joustDisplayWarning();
        os_timer_arm(&joust.tmr.ClearWarning, 1000 * 5, false);
        joustWarningShown = true;
        joust.gameState = R_WARNING;
    }
    else
    {
        // Otherwise just show the menu
        joustClearWarning(NULL);
        joust.gameState = R_MENU;
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
}

/**
 * @brief Draw the Joust menu with a title, levels, instructions, and button labels
 */
void ICACHE_FLASH_ATTR joustDrawMenu(void)
{
#define Y_MARGIN 6
    uint8_t textY = 0;

    clearDisplay();

    // Draw title
    plotText(32, textY, "Joust", RADIOSTARS, WHITE);
    textY += FONT_HEIGHT_RADIOSTARS + Y_MARGIN;
    // Draw instruction ticker
    if (0 > plotText(joust.instructionTextIdx, textY, "Joust is a multiplayer movement game where you try to jostle your opponents swadge while keeping yours still. 2 Player mode tracks your wins. Free For All does not, but supports any number of players - make sure everyone presses Start at the same time! Wrap your lanyard around your wrist to prevent dropping your swadge. Sound ON is highly recommended. Enjoy!", IBM_VGA_8, WHITE))
    {
        joust.instructionTextIdx = OLED_WIDTH;
    }
    textY += FONT_HEIGHT_IBMVGA8 + Y_MARGIN;
    // Draw level info. First figure out what level we're at
    int16_t nextLevel = 0;
    char lvlStr[32] = {0};
    if(joust.gam.joustWins < 4)
    {
        memcpy(lvlStr, "Serf Simian", sizeof("Serf Simian"));
        nextLevel = 4;
    }
    else if(joust.gam.joustWins < 8)
    {
        memcpy(lvlStr, "Peasant Primate", sizeof("Peasant Primate"));
        nextLevel = 8;
    }
    else if(joust.gam.joustWins < 12)
    {
        memcpy(lvlStr, "Page Probocsis", sizeof("Page Probocsis"));
        nextLevel = 12;
    }
    else if(joust.gam.joustWins < 16)
    {
        memcpy(lvlStr, "Squire Saki", sizeof("Squire Saki"));
        nextLevel = 16;
    }
    else if(joust.gam.joustWins < 22)
    {
        memcpy(lvlStr, "Apprentice Ape", sizeof("Apprentice Ape"));
        nextLevel = 22;
    }
    else if(joust.gam.joustWins < 28)
    {
        memcpy(lvlStr, "Maester Mandrill", sizeof("Maester Mandrill"));
        nextLevel = 28;
    }
    else if(joust.gam.joustWins < 36)
    {
        memcpy(lvlStr, "Thane Tamarin", sizeof("Thane Tamarin"));
        nextLevel = 36;
    }
    else if(joust.gam.joustWins < 44)
    {
        memcpy(lvlStr, "Lord Lemur", sizeof("Lord Lemur"));
        nextLevel = 44;
    }
    else if(joust.gam.joustWins < 60)
    {
        memcpy(lvlStr, "Baron Baboon", sizeof("Baron Baboon"));
        nextLevel = 60;
    }
    else if(joust.gam.joustWins < 100)
    {
        memcpy(lvlStr, "Viscount Vervet", sizeof("Viscount Vervet"));
    }
    else
    {
        memcpy(lvlStr, "Grandmaster Gorilla", sizeof("Grandmaster Gorilla"));
    }

    char menuStr[64] = {0};
    // First row, left justified
    ets_snprintf(menuStr, sizeof(menuStr), "%d Wins!", joust.gam.joustWins);
    plotText(0, textY, menuStr, TOM_THUMB, WHITE);

    // First row, right justified, maybe
    if(0 != nextLevel)
    {
        ets_snprintf(menuStr, sizeof(menuStr), "Next lvl at %d wins", nextLevel);
        plotText(OLED_WIDTH - getTextWidth(menuStr, TOM_THUMB), textY, menuStr, TOM_THUMB, WHITE);
    }
    textY += FONT_HEIGHT_TOMTHUMB + Y_MARGIN;

    // Second row
    ets_snprintf(menuStr, sizeof(menuStr), "You are a %s", lvlStr);
    plotText(0, textY, menuStr, TOM_THUMB, WHITE);
    textY += FONT_HEIGHT_TOMTHUMB + Y_MARGIN;



    // Draw button labels
    plotRect(
        -1,
        OLED_HEIGHT - FONT_HEIGHT_TOMTHUMB - 4,
        getTextWidth("Free For All", TOM_THUMB) + 3,
        OLED_HEIGHT + 1,
        WHITE);
    plotText(
        0,
        OLED_HEIGHT - FONT_HEIGHT_TOMTHUMB,
        "Free For All",
        TOM_THUMB,
        WHITE);

    plotRect(
        OLED_WIDTH - getTextWidth("2 Player", TOM_THUMB) - 4,
        OLED_HEIGHT - FONT_HEIGHT_TOMTHUMB - 4,
        OLED_WIDTH + 1,
        OLED_HEIGHT + 1,
        WHITE);
    plotText(
        OLED_WIDTH - getTextWidth("2 Player", TOM_THUMB),
        OLED_HEIGHT - FONT_HEIGHT_TOMTHUMB,
        "2 Player",
        TOM_THUMB,
        WHITE);
}

/**
 * @brief Decrement the index to draw the instructions at, then draw the menu
 *
 * @param arg unused
 */
void ICACHE_FLASH_ATTR joustScrollInstructions(void* arg __attribute__((unused)))
{
    joust.instructionTextIdx--;
    joustDrawMenu();
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
            if(joust.led.currBrightness == 64)
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

    // When there's a warning, flash the LEDs
    uint32_t ledColor = joust.con_color;
    uint8_t ledBright = joust.led.currBrightness;
    if(joust.mov > joust.rolling_average + WARNING_THRESHOLD)
    {
        ledBright = 150;
        ledColor = 0;
    }

    uint8_t i;
    for(i = 0; i < 6; i++)
    {
        joust.led.Leds[i].r = (EHSVtoHEX(ledColor, 255, ledBright) >>  0) & 0xFF;
        joust.led.Leds[i].g = (EHSVtoHEX(ledColor, 255, ledBright) >>  8) & 0xFF;
        joust.led.Leds[i].b = (EHSVtoHEX(ledColor, 255, ledBright) >> 16) & 0xFF;
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

    // Figure out what title to draw
    static uint8_t showWarningFrames = 0;
    if(joust.mov > joust.rolling_average + WARNING_THRESHOLD)
    {
        // Buzz a little as a warning
        startBuzzerSong(&joustbeepSFX);
        showWarningFrames = 15;
    }
    else if(0 < showWarningFrames)
    {
        showWarningFrames--;
    }

    // Draw a title
    if(0 < showWarningFrames)
    {
        plotCenteredText(0, 0, OLED_WIDTH, "!! Warning !!", RADIOSTARS, WHITE);
    }
    else
    {
        plotCenteredText(0, 0, OLED_WIDTH, "Joust Meter", RADIOSTARS, WHITE);
    }

    // Display the acceleration on the display
    plotRect(
        0,
        (OLED_HEIGHT / 2 - 18) + 5,
        OLED_WIDTH - 2,
        (OLED_HEIGHT / 2 + 18) + 5,
        WHITE);

    // Find the difference from the rolling average, scale it to 220px (5px margin on each side)
    int16_t diffFromAvg = ((joust.mov - joust.rolling_average) * 220) / 43;
    // Clamp it to the meter's draw range
    if(diffFromAvg < 0)
    {
        diffFromAvg = 0;
    }
    if(diffFromAvg > 118)
    {
        diffFromAvg = 118;
    }

    // Either set the meter if the difference is high, or slowly clear it
    if(diffFromAvg > joust.meterSize)
    {
        joust.meterSize = diffFromAvg;
    }
    else
    {
        if(joust.meterSize >= 12)
        {
            joust.meterSize -= 12;
        }
        else
        {
            joust.meterSize = 0;
        }
    }

    // draw it
    fillDisplayArea(
        5,
        (OLED_HEIGHT / 2 - 14) + 5,
        5 + joust.meterSize,
        (OLED_HEIGHT / 2 + 14) + 5,
        WHITE);
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
        joust.mov = (uint16_t) sqrt(pow(joust.joustAccel.x, 2) + pow(joust.joustAccel.y, 2) + pow(joust.joustAccel.z, 2));
        joust.rolling_average = (joust.rolling_average * 2 + joust.mov) / 3;

        if (joust.gameState == R_PLAYING || joust.gameState == R_PLAYINGFFA)
        {
            if(joust.mov > joust.rolling_average + 43)
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

    if(joust.gameState == R_MENU)
    {
        // Stop the scrolling text
        os_timer_disarm(&joust.tmr.ScrollInstructions);

        if(2 == button)
        {
            joust.gameState =  R_SEARCHING;
            joustDisarmAllLedTimers();
            os_timer_arm(&joust.tmr.ConnLed, 1, true);
            p2pStartConnection(&joust.p2pJoust);
            clearDisplay();
            plotText(0, 0, "Searching", IBM_VGA_8, WHITE);
            plotText(0, OLED_HEIGHT - (3 * (FONT_HEIGHT_IBMVGA8 + 1) - 3), "Stand near your", IBM_VGA_8, WHITE);
            plotText(0, OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)), "opponent now!", IBM_VGA_8, WHITE);


        }
        else if(1 == button)
        {
            joust.gameState =  R_WAITING;
            joustDisarmAllLedTimers();
            joust.led.currBrightness = 0;
            joust.led.ConnLedState = LED_CONNECTED_BRIGHT;
            joust.con_color = 140; // blue-green
            os_timer_arm(&joust.tmr.ShowConnectionLedFFA, 50, true);
            clearDisplay();
            plotText(0, 0, "GET READY", IBM_VGA_8, WHITE);
            plotText(0, OLED_HEIGHT - (5 * (FONT_HEIGHT_IBMVGA8 + 1)) + 3, "TO JOUST!", IBM_VGA_8, WHITE);
            plotText(0, OLED_HEIGHT - (3 * (FONT_HEIGHT_IBMVGA8 + 1)), "Move theirs", IBM_VGA_8, WHITE);
            plotText(0, OLED_HEIGHT - (2 * (FONT_HEIGHT_IBMVGA8 + 1)), "Not yours!", IBM_VGA_8, WHITE);
        }
    }
    else if(joust.gameState == R_SHOW_GAME_RESULT || joust.gameState == R_SEARCHING)
    {
        if(1 == button || 2 == button)
        {
            os_timer_arm(&joust.tmr.RestartJoust, 10, false);
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
        if(joust.gam.joustWins % 10 == 0)
        {
            startBuzzerSong(&WinGameBachSFX);

        }
        else if(joust.gam.joustWins % 2 == 0)
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
    //One if 15 games has the Bach sound
    if(joust.FFACounter % 15 == 0)
    {
        startBuzzerSong(&WinGameBachSFX);
    }
    else
    {
        startBuzzerSong(&endGameSFX);
    }

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
