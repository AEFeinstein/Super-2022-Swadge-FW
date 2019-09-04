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
#include <p2pConnection.h>
#include <math.h>
// #include <string>

#include "user_main.h"
#include "mode_joust_game.h"
#include "custom_commands.h"
#include "buttons.h"
#include "oled.h"
#include "font.h"
/*============================================================================
 * Defines
 *==========================================================================*/

#define JOUST_DEBUG_PRINT
#ifdef JOUST_DEBUG_PRINT
    #define joust_printf(...) os_printf(__VA_ARGS__)
#else
    #define joust_printf(...)
#endif

// The time we'll spend retrying messages
#define RETRY_TIME_MS 3000

// Minimum RSSI to accept a connection broadcast
// #define CONNECTION_RSSI 55

// Degrees between each LED
#define DEG_PER_LED 60

// Time to wait between connection events and game rounds.
// Transmission can be 3s (see above), the round @ 12ms period is 3.636s
// (240 steps of rotation + (252/4) steps of decay) * 12ms
#define FAILURE_RESTART_MS 8000

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
    R_MENU,
    R_SEARCHING,
    R_CONNECTING,
    R_SHOW_CONNECTION,
    R_PLAYING,
    R_WAITING,
    R_SHOW_GAME_RESULT
} joustGameState_t;

// typedef enum
// {
//     GOING_SECOND,
//     GOING_FIRST
// } playOrder_t;

// typedef enum
// {
//     RX_GAME_START_ACK,
//     RX_GAME_START_MSG
// } connectionEvt_t;

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

// typedef enum
// {
//     ACT_CLOCKWISE        = 0,
//     ACT_COUNTERCLOCKWISE = 1,
//     ACT_BOTH             = 2
// } gameAction_t;

// typedef enum
// {
//     EASY   = 0,
//     MEDIUM = 1,
//     HARD   = 2
// } difficulty_t;

// typedef enum
// {
//     NOT_DISPLAYING,
//     FAIL_DISPLAY_ON_1,
//     FAIL_DISPLAY_OFF_2,
//     FAIL_DISPLAY_ON_3,
//     FAIL_DISPLAY_OFF_4,
//     SCORE_DISPLAY_INIT,
//     FIRST_DIGIT_INC,
//     FIRST_DIGIT_OFF,
//     FIRST_DIGIT_ON,
//     FIRST_DIGIT_OFF_2,
//     SECOND_DIGIT_INC,
//     SECOND_DIGIT_OFF,
//     SECOND_DIGIT_ON,
//     SECOND_DIGIT_OFF_2,
//     SCORE_DISPLAY_FINISH
// } singlePlayerScoreDisplayState_t;

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
void ICACHE_FLASH_ATTR refStartRestartTimer(void* arg __attribute__((unused)));
void ICACHE_FLASH_ATTR refSinglePlayerRestart(void* arg __attribute__((unused)));
void ICACHE_FLASH_ATTR refSinglePlayerScoreLed(uint8_t ledToLight, led_t* colorPrimary, led_t* colorSecondary);
void ICACHE_FLASH_ATTR joustConnectionCallback(p2pInfo* p2p, connectionEvt_t event);
void ICACHE_FLASH_ATTR joustMsgCallbackFn(p2pInfo* p2p, char* msg, uint8_t* payload, uint8_t len);
void ICACHE_FLASH_ATTR joustMsgTxCbFn(p2pInfo* p2p, messageStatus_t status);

// Transmission Functions
void ICACHE_FLASH_ATTR joustSendMsg(char* msg, uint16_t len, bool shouldAck, void (*success)(void*),
                                  void (*failure)(void*));
void ICACHE_FLASH_ATTR refSendAckToMac(uint8_t* mac_addr);
void ICACHE_FLASH_ATTR joustTxAllRetriesTimeout(void* arg __attribute__((unused)) );
void ICACHE_FLASH_ATTR joustTxRetryTimeout(void* arg);

// Connection functions
void ICACHE_FLASH_ATTR joustConnectionTimeout(void* arg __attribute__((unused)));
void ICACHE_FLASH_ATTR refGameStartAckRecv(void* arg __attribute__((unused)));
void ICACHE_FLASH_ATTR refProcConnectionEvt(connectionEvt_t event);

// Game functions
void ICACHE_FLASH_ATTR joustStartPlaying(void* arg __attribute__((unused)));
void ICACHE_FLASH_ATTR joustStartRound(void);
void ICACHE_FLASH_ATTR joustSendRoundLossMsg(void);
void ICACHE_FLASH_ATTR refAdjustledSpeed(bool reset, bool up);
void ICACHE_FLASH_ATTR joustAccelerometerHandler(accel_t* accel);

