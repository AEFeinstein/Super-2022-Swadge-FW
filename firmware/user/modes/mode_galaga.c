/*
 * mode_galaga.c
 *
 *  Created on: Aug 11, 2020
 *      Author: Jonathan Moriarty
 */

/*============================================================================
 * Includes
 *==========================================================================*/

#include "user_config.h"
#include <osapi.h>
#include <mem.h>
#include <stdint.h>
#include <user_interface.h>

#include "user_main.h"
#include "embeddednf.h"
#include "oled.h"
#include "bresenham.h"
#include "assets.h"
#include "buttons.h"
#include "menu2d.h"
#include "linked_list.h"
#include "font.h"

#include "embeddednf.h"
#include "embeddedout.h"

//NOTES:
/*
Need to go through includes and remove unnecessary, list what each one is for.
Start with the basic mechanics of galaga, add a reflector shield with a CD, and more varied enemy attack patterns, etc.

This is a Swadge Mode which has states, the mode updates in different ways depending on the current state.
An Update consists of detecting and handline INPUT -> running any game LOGIC that is unrelated to input -> DISPLAY to the user the current mode state.

TODO:
player movement/render
enemy movement/render
player/enemy shooting
player reflect/dodge feature
win/loss conditions
scores
polish

*/

/*============================================================================
 * Defines, Structs, Enums
 *==========================================================================*/

// update task info.
#define UPDATE_TIME_MS 16
#define DISPLAY_REFRESH_MS 400 // This is a best guess for syncing LED FX with OLED FX.

// time info.
#define MS_TO_US_FACTOR 1000
#define S_TO_MS_FACTOR 1000
#define US_TO_MS_FACTOR 0.001
#define MS_TO_S_FACTOR 0.001

// useful display.
#define OLED_HALF_HEIGHT 32 // (OLED_HEIGHT / 2)

// LED FX
#define NUM_LEDS NUM_LIN_LEDS // This pulls from user_config that should be the right amount for the current swadge.
#define MODE_LED_BRIGHTNESS 0.125 // Factor that decreases overall brightness of LEDs since they are a little distracting at full brightness.

typedef enum
{
    GA_TITLE,
    GA_GAME,
    GA_SCORES,
    GA_GAMEOVER
} galagaState_t;

typedef struct
{
    galagaState_t state;

    uint32_t modeStartTime; // time mode started in microseconds.
    uint32_t stateStartTime; // time the most recent state started in microseconds.
    uint32_t deltaTime; // time elapsed since last update in microseconds.
    uint32_t modeTime;  // total time the mode has been running in microseconds.
    uint32_t stateTime; // total time the state has been running in microseconds.
    uint32_t modeFrames; // total number of frames elapsed in this mode.
    uint32_t stateFrames; // total number of frames elapsed in this state.

    uint8_t buttonState;
    uint8_t lastButtonState;

    accel_t accel;
    accel_t lastAccel;

    timer_t updateTimer;

    menu_t* menu;

    //TODO: every game related variable should be contained within this.
} galaga_t;

/*============================================================================
 * Prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR gaEnterMode(void);
void ICACHE_FLASH_ATTR gaExitMode(void);
void ICACHE_FLASH_ATTR gaButtonCallback(uint8_t state __attribute__((unused)), int button, int down);
void ICACHE_FLASH_ATTR gaAccelerometerCallback(accel_t* accel);
void ICACHE_FLASH_ATTR gaAudioCallback(int32_t samp);

static void ICACHE_FLASH_ATTR gaUpdate(void* arg __attribute__((unused)));

// handle inputs.
void ICACHE_FLASH_ATTR gaTitleInput(void);
void ICACHE_FLASH_ATTR gaGameInput(void);
void ICACHE_FLASH_ATTR gaScoresInput(void);
void ICACHE_FLASH_ATTR gaGameoverInput(void);

// update any input-unrelated logic.
void ICACHE_FLASH_ATTR gaTitleLogic(void);
void ICACHE_FLASH_ATTR gaGameLogic(void);
void ICACHE_FLASH_ATTR gaScoresLogic(void);
void ICACHE_FLASH_ATTR gaGameoverLogic(void);

// draw the frame.
void ICACHE_FLASH_ATTR gaTitleDisplay(void);
void ICACHE_FLASH_ATTR gaGameDisplay(void);
void ICACHE_FLASH_ATTR gaScoresDisplay(void);
void ICACHE_FLASH_ATTR gaGameoverDisplay(void);

// mode state management.
void ICACHE_FLASH_ATTR gaSetState(galagaState_t newState);

static void ICACHE_FLASH_ATTR gaMenuCallback(const char* menuItem);

// input checking.
bool ICACHE_FLASH_ATTR gaIsButtonPressed(uint8_t button);
bool ICACHE_FLASH_ATTR gaIsButtonReleased(uint8_t button);
bool ICACHE_FLASH_ATTR gaIsButtonDown(uint8_t button);
bool ICACHE_FLASH_ATTR gaIsButtonUp(uint8_t button);

//TODO: drawing functions.

//TODO: score operations.
/*void ICACHE_FLASH_ATTR loadHighScores(void);
void ICACHE_FLASH_ATTR saveHighScores(void);
bool ICACHE_FLASH_ATTR updateHighScores(uint32_t newScore);*/

