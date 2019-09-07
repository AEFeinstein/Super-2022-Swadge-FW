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


    os_timer_disarm(&joust.tmr.RestartJoust);
    os_timer_setfn(&joust.tmr.RestartJoust, joustRestart, NULL);


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


    // Check for match end
    // joust_printf("wins: %d, losses %d\r\n", ref.gam.Wins, ref.gam.Losses);
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
    joustRoundResultLed(false);



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



/**
 * Show the wins and losses
 *
 * @param roundWinner true if this swadge was a winner, false if the other
 *                    swadge won
 */
void ICACHE_FLASH_ATTR joustRoundResultLed(bool roundWinner)
{
    uint8_t currBrightness = joust.led.Leds[0].r;
    currBrightness = 0xFF;
    // joust.led.ConnLedState = LED_CONNECTED_DIM;
    joust.led.connectionDim = 255;
    ets_memset(joust.led.Leds, currBrightness, sizeof(joust.led.Leds));
    setLeds(joust.led.Leds, sizeof(joust.led.Leds));
    joust.gameState = R_SHOW_GAME_RESULT;
    if(roundWinner){
      clearDisplay();
      plotText(0, 0, "Winner", IBM_VGA_8);
    }else{
      clearDisplay();
      plotText(0, 0, "Loser", IBM_VGA_8);
    }
    os_timer_arm(&joust.tmr.RestartJoust, 8000, false);


}