// LED Functions
void ICACHE_FLASH_ATTR joustDisarmAllLedTimers(void);
void ICACHE_FLASH_ATTR joustConnLedTimeout(void* arg __attribute__((unused)));
void ICACHE_FLASH_ATTR joustShowConnectionLedTimeout(void* arg __attribute__((unused)));
void ICACHE_FLASH_ATTR refGameLedTimeout(void* arg __attribute__((unused)));
void ICACHE_FLASH_ATTR joustRoundResultLed(bool);

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
    .fnAudioCallback = NULL,
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
    // Game state variables
    struct
    {
        //gameAction_t Action;
        bool shouldTurnOnLeds;
        uint8_t Wins;
        uint8_t Losses;
        uint8_t ledPeriodMs;
        bool singlePlayer;
        uint16_t singlePlayerRounds;
        uint8_t singlePlayerTopHits;
        //difficulty_t difficulty;
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
        //singlePlayerScoreDisplayState_t singlePlayerDisplayState;
        uint8_t digitToDisplay;
        uint8_t ledsLit;
    } led;

    p2pInfo p2pJoust;
} joust;

// Colors
// static led_t digitCountFirstPrimary =
// {
//     .r = 0xFE,
//     .g = 0x87,
//     .b = 0x1D,
// };
// static led_t digitCountFirstSecondary =
// {
//     .r = 0x11,
//     .g = 0x39,
//     .b = 0xFE,
// };
// static led_t digitCountSecondPrimary =
// {
//     .r = 0xCB,
//     .g = 0x1F,
//     .b = 0xFF,
// };
// static led_t digitCountSecondSecondary =
// {
//     .r = 0x00,
//     .g = 0xFC,
//     .b = 0x0F,
// };