//TODO: LED FX functions.

/*============================================================================
 * Variables
 *==========================================================================*/

swadgeMode galagaMode =
{
    .modeName = "galaga",
    .fnEnterMode = gaEnterMode,
    .fnExitMode = gaExitMode,
    .fnButtonCallback = gaButtonCallback,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = gaAccelerometerCallback,
    .fnAudioCallback = gaAudioCallback,
    .menuImg = "copter-menu.gif" //TODO: need a menu image to link to here.
};

galaga_t* galaga;

static const char ga_title[]  = "GALAGA";
static const char ga_start[]  = "START";
static const char ga_scores[] = "HIGH SCORES";
static const char ga_quit[]   = "QUIT";

/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * Initializer for galaga
 */
void ICACHE_FLASH_ATTR gaEnterMode(void)
{
    // Give us responsive input.
    enableDebounce(false);

    // Alloc and clear everything.
    galaga = os_malloc(sizeof(galaga_t));
    ets_memset(galaga, 0, sizeof(galaga_t));

    // Reset mode time tracking.
    galaga->modeStartTime = system_get_time();
    galaga->modeTime = 0;
    galaga->modeFrames = 0;

    // Reset input tracking.
    galaga->buttonState = 0;
    galaga->lastButtonState = 0;

    // TODO: is this the correct way to initialize this?
    ets_memset(&(galaga->accel), 0, sizeof(accel_t));
    ets_memset(&(galaga->lastAccel), 0, sizeof(accel_t));

    // Reset mode state.
    galaga->state = GA_TITLE;
    gaSetState(GA_TITLE);

    // Init menu system.
    galaga->menu = initMenu(ga_title, gaMenuCallback); //TODO: input handler for menu?
    addRowToMenu(galaga->menu);
    addItemToRow(galaga->menu, ga_start);
    addRowToMenu(galaga->menu);
    addItemToRow(galaga->menu, ga_scores);
    addRowToMenu(galaga->menu);
    addItemToRow(galaga->menu, ga_quit);
    drawMenu(galaga->menu);

    // Start the update loop.
    timerDisarm(&(galaga->updateTimer));
    timerSetFn(&(galaga->updateTimer), gaUpdate, NULL);
    timerArm(&(galaga->updateTimer), UPDATE_TIME_MS, true);

    //InitColorChord(); //TODO: Initialize preferred swadge LED behavior.
}

/**
 * Called when galaga is exited
 */
void ICACHE_FLASH_ATTR gaExitMode(void)
{
    timerDisarm(&(galaga->updateTimer));
    timerFlush();
    deinitMenu(galaga->menu);
    //clear(&galaga->obstacles);
    // TODO: free and clear everything.
    os_free(galaga);
}

/**
 * TODO
 *
 * @param state  A bitmask of all button states, unused
 * @param button The button which triggered this event
 * @param down   true if the button was pressed, false if it was released
 */
void ICACHE_FLASH_ATTR gaButtonCallback( uint8_t state, int button __attribute__((unused)), int down __attribute__((unused)))
{
    galaga->buttonState = state; // Set the state of all buttons
    if (down) menuButton(galaga->menu, button);
}

void ICACHE_FLASH_ATTR gaAccelerometerCallback(accel_t* accel)
{
    galaga->accel.x = accel->x;   // Set the accelerometer values
    galaga->accel.y = accel->y;
    galaga->accel.z = accel->z;
}

/**
 * This is called every time an audio sample is read from the ADC
 * This processes the sample and will display update the LEDs every
 * 128 samples
 *
 * @param samp A 32 bit audio sample read from the ADC (microphone)
 */
void ICACHE_FLASH_ATTR gaAudioCallback(int32_t samp)
{
    //TODO: should I even do anything with this?
}

/**
 * @brief called on a timer, updates the game state
 *
 * @param arg
 */
static void ICACHE_FLASH_ATTR gaUpdate(void* arg __attribute__((unused)))
{
    // Update time tracking.
    // NOTE: delta time is in microseconds.
    // UPDATE time is in milliseconds.

    uint32_t newModeTime = system_get_time() - galaga->modeStartTime;
    uint32_t newStateTime = system_get_time() - galaga->stateStartTime;
    galaga->deltaTime = newModeTime - galaga->modeTime;
    galaga->modeTime = newModeTime;
    galaga->stateTime = newStateTime;
    galaga->modeFrames++;
    galaga->stateFrames++;

    // Handle Input
    switch( galaga->state )
    {
        default:
        case GA_TITLE:
        {
            gaTitleInput();
            break;
        }
        case GA_GAME:
        {
            gaGameInput();
            break;
        }
        case GA_SCORES:
        {
            gaScoresInput();
            break;
        }
        case GA_GAMEOVER:
        {
            gaGameoverInput();
            break;
        }
    };

    // Mark what our inputs were the last time we acted on them.
    galaga->lastButtonState = galaga->buttonState;
    galaga->lastAccel = galaga->accel;

    // Handle State Logic
    switch( galaga->state )
    {
        default:
        case GA_TITLE:
        {
            gaTitleLogic();
            break;
        }
        case GA_GAME:
        {
            gaGameLogic();
            break;
        }
        case GA_SCORES:
        {
            gaScoresLogic();
            break;
        }
        case GA_GAMEOVER:
        {
            gaGameoverLogic();
            break;
        }
    };

    // Handle Drawing Frame (based on the state)
    switch( galaga->state )
    {
        default:
        case GA_TITLE:
        {
            gaTitleDisplay();
            break;
        }
        case GA_GAME:
        {
            gaGameDisplay();
            break;
        }
        case GA_SCORES:
        {
            gaScoresDisplay();
            break;
        }
        case GA_GAMEOVER:
        {
            gaGameoverDisplay();
            break;
        }
    };

    // Draw debug FPS counter.
    /*double seconds = ((double)stateTime * (double)US_TO_MS_FACTOR * (double)MS_TO_S_FACTOR);
    int32_t fps = (int)((double)stateFrames / seconds);
    ets_snprintf(uiStr, sizeof(uiStr), "FPS: %d", fps);
    plotText(OLED_WIDTH - getTextWidth(uiStr, TOM_THUMB) - 1, OLED_HEIGHT - (1 * (FONT_HEIGHT_TOMTHUMB + 1)), uiStr, TOM_THUMB, WHITE);*/
}

// helper functions.

void ICACHE_FLASH_ATTR gaSetState(galagaState_t newState)
{
    galagaState_t prevState = galaga->state;
    galaga->state = newState;
    galaga->stateStartTime = system_get_time();
    galaga->stateTime = 0;
    galaga->stateFrames = 0;

    switch( galaga->state )
    {
        default:
        case GA_TITLE:
            break;
        case GA_GAME:
            break;
        case GA_SCORES:
            break;
        case GA_GAMEOVER:
            break;
    };
}

bool ICACHE_FLASH_ATTR gaIsButtonPressed(uint8_t button)
{
    return (galaga->buttonState & button) && !(galaga->lastButtonState & button);
}

bool ICACHE_FLASH_ATTR gaIsButtonReleased(uint8_t button)
{
    return !(galaga->buttonState & button) && (galaga->lastButtonState & button);
}

bool ICACHE_FLASH_ATTR gaIsButtonDown(uint8_t button)
{
    return galaga->buttonState & button;
}

bool ICACHE_FLASH_ATTR gaIsButtonUp(uint8_t button)
{
    return !(galaga->buttonState & button);
}

void ICACHE_FLASH_ATTR gaTitleInput(void)
{
    
}

void ICACHE_FLASH_ATTR gaTitleLogic(void)
{

}

void ICACHE_FLASH_ATTR gaTitleDisplay(void)
{
    drawMenu(galaga->menu);
}

void ICACHE_FLASH_ATTR gaGameInput(void)
{

}

void ICACHE_FLASH_ATTR gaGameLogic(void)
{

}

void ICACHE_FLASH_ATTR gaGameDisplay(void)
{

}

void ICACHE_FLASH_ATTR gaScoresInput(void)
{

}

void ICACHE_FLASH_ATTR gaScoresLogic(void)
{

}

void ICACHE_FLASH_ATTR gaScoresDisplay(void)
{

}

void ICACHE_FLASH_ATTR gaGameoverInput(void)
{

}

void ICACHE_FLASH_ATTR gaGameoverLogic(void)
{

}

void ICACHE_FLASH_ATTR gaGameoverDisplay(void)
{

}

/**
 * TODO
 *
 * @param menuItem
 */
static void ICACHE_FLASH_ATTR gaMenuCallback(const char* menuItem)
{
    if (ga_start == menuItem)
    {
        // Change state to start game.
        gaSetState(GA_GAME);
    }
    else if (ga_scores == menuItem)
    {
        // Change state to score screen.
        gaSetState(GA_SCORES);
    }
    else if (ga_quit == menuItem)
    {
        // Exit this swadge mode.
        switchToSwadgeMode(0);
    }
}