/*============================================================================
 * Functions
 *==========================================================================*/

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
             joust.gameState = R_SHOW_CONNECTION;
             clearDisplay();
             plotText(0, 0, "Found", IBM_VGA_8);
             ets_memset(joust.led.Leds, 0, sizeof(joust.led.Leds));
             joust.led.ConnLedState = LED_CONNECTED_BRIGHT;

             joustDisarmAllLedTimers();
             // 6ms * ~500 steps == 3s animation
             //This is the start of the game
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

     switch(joust.gameState)
     {
         case R_CONNECTING:
             break;
         case R_WAITING:
         {
             // Received a message that the other swadge lost
             // if(0 == ets_memcmp(msg, "los", 3))
             // {
             //     // The other swadge lost, so chalk a win!
             //     // ref.gam.Wins++;
             //
             //     // Display the win
             //     joustRoundResultLed(true);
             // }
             // if(0 == ets_memcmp(msg, "cnt", 3))
             // {
             //     // Get faster or slower based on the other swadge's timing
             //     if(0 == ets_memcmp(payload, spdUp, ets_strlen(spdUp)))
             //     {
             //         refAdjustledSpeed(false, true);
             //     }
             //     else if(0 == ets_memcmp(payload, spdDn, ets_strlen(spdDn)))
             //     {
             //         refAdjustledSpeed(false, false);
             //     }
             //
             //     refStartRound();
             // }
             break;
         }
         case R_PLAYING:
         {
             if(0 == ets_memcmp(msg, "los", 3))
             {
                 // The other swadge lost, so chalk a win!
                 // ref.gam.Wins++;

                 // Display the win
                 joustRoundResultLed(true);
             }
             // Currently playing a game, if a message is sent, then update score
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


//were gonna need a lobby with lots of players
/**
 * Initialize everything and start sending broadcast messages
 */
void ICACHE_FLASH_ATTR joustInit(void)
{
    //just print for now
    joust_printf("\nwe are NOW in the JOUST MODE\n");
    joust_printf("%s\r\n", __func__);
    clearDisplay();
    plotText(0, 0, "Press button", IBM_VGA_8);
    // Enable button debounce for consistent 1p/2p and difficulty config
    enableDebounce(true);
    // Make sure everything is zero!
    ets_memset(&joust, 0, sizeof(joust));
    p2pInitialize(&joust.p2pJoust, "jou", joustConnectionCallback, joustMsgCallbackFn);



    // Get and save the string form of our MAC address
    // uint8_t mymac[6];
    // wifi_get_macaddr(SOFTAP_IF, mymac);
    // joust_printf("\ngonna print the mac string\n");
    // ets_sprintf(joust.cnc.macStr, macFmtStrjoust,
    //             mymac[0],
    //             mymac[1],
    //             mymac[2],
    //             mymac[3],
    //             mymac[4],
    //             mymac[5]);


     //we don't need a timer to show a successful connection, but we do need
     //to start the game eventually
    // Set up a timer for showing a successful connection, don't start it
    os_timer_disarm(&joust.tmr.ShowConnectionLed);
    os_timer_setfn(&joust.tmr.ShowConnectionLed, joustShowConnectionLedTimeout, NULL);

       //specific to reflector game, don't need this
//     // Set up a timer for showing the game, don't start it
//     os_timer_disarm(&ref.tmr.GameLed);
//     os_timer_setfn(&ref.tmr.GameLed, refGameLedTimeout, NULL);


    // Set up a timer for starting the next round, don't start it
    os_timer_disarm(&joust.tmr.StartPlaying);
    os_timer_setfn(&joust.tmr.StartPlaying, joustStartPlaying, NULL);

       //some is specific to reflector game, but we can still use some for
       //setting leds
    // Set up a timer to update LEDs, start it
    os_timer_disarm(&joust.tmr.ConnLed);
    os_timer_setfn(&joust.tmr.ConnLed, joustConnLedTimeout, NULL);

    //specific to reflector game
//     // Set up a timer to restart after failure. don't start it
//     os_timer_disarm(&ref.tmr.SinglePlayerRestart);
//     os_timer_setfn(&ref.tmr.SinglePlayerRestart, refSinglePlayerRestart, NULL);
//

    // p2pStartConnection(&joust.p2pJoust);
    joust.gameState = R_MENU;
    joust.p2pJoust.connectionRssi = 10;
    os_timer_arm(&joust.tmr.ConnLed, 1, true);
}


/**
 * Clean up all timers
 */
void ICACHE_FLASH_ATTR joustDeinit(void)
{

    joust_printf("%s\r\n", __func__);
    p2pDeinit(&joust.p2pJoust);
    // os_timer_disarm(&ref.tmr.StartPlaying);
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
    // os_timer_disarm(&ref.tmr.GameLed);
    // os_timer_disarm(&ref.tmr.SinglePlayerRestart);
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
    uint8_t currBrightness = joust.led.Leds[0].r;
    switch(joust.led.ConnLedState)
    {
        case LED_CONNECTED_BRIGHT:
        {
            currBrightness++;
            if(currBrightness == 0xFF)
            {
                joust.led.ConnLedState = LED_CONNECTED_DIM;
            }
            break;
        }
        case LED_CONNECTED_DIM:
        {
            currBrightness--;
            if(currBrightness == 0x00)
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
    ets_memset(joust.led.Leds, currBrightness, sizeof(joust.led.Leds));
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
    ets_memset(joust.led.Leds, 0, sizeof(joust.led.Leds));
    setLeds(joust.led.Leds, sizeof(joust.led.Leds));

    // // Reset the LED timer to the default speed
    // refAdjustledSpeed(true, true);
    //
    // Check for match end
    // joust_printf("wins: %d, losses %d\r\n", ref.gam.Wins, ref.gam.Losses);
    joust.gameState = R_PLAYING;
    // joustStartRound();
    // if(ref.gam.Wins == 3 || ref.gam.Losses == 3)
    // {
    //     // Tally match win in SPI flash
    //     if(ref.gam.Wins == 3)
    //     {
    //         incrementRefGameWins();
    //     }
    //
    //     // Match over, reset everything
    //     refRestart(NULL);
    // }
    // else if(GOING_FIRST == ref.cnc.playOrder)
    // {
    //     ref.gameState = R_PLAYING;
    //
    //     // Start playing
    //     refStartRound();
    // }
    // else if(GOING_SECOND == ref.cnc.playOrder)
    // {
    //     ref.gameState = R_WAITING;
    //
    //     ref.gam.receiveFirstMsg = false;
    //
    //     // Start a timer to reinit if we never receive a result (disconnect)
    //     refStartRestartTimer(NULL);
    // }
}

/**
 * Start a round of the game by picking a random action and starting
 * refGameLedTimeout()
 */
void ICACHE_FLASH_ATTR joustStartRound(void)
{
    joust.gameState = R_PLAYING;
    //
    // // pick a random game action
    // ref.gam.Action = os_random() % 3;
    //
    // // Set the LED's starting angle
    // switch(ref.gam.Action)
    // {
    //     case ACT_CLOCKWISE:
    //     {
    //         joust_printf("ACT_CLOCKWISE\r\n");
    //         ref.led.Degree = 300;
    //         break;
    //     }
    //     case ACT_COUNTERCLOCKWISE:
    //     {
    //         joust_printf("ACT_COUNTERCLOCKWISE\r\n");
    //         ref.led.Degree = 60;
    //         break;
    //     }
    //     case ACT_BOTH:
    //     {
    //         joust_printf("ACT_BOTH\r\n");
    //         ref.led.Degree = 0;
    //         break;
    //     }
    //     default:
    //     {
    //         break;
    //     }
    // }
    // ref.gam.shouldTurnOnLeds = true;
    //
    // // Clear the LEDs first
    // refDisarmAllLedTimers();
    // ets_memset(ref.led.Leds, 0, sizeof(ref.led.Leds));
    // setLeds(ref.led.Leds, sizeof(ref.led.Leds));
    // // Then set the game in motion
    // os_timer_arm(&ref.tmr.GameLed, ref.gam.ledPeriodMs, true);
}


void ICACHE_FLASH_ATTR joustUpdateDisplay(void)
{
    // Clear the display
    clearDisplay();
    // Draw a title
    plotText(0, 0, "JOUST", RADIOSTARS);
    // Display the acceleration on the display
    char accelStr[32] = {0};
    ets_snprintf(accelStr ,sizeof(accelStr), "X:%d" , joust.rolling_average);
    plotText(0, OLED_HEIGHT - (3 * (FONT_HEIGHT_IBMVGA8 + 1)), accelStr, IBM_VGA_8);

}

/**
 * Update the acceleration for the Joust mode
 */
void ICACHE_FLASH_ATTR joustAccelerometerHandler(accel_t* accel)
{

    joust.joustAccel.x = accel->x;
    joust.joustAccel.y = accel->y;
    joust.joustAccel.z = accel->z;
    int mov = (int) sqrt(pow(joust.joustAccel.x,2) + pow(joust.joustAccel.y,2)+ pow(joust.joustAccel.z,2));
    joust.rolling_average = (joust.rolling_average*2 + mov)/3;
    if (joust.gameState == R_PLAYING){
      if(mov > joust.rolling_average + 30){
        joustSendRoundLossMsg();
      }
      else{
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
            joust.led.connectionDim = 255;

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
            joust.led.connectionDim = 255;
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
    //     switch(ref.gam.difficulty)
    //     {
    //         case EASY:
    //         {
    //             // Turn on blue
    //             ref.led.Leds[i].b = ref.led.connectionDim;
    //             break;
    //         }
    //         case MEDIUM:
    //         {
    //             // Turn on green
    //             ref.led.Leds[i].g = ref.led.connectionDim;
    //             break;
    //         }
    //         case HARD:
    //         {
    //             // Turn on red
    //             ref.led.Leds[i].r = ref.led.connectionDim;
    //             break;
    //         }
    //         default:
    //         {
    //             break;
    //         }
    //     }
    }

    // Overwrite two LEDs based on the connection status
    // if(ref.cnc.rxGameStartAck)
    // {
    //     switch(ref.gam.difficulty)
    //     {
    //         case EASY:
    //         {
    //             // Green on blue
    //             ref.led.Leds[2].g = 25;
    //             ref.led.Leds[2].r = 0;
    //             ref.led.Leds[2].b = 0;
    //             break;
    //         }
    //         case MEDIUM:
    //         case HARD:
    //         {
    //             // Blue on green and red
    //             ref.led.Leds[2].g = 0;
    //             ref.led.Leds[2].r = 0;
    //             ref.led.Leds[2].b = 25;
    //             break;
    //         }
    //         default:
    //         {
    //             break;
    //         }
    //     }
    // }
    // if(ref.cnc.rxGameStartMsg)
    // {
    //     switch(ref.gam.difficulty)
    //     {
    //         case EASY:
    //         {
    //             // Green on blue
    //             ref.led.Leds[4].g = 25;
    //             ref.led.Leds[4].r = 0;
    //             ref.led.Leds[4].b = 0;
    //             break;
    //         }
    //         case MEDIUM:
    //         case HARD:
    //         {
    //             // Blue on green and red
    //             ref.led.Leds[4].g = 0;
    //             ref.led.Leds[4].r = 0;
    //             ref.led.Leds[4].b = 25;
    //             break;
    //         }
    //         default:
    //         {
    //             break;
    //         }
    //     }
    // }

    // Physically set the LEDs
    setLeds(joust.led.Leds, sizeof(joust.led.Leds));
}

// /**
//  * Called every 100ms, this updates the LEDs during the game
//  */
// void ICACHE_FLASH_ATTR refGameLedTimeout(void* arg __attribute__((unused)))
// {
//     // Decay all LEDs
//     uint8_t i;
//     for(i = 0; i < 6; i++)
//     {
//         if(ref.led.Leds[i].r > 0)
//         {
//             ref.led.Leds[i].r -= 4;
//         }
//         ref.led.Leds[i].g = 0;
//         ref.led.Leds[i].b = ref.led.Leds[i].r / 4;
//
//     }
//
//     // Sed LEDs according to the mode
//     if (ref.gam.shouldTurnOnLeds && ref.led.Degree % DEG_PER_LED == 0)
//     {
//         switch(ref.gam.Action)
//         {
//             case ACT_BOTH:
//             {
//                 // Make sure this value decays to exactly zero above
//                 ref.led.Leds[ref.led.Degree / DEG_PER_LED].r = 252;
//                 ref.led.Leds[ref.led.Degree / DEG_PER_LED].g = 0;
//                 ref.led.Leds[ref.led.Degree / DEG_PER_LED].b = 252 / 4;
//
//                 ref.led.Leds[(360 - ref.led.Degree) / DEG_PER_LED].r = 252;
//                 ref.led.Leds[(360 - ref.led.Degree) / DEG_PER_LED].g = 0;
//                 ref.led.Leds[(360 - ref.led.Degree) / DEG_PER_LED].b = 252 / 4;
//                 break;
//             }
//             case ACT_COUNTERCLOCKWISE:
//             case ACT_CLOCKWISE:
//             {
//                 ref.led.Leds[ref.led.Degree / DEG_PER_LED].r = 252;
//                 ref.led.Leds[ref.led.Degree / DEG_PER_LED].g = 0;
//                 ref.led.Leds[ref.led.Degree / DEG_PER_LED].b = 252 / 4;
//                 break;
//             }
//             default:
//             {
//                 break;
//             }
//         }
//
//         // Don't turn on LEDs past 180 degrees
//         if(180 == ref.led.Degree)
//         {
//             joust_printf("end of pattern\r\n");
//             ref.gam.shouldTurnOnLeds = false;
//         }
//     }
//
//     // Move the exciter according to the mode
//     switch(ref.gam.Action)
//     {
//         case ACT_BOTH:
//         case ACT_CLOCKWISE:
//         {
//             ref.led.Degree += 2;
//             if(ref.led.Degree > 359)
//             {
//                 ref.led.Degree -= 360;
//             }
//
//             break;
//         }
//         case ACT_COUNTERCLOCKWISE:
//         {
//             ref.led.Degree -= 2;
//             if(ref.led.Degree < 0)
//             {
//                 ref.led.Degree += 360;
//             }
//
//             break;
//         }
//         default:
//         {
//             break;
//         }
//     }
//
//     // Physically set the LEDs
//     setLeds(ref.led.Leds, sizeof(ref.led.Leds));
//
//     led_t blankLeds[6] = {{0}};
//     if(false == ref.gam.shouldTurnOnLeds &&
//             0 == ets_memcmp(ref.led.Leds, &blankLeds[0], sizeof(blankLeds)))
//     {
//         // If the last LED is off, the user missed the window of opportunity
//         refSendRoundLossMsg();
//     }
// }
//
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
void ICACHE_FLASH_ATTR joustButton( uint8_t state,
        int button __attribute__((unused)), int down __attribute__((unused)))
{
    if(!down)
    {
        // Ignore all button releases
        return;
    }

    if(joust.gameState ==  R_MENU){
      if(1 == button || 2 == button){
        joust.gameState =  R_SEARCHING;
        p2pStartConnection(&joust.p2pJoust);
        joust.p2pJoust.connectionRssi = 10;
        clearDisplay();
        plotText(0, 0, "Searching", IBM_VGA_8);
      }
    }

    // if(true == ref.gam.singlePlayer &&
    //         NOT_DISPLAYING != ref.led.singlePlayerDisplayState)
    // {
    //     // Single player score display, ignore button input
    //     return;
    // }
    //
    // // If we're still connecting and no connection has started yet
    // if(R_CONNECTING == ref.gameState && !ref.cnc.rxGameStartAck && !ref.cnc.rxGameStartMsg)
    // {
    //     if(1 == button)
    //     {
    //         // Start single player mode
    //         ref.gam.singlePlayer = true;
    //         ref.cnc.playOrder = GOING_FIRST;
    //         joustStartPlaying(NULL);
    //     }
    //     else if(2 == button)
    //     {
    //         // Adjust difficulty
    //         ref.gam.difficulty = (ref.gam.difficulty + 1) % 3;
    //     }
    // }
    // // If we're playing the game
    // else if(R_PLAYING == ref.gameState && true == down)
    // {
    //     bool success = false;
    //     bool failed = false;
    //
    //     // And the final LED is lit
    //     if(ref.led.Leds[3].r > 0)
    //     {
    //         // If it's the right button for a single button mode
    //         if ((ACT_COUNTERCLOCKWISE == ref.gam.Action && 2 == button) ||
    //                 (ACT_CLOCKWISE == ref.gam.Action && 1 == button))
    //         {
    //             success = true;
    //         }
    //         // If it's the wrong button for a single button mode
    //         else if ((ACT_COUNTERCLOCKWISE == ref.gam.Action && 1 == button) ||
    //                  (ACT_CLOCKWISE == ref.gam.Action && 2 == button))
    //         {
    //             failed = true;
    //         }
    //         // Or both buttons for both
    //         else if(ACT_BOTH == ref.gam.Action && ((0b110 & state) == 0b110))
    //         {
    //             success = true;
    //         }
    //     }
    //     else
    //     {
    //         // If the final LED isn't lit, it's always a failure
    //         failed = true;
    //     }
    //
    //     if(success)
    //     {
    //         joust_printf("Won the round, continue the game\r\n");
    //
    //         char* spdPtr;
    //         // Add information about the timing
    //         if(ref.led.Leds[3].r >= 192)
    //         {
    //             // Speed up if the button is pressed when the LED is brightest
    //             spdPtr = spdUp;
    //         }
    //         else if(ref.led.Leds[3].r >= 64)
    //         {
    //             // No change for the middle range
    //             spdPtr = spdNc;
    //         }
    //         else
    //         {
    //             // Slow down if button is pressed when the LED is dimmest
    //             spdPtr = spdDn;
    //         }
    //
    //         // Single player follows different speed up/down logic
    //         if(ref.gam.singlePlayer)
    //         {
    //             ref.gam.singlePlayerRounds++;
    //             if(spdPtr == spdUp)
    //             {
    //                 // If the hit is in the first 25%, increment the speed every third hit
    //                 // adjust speed up after three consecutive hits
    //                 ref.gam.singlePlayerTopHits++;
    //                 if(3 == ref.gam.singlePlayerTopHits)
    //                 {
    //                     refAdjustledSpeed(false, true);
    //                     ref.gam.singlePlayerTopHits = 0;
    //                 }
    //             }
    //             if(spdPtr == spdNc || spdPtr == spdDn)
    //             {
    //                 // If the hit is in the second 75%, increment the speed immediately
    //                 refAdjustledSpeed(false, true);
    //                 // Reset the top hit count too because it just sped up
    //                 ref.gam.singlePlayerTopHits = 0;
    //             }
    //             refStartRound();
    //         }
    //         else
    //         {
    //             // Now waiting for a result from the other swadge
    //             ref.gameState = R_WAITING;
    //
    //             // Clear the LEDs and stop the timer
    //             refDisarmAllLedTimers();
    //             ets_memset(ref.led.Leds, 0, sizeof(ref.led.Leds));
    //             setLeds(ref.led.Leds, sizeof(ref.led.Leds));
    //
    //             // Send a message to the other swadge that this round was a success
    //             ets_sprintf(&roundContinueMsg[MAC_IDX], macFmtStr,
    //                         ref.cnc.otherMac[0],
    //                         ref.cnc.otherMac[1],
    //                         ref.cnc.otherMac[2],
    //                         ref.cnc.otherMac[3],
    //                         ref.cnc.otherMac[4],
    //                         ref.cnc.otherMac[5]);
    //             roundContinueMsg[EXT_IDX - 1] = '_';
    //             ets_sprintf(&roundContinueMsg[EXT_IDX], "%s", spdPtr);
    //
    //             // If it's acked, start a timer to reinit if a result is never received
    //             // If it's not acked, reinit with refRestart()
    //             refSendMsg(roundContinueMsg, ets_strlen(roundContinueMsg), true, refStartRestartTimer, refRestart);
    //         }
    //     }
    //     else if(failed)
    //     {
    //         // Tell the other swadge
    //         refSendRoundLossMsg();
    //     }
    //     else
    //     {
    //         joust_printf("Neither won nor lost the round\r\n");
    //     }
    // }
}


//
// /**
//  * This starts a timer to reinit everything, used in case of a failure
//  *
//  * @param arg unused
//  */
// void ICACHE_FLASH_ATTR refStartRestartTimer(void* arg __attribute__((unused)))
// {
//     // Give 5 seconds to get a result, or else restart
//     os_timer_arm(&ref.tmr.Reinit, FAILURE_RESTART_MS, false);
// }
//
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

    // ref.gam.Losses++;

    // Show the current wins & losses
    joustRoundResultLed(false);
    // Send a message to that ESP that we lost the round
    // If it's acked, start a timer to reinit if another message is never received
    // If it's not acked, reinit with refRestart()
    p2pSendMsg(&joust.p2pJoust, "los", NULL, 0, joustMsgTxCbFn);


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


//
// /**
//  * This animation displays the single player score by first drawing the tens
//  * digit around the hexagon, blinking it, then drawing the ones digit around
//  * the hexagon
//  *
//  * Once the animation is finished, the next game is started
//  *
//  * @param arg unused
//  */
// void ICACHE_FLASH_ATTR refSinglePlayerRestart(void* arg __attribute__((unused)))
// {
//     switch(ref.led.singlePlayerDisplayState)
//     {
//         case NOT_DISPLAYING:
//         {
//             // Not supposed to be here
//             break;
//         }
//         case FAIL_DISPLAY_ON_1:
//         {
//             ets_memset(ref.led.Leds, 0, sizeof(ref.led.Leds));
//             uint8_t i;
//             for(i = 0; i < 6; i++)
//             {
//                 ref.led.Leds[i].r = 0xFF;
//             }
//
//             ref.led.singlePlayerDisplayState = FAIL_DISPLAY_OFF_2;
//             break;
//         }
//         case FAIL_DISPLAY_OFF_2:
//         {
//             ets_memset(ref.led.Leds, 0, sizeof(ref.led.Leds));
//             ref.led.singlePlayerDisplayState = FAIL_DISPLAY_ON_3;
//             break;
//         }
//         case FAIL_DISPLAY_ON_3:
//         {
//             uint8_t i;
//             for(i = 0; i < 6; i++)
//             {
//                 ref.led.Leds[i].r = 0xFF;
//             }
//             ref.led.singlePlayerDisplayState = FAIL_DISPLAY_OFF_4;
//             break;
//         }
//         case FAIL_DISPLAY_OFF_4:
//         {
//             ets_memset(ref.led.Leds, 0, sizeof(ref.led.Leds));
//             ref.led.singlePlayerDisplayState = SCORE_DISPLAY_INIT;
//             break;
//         }
//         case SCORE_DISPLAY_INIT:
//         {
//             // Clear the LEDs
//             ets_memset(ref.led.Leds, 0, sizeof(ref.led.Leds));
//             ref.led.ledsLit = 0;
//
//             if(ref.gam.singlePlayerRounds > 9)
//             {
//                 // For two digit numbers, start with the tens digit
//                 ref.led.singlePlayerDisplayState = FIRST_DIGIT_INC;
//                 ref.led.digitToDisplay = ref.gam.singlePlayerRounds / 10;
//             }
//             else
//             {
//                 // Otherwise just go to the ones digit
//                 ref.led.singlePlayerDisplayState = SECOND_DIGIT_INC;
//                 ref.led.digitToDisplay = ref.gam.singlePlayerRounds % 10;
//             }
//             break;
//         }
//         case FIRST_DIGIT_INC:
//         {
//             // Light each LED one at a time
//             if(ref.led.ledsLit < ref.led.digitToDisplay)
//             {
//                 // Light the LED
//                 refSinglePlayerScoreLed(ref.led.ledsLit, &digitCountFirstPrimary, &digitCountFirstSecondary);
//
//                 // keep track of how many LEDs are lit
//                 ref.led.ledsLit++;
//             }
//             else
//             {
//                 // All LEDs are lit, blink this number
//                 ref.led.singlePlayerDisplayState = FIRST_DIGIT_OFF;
//             }
//             break;
//         }
//         case FIRST_DIGIT_OFF:
//         {
//             // Turn everything off
//             ets_memset(ref.led.Leds, 0, sizeof(ref.led.Leds));
//             ref.led.ledsLit = 0;
//
//             // Then set it up to turn on
//             ref.led.singlePlayerDisplayState = FIRST_DIGIT_ON;
//             break;
//         }
//         case FIRST_DIGIT_ON:
//         {
//             // Reset the timer to show the final number a little longer
//             os_timer_disarm(&ref.tmr.SinglePlayerRestart);
//             os_timer_arm(&ref.tmr.SinglePlayerRestart, RESTART_COUNT_BLINK_PERIOD_MS, false);
//
//             // Light the full number all at once
//             while(ref.led.ledsLit < ref.led.digitToDisplay)
//             {
//                 refSinglePlayerScoreLed(ref.led.ledsLit, &digitCountFirstPrimary, &digitCountFirstSecondary);
//                 // keep track of how many LEDs are lit
//                 ref.led.ledsLit++;
//             }
//             // Then turn everything off again
//             ref.led.singlePlayerDisplayState = FIRST_DIGIT_OFF_2;
//             break;
//         }
//         case FIRST_DIGIT_OFF_2:
//         {
//             // Reset the timer to normal speed
//             os_timer_disarm(&ref.tmr.SinglePlayerRestart);
//             os_timer_arm(&ref.tmr.SinglePlayerRestart, RESTART_COUNT_PERIOD_MS, true);
//
//             // turn all LEDs off
//             ets_memset(ref.led.Leds, 0, sizeof(ref.led.Leds));
//             ref.led.ledsLit = 0;
//
//             ref.led.digitToDisplay = ref.gam.singlePlayerRounds % 10;
//             ref.led.singlePlayerDisplayState = SECOND_DIGIT_INC;
//
//             break;
//         }
//         case SECOND_DIGIT_INC:
//         {
//             // Light each LED one at a time
//             if(ref.led.ledsLit < ref.led.digitToDisplay)
//             {
//                 // Light the LED
//                 refSinglePlayerScoreLed(ref.led.ledsLit, &digitCountSecondPrimary, &digitCountSecondSecondary);
//
//                 // keep track of how many LEDs are lit
//                 ref.led.ledsLit++;
//             }
//             else
//             {
//                 // All LEDs are lit, blink this number
//                 ref.led.singlePlayerDisplayState = SECOND_DIGIT_OFF;
//             }
//             break;
//         }
//         case SECOND_DIGIT_OFF:
//         {
//             // Turn everything off
//             ets_memset(ref.led.Leds, 0, sizeof(ref.led.Leds));
//             ref.led.ledsLit = 0;
//
//             // Then set it up to turn on
//             ref.led.singlePlayerDisplayState = SECOND_DIGIT_ON;
//             break;
//         }
//         case SECOND_DIGIT_ON:
//         {
//             // Reset the timer to show the final number a little longer
//             os_timer_disarm(&ref.tmr.SinglePlayerRestart);
//             os_timer_arm(&ref.tmr.SinglePlayerRestart, RESTART_COUNT_BLINK_PERIOD_MS, false);
//
//             // Light the full number all at once
//             while(ref.led.ledsLit < ref.led.digitToDisplay)
//             {
//                 refSinglePlayerScoreLed(ref.led.ledsLit, &digitCountSecondPrimary, &digitCountSecondSecondary);
//                 // keep track of how many LEDs are lit
//                 ref.led.ledsLit++;
//             }
//             // Then turn everything off again
//             ref.led.singlePlayerDisplayState = SECOND_DIGIT_OFF_2;
//             break;
//         }
//         case SECOND_DIGIT_OFF_2:
//         {
//             // Reset the timer to normal speed
//             os_timer_disarm(&ref.tmr.SinglePlayerRestart);
//             os_timer_arm(&ref.tmr.SinglePlayerRestart, RESTART_COUNT_PERIOD_MS, true);
//
//             // turn all LEDs off
//             ets_memset(ref.led.Leds, 0, sizeof(ref.led.Leds));
//             ref.led.ledsLit = 0;
//
//             ref.led.singlePlayerDisplayState = SCORE_DISPLAY_FINISH;
//
//             break;
//         }
//         case SCORE_DISPLAY_FINISH:
//         {
//             // Disarm the timer
//             os_timer_disarm(&ref.tmr.SinglePlayerRestart);
//
//             // For next time
//             ref.led.singlePlayerDisplayState = NOT_DISPLAYING;
//
//             // Reset and start another round
//             ref.gam.singlePlayerRounds = 0;
//             ref.gam.singlePlayerTopHits = 0;
//             refAdjustledSpeed(true, true);
//             refStartRound();
//             break;
//         }
//         default:
//         {
//             break;
//         }
//     }
//
//     setLeds(ref.led.Leds, sizeof(ref.led.Leds));
// }
//

/**
 * Show the wins and losses
 *
 * @param roundWinner true if this swadge was a winner, false if the other
 *                    swadge won
 */
void ICACHE_FLASH_ATTR joustRoundResultLed(bool roundWinner)
{
    joustRestart(NULL);
    // sint8_t i;
    //
    // // Clear the LEDs
    // ets_memset(ref.led.Leds, 0, sizeof(ref.led.Leds));
    //
    // // Light green for wins
    // for(i = 4; i < 4 + ref.gam.Wins; i++)
    // {
    //     // Green
    //     ref.led.Leds[i % 6].g = 255;
    //     ref.led.Leds[i % 6].r = 0;
    //     ref.led.Leds[i % 6].b = 0;
    // }
    //
    // // Light reds for losses
    // for(i = 2; i >= (3 - ref.gam.Losses); i--)
    // {
    //     // Red
    //     ref.led.Leds[i].g = 0;
    //     ref.led.Leds[i].r = 255;
    //     ref.led.Leds[i].b = 0;
    // }
    //
    // // Push out LED data
    // refDisarmAllLedTimers();
    // setLeds(ref.led.Leds, sizeof(ref.led.Leds));
    //
    // // Set up the next round based on the winner
    // if(roundWinner)
    // {
    //     ref.gameState = R_SHOW_GAME_RESULT;
    //     ref.cnc.playOrder = GOING_FIRST;
    // }
    // else
    // {
    //     // Set ref.gameState here to R_WAITING to make sure a message isn't missed
    //     ref.gameState = R_WAITING;
    //     ref.cnc.playOrder = GOING_SECOND;
    //     ref.gam.receiveFirstMsg = false;
    // }
    //
    // // Call joustStartPlaying in 3 seconds
    // os_timer_arm(&ref.tmr.StartPlaying, 3000, false);
}

// /**
//  * Adjust the speed of the game, or reset it to the default value for this
//  * difficulty
//  *
//  * @param reset true to reset to the starting value, up will be ignored
//  * @param up    if reset is false, if this is true, speed up, otherwise slow down
//  */
// void ICACHE_FLASH_ATTR refAdjustledSpeed(bool reset, bool up)
// {
//     // If you're in single player, ignore any speed downs
//     if(ref.gam.singlePlayer && up == false)
//     {
//         return;
//     }
//
//     if(reset)
//     {
//         switch(ref.gam.difficulty)
//         {
//             case EASY:
//             {
//                 ref.gam.ledPeriodMs = LED_TIMER_MS_STARTING_EASY;
//                 break;
//             }
//             case MEDIUM:
//             {
//                 ref.gam.ledPeriodMs = LED_TIMER_MS_STARTING_MEDIUM;
//                 break;
//             }
//             case HARD:
//             {
//                 ref.gam.ledPeriodMs = LED_TIMER_MS_STARTING_HARD;
//                 break;
//             }
//             default:
//             {
//                 break;
//             }
//         }
//     }
//     else if (GOING_SECOND == ref.cnc.playOrder && false == ref.gam.receiveFirstMsg)
//     {
//         // If going second, ignore the first up/dn from the first player
//         ref.gam.receiveFirstMsg = true;
//     }
//     else if(up)
//     {
//         switch(ref.gam.difficulty)
//         {
//             case EASY:
//             case MEDIUM:
//             {
//                 ref.gam.ledPeriodMs--;
//                 break;
//             }
//             case HARD:
//             {
//                 ref.gam.ledPeriodMs -= 2;
//                 break;
//             }
//             default:
//             {
//                 break;
//             }
//         }
//         // Anything less than a 3ms period is impossible...
//         if(ref.gam.ledPeriodMs < 3)
//         {
//             ref.gam.ledPeriodMs = 3;
//         }
//     }
//     else
//     {
//         switch(ref.gam.difficulty)
//         {
//             case EASY:
//             case MEDIUM:
//             {
//                 ref.gam.ledPeriodMs++;
//                 break;
//             }
//             case HARD:
//             {
//                 ref.gam.ledPeriodMs += 2;
//                 break;
//             }
//             default:
//             {
//                 break;
//             }
//         }
//     }
// }
