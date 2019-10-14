/*
*	mode_color_movement.c
*
*	Created on: 10 Oct 2019
*               Author: bbkiw
*/

#include <osapi.h>
#include <user_interface.h>
#include <stdlib.h>
#include "maxtime.h"
#include "user_main.h"	//swadge mode
#include "mode_color_movement.h"
#include "mode_dance.h"
#include "ccconfig.h"
#include "DFT32.h"
#include "buttons.h"
#include "oled.h"		//display functions
#include "font.h"		//draw text
#include "bresenham.h"	//draw shapes
#include "hpatimer.h"   //for sound
#include "linked_list.h" //custom linked list
#include "custom_commands.h" //saving and loading high scores and last scores
#include "math.h"
#include "embeddedout.h"

/*============================================================================
 * Defines
 *==========================================================================*/

//#define CM_DEBUG_PRINT
#ifdef CM_DEBUG_PRINT
#include <stdlib.h>
    #define CM_printf(...) os_printf(__VA_ARGS__)
#else
    #define CM_printf(...)
#endif

//#ifndef max
//    #define max(a,b) ((a) > (b) ? (a) : (b))
//#endif
// #ifndef min
//     #define min(a,b) ((a) < (b) ? (a) : (b))
// #endif

// controls (title)
#define BTN_TITLE_START_SCORES LEFT
#define BTN_TITLE_START_GAME RIGHT

// controls (game)
#define BTN_GAME_RIGHT RIGHT
#define BTN_GAME_LEFT LEFT

// controls (scores)
#define BTN_SCORES_CLEAR_SCORES LEFT
#define BTN_SCORES_START_TITLE RIGHT

// controls (gameover)
#define BTN_GAMEOVER_START_TITLE LEFT
#define BTN_GAMEOVER_START_GAME RIGHT

// update task (16 would give 60 fps like ipad, need read accel that fast too?)
#define UPDATE_TIME_MS 16

// time info.
#define MS_TO_US_FACTOR 1000
#define S_TO_MS_FACTOR 1000
//#define US_TO_MS_FACTOR 0.001

#define CLEAR_SCORES_HOLD_TIME (5 * MS_TO_US_FACTOR * S_TO_MS_FACTOR)

#define NUM_CM_HIGH_SCORES 3
#define NUM_DOTS 120
#define SOUND_ON true
#define ALPHA_FAST 0.3
#define ALPHA_SLOW 0.05
#define ALPHA_CROSS 0.1
#define ALPHA_ACTIVE 0.03
#define CROSS_TOL 0.06
#define SPECIAL_EFFECT true

#define BOX_LEVEL 0
#define PRACTICE_LEVEL 1
#define EASY_LEVEL 2
#define MIDDLE_LEVEL 3
#define HARD_LEVEL 4
#define KILLER_LEVEL 5
#define IMPOSSIBLE_LEVEL 6

// LEDs relation to screen
#define LED_UPPER_LEFT LED_1
#define LED_UPPER_MID LED_2
#define LED_UPPER_RIGHT LED_3
#define LED_LOWER_RIGHT LED_4
#define LED_LOWER_MID LED_5
#define LED_LOWER_LEFT LED_6

#define SCREEN_BORDER 0

// any enums go here.

typedef enum
{
    CM_TITLE,	// title screen
    CM_GAME,	// play the actual game
    CM_AUTO,	// automataically play the actual game
    CM_SCORES,	// high scores
    CM_GAMEOVER // game over
} cmState_t;

typedef enum
{
    UPPER_LEFT,
    LOWER_LEFT,
    LOWER_RIGHT,
    UPPER_RIGHT
} exitSpot_t;



// Title screen info.

// Score screen info.
uint32_t clearScoreTimer;
bool holdingClearScore;

// Game state info.
uint32_t score; // The current score this game.
uint32_t highScores[NUM_CM_HIGH_SCORES];
bool newHighScore;

// function prototypes go here.
/*============================================================================
 * Prototypes
 *==========================================================================*/
// Led patterns borrowed from mode_dance.c
void ICACHE_FLASH_ATTR danceTimerMode1(void);
void ICACHE_FLASH_ATTR danceTimerMode2(void);
void ICACHE_FLASH_ATTR danceTimerMode3(void);
void ICACHE_FLASH_ATTR danceTimerMode4(void);
void ICACHE_FLASH_ATTR danceTimerMode6(void);
void ICACHE_FLASH_ATTR danceTimerMode13(void);
void ICACHE_FLASH_ATTR danceTimerMode16(void);
void ICACHE_FLASH_ATTR danceTimerMode17(void);

void ICACHE_FLASH_ATTR cmInit(void);
void ICACHE_FLASH_ATTR cmDeInit(void);
void ICACHE_FLASH_ATTR cmButtonCallback(uint8_t state, int button, int down);
void ICACHE_FLASH_ATTR cmAccelerometerCallback(accel_t* accel);

// Free memory
void ICACHE_FLASH_ATTR cmFreeMemory(void);

// game loop functions.
void ICACHE_FLASH_ATTR cmUpdate(void* arg);

// handle inputs.
void ICACHE_FLASH_ATTR cmTitleInput(void);
void ICACHE_FLASH_ATTR cmGameInput(void);
void ICACHE_FLASH_ATTR cmScoresInput(void);
void ICACHE_FLASH_ATTR cmGameoverInput(void);

// update any input-unrelated logic.
void ICACHE_FLASH_ATTR cmTitleUpdate(void);
void ICACHE_FLASH_ATTR cmGameUpdate(void);
void ICACHE_FLASH_ATTR cmAutoGameUpdate(void);
void ICACHE_FLASH_ATTR cmScoresUpdate(void);
void ICACHE_FLASH_ATTR cmGameoverUpdate(void);

// draw the frame.
void ICACHE_FLASH_ATTR cmTitleDisplay(void);
void ICACHE_FLASH_ATTR cmGameDisplay(void);
void ICACHE_FLASH_ATTR cmScoresDisplay(void);
void ICACHE_FLASH_ATTR cmGameoverDisplay(void);

// mode state management.
void ICACHE_FLASH_ATTR cmChangeState(cmState_t newState);

// input checking.
bool ICACHE_FLASH_ATTR cmIsButtonPressed(uint8_t button);
bool ICACHE_FLASH_ATTR cmIsButtonReleased(uint8_t button);
bool ICACHE_FLASH_ATTR cmIsButtonDown(uint8_t button);
bool ICACHE_FLASH_ATTR cmIsButtonUp(uint8_t button);

// drawing functions.
static void ICACHE_FLASH_ATTR plotCenteredText(uint8_t x0, uint8_t y, uint8_t x1, char* text, fonts font, color col);
static uint8_t getTextWidth(char* text, fonts font);


// score operations.
//static void loadHighScores(void);
//static void saveHighScores(void);
//static bool updateHighScores(uint32_t newScore);

// Additional Helper
void ICACHE_FLASH_ATTR setCMLeds(led_t* ledData, uint8_t ledDataLen);
void ICACHE_FLASH_ATTR cmChangeLevel(void);

void ICACHE_FLASH_ATTR cmNewSetup(void);

uint16_t ICACHE_FLASH_ATTR circularPush(int16_t value, uint16_t insertInd, int16_t buffer[]);
int16_t ICACHE_FLASH_ATTR IIRFilter(float alpha, int16_t input, int16_t output);
void ICACHE_FLASH_ATTR AdjustPlotDots(int16_t buffer1[], uint16_t insert1, int16_t buffer2[], uint16_t insert2);
void ICACHE_FLASH_ATTR AdjustPlotDotsSingle(int16_t buffer[], uint16_t insert);
/*============================================================================
 * Static Const Variables
 *==========================================================================*/

static const uint8_t cmBrightnesses[] =
{
    0x01,
    0x08,
    0x40,
    0x80,
};


static const char * levelName[] = {"BOX", "PRACTICE", "EASY", "MIDDLE", "HARD", "KILLER", "IMPOSSIBLE"};

/*============================================================================
 * Variables
 *==========================================================================*/


// game logic operations.

swadgeMode colorMoveMode =
{
	.modeName = "ColorShake",
	.fnEnterMode = cmInit,
	.fnExitMode = cmDeInit,
	.fnButtonCallback = cmButtonCallback,
	.wifiMode = NO_WIFI,
	.fnEspNowRecvCb = NULL,
	.fnEspNowSendCb = NULL,
	.fnAccelerometerCallback = cmAccelerometerCallback
};

accel_t cmAccel = {0};
accel_t cmLastAccel = {0};

accel_t cmLastTestAccel = {0};

uint8_t cmButtonState = 0;
uint8_t cmLastButtonState = 0;

uint8_t cmLevel = IMPOSSIBLE_LEVEL;
uint8_t cmBrightnessIdx = 0;
uint8_t ledOrderInd[] = {LED_UPPER_LEFT, LED_LOWER_LEFT, LED_LOWER_MID, LED_LOWER_RIGHT, LED_UPPER_RIGHT, LED_UPPER_MID};
static led_t leds[NUM_LIN_LEDS] = {{0}};
int CM_ledCount = 0;
static os_timer_t timerHandleUpdate = {0};

static uint32_t modeStartTime = 0; // time mode started in microseconds.
static uint32_t stateStartTime = 0; // time the most recent state started in microseconds.
static uint32_t deltaTime = 0;	// time elapsed since last update.
static uint32_t modeTime = 0;	// total time the mode has been running.
static uint32_t stateTime = 0;	// total time the game has been running.

static cmState_t currState = CM_TITLE;
static cmState_t prevState;


int16_t xAccel;
int16_t yAccel;
int16_t zAccel;

bool gameover;

//TODO wanted circular buffer to be a structure
//And implement handling of it by helper function
//Can't see how now, so will do by brute force in the mean time
// typedef struct
// {
//     uint16_t length;
//     uint16_t insertInd;
//     int16_t * data;
// } circularBuffer_t;


// circularBuffer_t  bufNormAccel;
// circularBuffer_t  bufHighPassNormAccel;
// circularBuffer_t  bufXaccel;
// circularBuffer_t  bufLowPassXaccel;
// circularBuffer_t  bufYaccel;
// circularBuffer_t  bufLowPassYaccel;
// circularBuffer_t  bufZaccel;
// circularBuffer_t  bufLowPassZaccel;

int16_t * bufNormAccel;
int16_t * bufHighPassNormAccel;
int16_t * bufXaccel;
int16_t * bufLowPassXaccel;
int16_t * bufYaccel;
int16_t * bufLowPassYaccel;
int16_t * bufZaccel;
int16_t * bufLowPassZaccel;

// Point (index) to insertion point
uint16_t  bufNormAccelInsert;
uint16_t  bufHighPassNormAccelInsert;
uint16_t  bufXaccelInsert;
uint16_t  bufLowPassXaccelInsert;
uint16_t  bufYaccelInsert;
uint16_t  bufLowPassYaccelInsert;
uint16_t  bufZaccelInsert;
uint16_t  bufLowPassZaccelInsert;

float REVOLUTIONS_PER_ZERO_CROSSING = 0.5;
uint8_t PLOT_SCALE = 32;
uint8_t PLOT_SHIFT = 32;
// do via timer now 10 fps FRAME_RESET = 0 // 0 for 60 fps, 1 for 30 fps, 2 for 20 fps, k for 60/(k+1) fps
uint8_t FRAME_RESET = 0;

int16_t showcount = 0; // used for skipping frames
int16_t adj = SCREEN_BORDER;
int16_t wid = 128 - 2 * SCREEN_BORDER;

uint8_t ledcycle = 0;

int16_t lowPassNormAccel = 0;
int16_t lowPassXaccel = 0;
int16_t lowPassYaccel = 0;
int16_t lowPassZaccel = 0;
int16_t smoothNormAccel = 0;
int16_t smoothXaccel = 0;
int16_t smoothYaccel = 0;
int16_t smoothZaccel = 0;
int16_t prevHighPassNormAccel = 0;
int16_t smoothActivity = 0;

// times float? or use cycles?
int16_t lastzerocrosst = 0;
int16_t ledPrevIncTime = 0;
int16_t aveZeroCrossInterval = 5;
int16_t crossinterval = 5;

bool pause = false;
bool skipNextCross = true;
bool still = true;

// Helpers
uint16_t ICACHE_FLASH_ATTR circularPush(int16_t value, uint16_t insertInd, int16_t * buffer)
{
    buffer[insertInd] = value;
    if (insertInd >= NUM_DOTS)
    {
        return 0;
    } else {
        return insertInd + 1;
    }
}

int16_t ICACHE_FLASH_ATTR IIRFilter(float alpha, int16_t  input, int16_t output)
{
    // return updated output and returns it
    return  (1 - alpha) * output + alpha * input;
}

void ICACHE_FLASH_ATTR AdjustPlotDots(int16_t buffer1[], uint16_t insert1, int16_t buffer2[], uint16_t insert2)
{
    // Plots a graph with x from 0 to 119 and y from buffer1 - buffer2
    uint8_t i;
    uint8_t i1 = insert1 + 1; // oldest
    uint8_t i2 = insert2 + 1;
    for (i = 0; i < NUM_DOTS; i++ , i1++, i2++)
    {
        i1 = (i1 < NUM_DOTS) ? i1 : 0;
        i2 = (i2 < NUM_DOTS) ? i2 : 0;
        drawPixel(i, buffer1[i1] - buffer2[i2] + PLOT_SHIFT, WHITE);
    }
    //plotCircle(64,32,10,WHITE);
    // pos[1] = PLOT_SCALE * buffer[i] + PLOT_SHIFT
   
}
void ICACHE_FLASH_ATTR AdjustPlotDotsSingle(int16_t buffer1[], uint16_t insert1)
{
    // Plots a graph with x from 0 to 119 and y from buffer1
    uint8_t i;
    uint8_t i1 = insert1 + 1; // oldest
    for (i = 0; i < NUM_DOTS; i++ , i1++)
    {
        i1 = (i1 < NUM_DOTS) ? i1 : 0;
        drawPixel(i, buffer1[i1] + PLOT_SHIFT, WHITE);
        //os_printf("(%d, %d) ", i, buffer1[i1] + PLOT_SHIFT);
    }
    //os_printf("\n");
}


void ICACHE_FLASH_ATTR cmInit(void)
{
    // External from mode_dance to set brightness when using dance mode display
    danceBrightnessIdx = 2;
    // Give us reliable button input.
	enableDebounce(false);

	// Reset mode time tracking.
	modeStartTime = system_get_time();
	modeTime = 0;

    // Reset state stuff.
	cmChangeState(CM_TITLE);

    // Set up all initialization and allocation of memory
    cmNewSetup();

	// Start the update loop.
    os_timer_disarm(&timerHandleUpdate);
    os_timer_setfn(&timerHandleUpdate, (os_timer_func_t*)cmUpdate, NULL);
    os_timer_arm(&timerHandleUpdate, UPDATE_TIME_MS, 1);
}

void ICACHE_FLASH_ATTR cmDeInit(void)
{
    cmFreeMemory();
	os_timer_disarm(&timerHandleUpdate);
}

void ICACHE_FLASH_ATTR cmButtonCallback(uint8_t state, int button __attribute__((unused)), int down __attribute__((unused)))
{
	cmButtonState = state;	// Set the state of all buttons
}

void ICACHE_FLASH_ATTR cmAccelerometerCallback(accel_t* accel)
{
    // Set the accelerometer values
    // x coor relates to left right on OLED
    // y coor relates to up down on OLED
    cmAccel.x = accel->y;
    cmAccel.y = accel->x;
    cmAccel.z = accel->z;
}

/**
 * Intermediate function which adjusts brightness and sets the LEDs
 *
 * @param ledData    The LEDs to be scaled, then set
 * @param ledDataLen The length of the LEDs to set
 */
void ICACHE_FLASH_ATTR setCMLeds(led_t* ledData, uint8_t ledDataLen)
{
    uint8_t i;
    led_t ledsAdjusted[NUM_LIN_LEDS];
    for(i = 0; i < ledDataLen / sizeof(led_t); i++)
    {
        ledsAdjusted[i].r = ledData[i].r / cmBrightnesses[cmBrightnessIdx];
        ledsAdjusted[i].g = ledData[i].g / cmBrightnesses[cmBrightnessIdx];
        ledsAdjusted[i].b = ledData[i].b / cmBrightnesses[cmBrightnessIdx];
    }
    setLeds(ledsAdjusted, ledDataLen);
}

void ICACHE_FLASH_ATTR cmUpdate(void* arg __attribute__((unused)))
{
	// Update time tracking.
    // NOTE: delta time is in microseconds.
    // UPDATE time is in milliseconds.

	uint32_t newModeTime = system_get_time() - modeStartTime;
	uint32_t newStateTime = system_get_time() - stateStartTime;
	deltaTime = newModeTime - modeTime;
	modeTime = newModeTime;
	stateTime = newStateTime;

	// Handle Input (based on the state)
	switch( currState )
    {
        case CM_TITLE:
        {
			cmTitleInput();
            break;
        }
        case CM_GAME:
        case CM_AUTO:
        {
			cmGameInput();
            break;
        }
        case CM_SCORES:
        {
			cmScoresInput();
            break;
        }
        case CM_GAMEOVER:
        {
            cmGameoverInput();
            break;
        }
        default:
            break;
    };

    // Mark what our inputs were the last time we acted on them.
    cmLastButtonState = cmButtonState;
    cmLastAccel = cmAccel;

    // Handle Game Logic (based on the state)
	switch( currState )
    {
        case CM_TITLE:
        {
			cmTitleUpdate();
            break;
        }
        case CM_GAME:
        {
			cmGameUpdate();
            break;
        }
        case CM_AUTO:
        {
			cmAutoGameUpdate();
            break;
        }
        case CM_SCORES:
        {
			cmScoresUpdate();
            break;
        }
        case CM_GAMEOVER:
        {
            cmGameoverUpdate();
            break;
        }
        default:
            break;
    };

	// Handle Drawing Frame (based on the state)
	switch( currState )
    {
        case CM_TITLE:
        {
			cmTitleDisplay();
            break;
        }
        case CM_GAME:
        case CM_AUTO:
        {
			cmGameDisplay();
            break;
        }
        case CM_SCORES:
        {
			cmScoresDisplay();
            break;
        }
        case CM_GAMEOVER:
        {
            cmGameoverDisplay();
            break;
        }
        default:
            break;
    };
}

void ICACHE_FLASH_ATTR cmTitleInput(void)
{
    //button a = start game
    if(cmIsButtonPressed(BTN_TITLE_START_GAME))
    {
        cmChangeState(CM_GAME);
    }
    //button b = go to score screen
    else if(cmIsButtonPressed(BTN_TITLE_START_SCORES))
    {
        cmChangeState(CM_SCORES);
    }
}

void ICACHE_FLASH_ATTR cmChangeLevel(void)
{
    switch (cmLevel)
        {
        case BOX_LEVEL:
            cmLevel = PRACTICE_LEVEL;
            break;
        case PRACTICE_LEVEL:
            cmLevel = EASY_LEVEL;
            break;
        case EASY_LEVEL:
            cmLevel = MIDDLE_LEVEL;
            break;
        case MIDDLE_LEVEL:
            cmLevel = HARD_LEVEL;
            break;
        case HARD_LEVEL:
            cmLevel = KILLER_LEVEL;
            break;
        case KILLER_LEVEL:
            cmLevel = IMPOSSIBLE_LEVEL;
            break;
        case IMPOSSIBLE_LEVEL:
            cmLevel = BOX_LEVEL;
            break;
        default:
            break;
        }
}
void ICACHE_FLASH_ATTR cmGameInput(void)
{
    //button b = abort and restart at same level
    if(cmIsButtonPressed(BTN_GAME_RIGHT))
    {
        cmFreeMemory();
        cmNewSetup();
        cmChangeState(CM_GAME);
    }
    //button a = abort and automatically do cm
    else if(cmIsButtonPressed(BTN_GAME_LEFT))
    {
        cmChangeState(CM_AUTO);
    }
}

void ICACHE_FLASH_ATTR cmScoresInput(void)
{
	//button a = hold to clear scores.
    if(holdingClearScore && cmIsButtonDown(BTN_SCORES_CLEAR_SCORES))
    {
        clearScoreTimer += deltaTime;
        if (clearScoreTimer >= CLEAR_SCORES_HOLD_TIME)
        {
            clearScoreTimer = 0;
            memset(highScores, 0, NUM_CM_HIGH_SCORES * sizeof(uint32_t));
            //saveHighScores();
            //loadHighScores();
            //cmSetLastScore(0);
        }
    }
    else if(cmIsButtonUp(BTN_SCORES_CLEAR_SCORES))
    {
        clearScoreTimer = 0;
    }
    // This is added to prevent people holding left from the previous screen from accidentally clearing their scores.
    else if(cmIsButtonPressed(BTN_SCORES_CLEAR_SCORES))
    {
        holdingClearScore = true;
    }

    //button b = go to title screen
    if(cmIsButtonPressed(BTN_SCORES_START_TITLE))
    {
        cmChangeState(CM_TITLE);
    }
}

void ICACHE_FLASH_ATTR cmGameoverInput(void)
{
    //button a = start game
    if(cmIsButtonPressed(BTN_GAMEOVER_START_GAME))
    {
        cmFreeMemory();
        cmNewSetup();
        cmChangeState(CM_TITLE);
    }
    //button b = go to title screen
    else if(cmIsButtonPressed(BTN_GAMEOVER_START_TITLE))
    {
        cmChangeLevel();
        cmFreeMemory();
        cmNewSetup();
        cmChangeState(CM_TITLE);
    }
}

void ICACHE_FLASH_ATTR cmTitleUpdate(void)
{
}

void ICACHE_FLASH_ATTR cmGameUpdate(void)
{
    // bool gonethruany;
    static struct maxtime_t CM_updatedisplay_timer = { .name="CM_updateDisplay"};
    maxTimeBegin(&CM_updatedisplay_timer);

    clearDisplay();

 
    showcount += 1;
    if (showcount > FRAME_RESET) showcount = 0;
    if (showcount > 0) return;
    if (pause) return;

    xAccel = cmAccel.x;
    yAccel = cmAccel.y;
    zAccel = cmAccel.z;

    // IPAD accel = list(map(lambda x, y : x + y, motion.get_gravity(),  motion.get_user_acceleration()))
    int16_t normAccel = sqrt((((int32_t)xAccel)^2) + (((int32_t)yAccel)^2) + (((int32_t)zAccel)^2)  );
    // empirical adjustment
    //normAccel *= 3;

    if (SPECIAL_EFFECT)
    {
        // intertwine raw signal with smoothed
        // only need these buffers if want special effect
        bufXaccelInsert = circularPush(xAccel, bufXaccelInsert, bufXaccel);
        bufYaccelInsert = circularPush(yAccel, bufYaccelInsert, bufYaccel);
        bufZaccelInsert = circularPush(zAccel, bufZaccelInsert, bufZaccel);
    }
    // slightly smoothed signal
    float alphaFast = ALPHA_FAST;

    smoothNormAccel = IIRFilter(alphaFast, normAccel, smoothNormAccel);
    bufNormAccelInsert = circularPush(smoothNormAccel, bufNormAccelInsert, bufNormAccel);

    smoothXaccel = IIRFilter(alphaFast, xAccel, smoothXaccel);
    bufXaccelInsert = circularPush(smoothXaccel, bufXaccelInsert, bufXaccel);

    smoothYaccel = IIRFilter(alphaFast, yAccel, smoothYaccel);
    bufYaccelInsert = circularPush(smoothYaccel, bufYaccelInsert, bufYaccel);

    smoothZaccel = IIRFilter(alphaFast, zAccel, smoothZaccel);
    bufZaccelInsert = circularPush(smoothZaccel, bufZaccelInsert, bufZaccel);


    // high pass by removing highly smoothed low pass (dc bias)
    float alphaSlow = ALPHA_SLOW;

    lowPassNormAccel = IIRFilter(alphaSlow, normAccel, lowPassNormAccel);
    bufHighPassNormAccelInsert= circularPush(smoothNormAccel - lowPassNormAccel, bufHighPassNormAccelInsert, bufHighPassNormAccel);

    // low pass for the three axes

    lowPassXaccel = IIRFilter(alphaSlow, xAccel, lowPassXaccel);
    bufLowPassXaccelInsert = circularPush(lowPassXaccel, bufLowPassXaccelInsert, bufLowPassXaccel);

    lowPassYaccel = IIRFilter(alphaSlow, yAccel, lowPassYaccel);
    bufLowPassYaccelInsert = circularPush(lowPassYaccel, bufLowPassYaccelInsert, bufLowPassYaccel);

    lowPassZaccel = IIRFilter(alphaSlow, zAccel, lowPassZaccel);
    bufLowPassZaccelInsert = circularPush(lowPassZaccel, bufLowPassZaccelInsert, bufLowPassZaccel);

    // Plot slightly smoothed less dc bias by adjusting the dots
    // AdjustPlotDotsSingle(bufHighPassNormAccel, bufHighPassNormAccel);
    AdjustPlotDots(bufXaccel,bufXaccelInsert, bufLowPassXaccel, bufLowPassXaccelInsert);
    AdjustPlotDots(bufYaccel,bufYaccelInsert, bufLowPassYaccel, bufLowPassYaccelInsert);
    AdjustPlotDots(bufZaccel, bufZaccelInsert, bufLowPassZaccel, bufLowPassZaccelInsert);

    // Identify stillness
    float alphaActive = ALPHA_ACTIVE; // smoothing of deviaton from mean
    smoothActivity = IIRFilter(alphaActive, abs(bufHighPassNormAccel[-1]), smoothActivity);

    CM_printf("smoothActivity = %d", smoothActivity);
    // Estimate bpm when movement activity

    // Might not need checking for stillness if use tolerance for cross checking (see below)
    float alphaCross = ALPHA_CROSS; // for smoothing gaps between zero crossing

    // if very still stop updateing period estimation
    if (smoothActivity < 1/30.0)
    {
        if (!still)
        {
            still = true;
            //if (SOUND_ON) //sound.stop_all_effects()}
        }
    } else { // estimate bpm
        if (still)
        {
            skipNextCross = true;
            still = false;
        }
    }
//#define USE_ZERO_CROSSING
#ifdef USE_ZERO_CROSSING
    // zero crossing NO tolerance check
    if bufHighPassNormAccel[-1] * bufHighPassNormAccel[-2] < 0:
#else
    // downward zero crossing with tolerance
    if ((bufHighPassNormAccel[-1] > 0) & (bufHighPassNormAccel[-2] < 0) & ((bufHighPassNormAccel[-1] - bufHighPassNormAccel[-2]) > CROSS_TOL))
#endif
    {
        if (skipNextCross)
        {
            skipNextCross = false;
        } else {
            crossinterval = modeTime - lastzerocrosst;
        }
        lastzerocrosst = modeTime;
    }

    // period for downward crossing
    aveZeroCrossInterval = (1 - alphaCross) * aveZeroCrossInterval + alphaCross * crossinterval;
    //prevHighPassNormAccel = bufHighPassNormAccel[-1];
    //bpm = 30/aveZeroCrossInterval; // if using 1/2 period estimate for crossing
    // int16_t bpm = 60 * MS_TO_US_FACTOR * S_TO_MS_FACTOR / aveZeroCrossInterval;

    //TODO should be in cmGameDisplay()
    // graphical view of bpm could be here

    // graphical view of amp could be here



    //TODO pitch could simple be computed by tilt
    //   as dac suggestd, button to actuate sound so
    //   make a musical instrument
    if (SOUND_ON)
    {

        // TODO choose nice range of bpm and map to nice pitch range
        // could be from 110 to 880 say if buzzer can accept changing
        // freq. Or could be discrete jumps to pentatonic scale
        //  from buzzer.h to play freq f need  compute period (5,000,000 / (2 * f))
        //  to set up a noteFreq_t (NOTE should be notePeriod_t)
        //  then set up musicalNote_t with this and a duration

        //pitch = (min( max(bpm, 75), 150) - 75) / 75;


        // song_t testSong =
        // {
        //     .shouldLoop = true,
        //     .numNotes = 1,
        //     .notes = FIX to take period

        // };

        // Play this song continous and stop then change to another freq
        // or use button to start and stop

        // Is it possible to play bufHighPassNormAccel at 160 times faster?
        // this would convert 30 to 300 bpm to 80 to 800 Hz
        // audible indication of bpm
        //sound.play_effect('game:Beep', volume = 300*smoothActivity/500, pitch=pitch)
    }

    //os_printf("bpm %d, activity %d\n", bpm, smoothActivity);

    uint8_t ledr;
    uint8_t ledg;
    uint8_t ledb;

    // Various led options
    // colors via 3 axes strength
    //scaleLed = 8;
    //ledr = scaleLed * math.fabs(graphRed[-1].position[1]-384)/384;
    //ledg = scaleLed * math.fabs(graphGreen[-1].position[1]-384)/384;
    //ledb = scaleLed * math.fabs(graphBlue[-1].position[1]-384)/384;

    // TODO related to bpm - map nice range to 0 to 255 for angle
    int16_t angle = 0; // replace with nice formula
    uint32_t colorToShow = EHSVtoHEX(angle, 0xFF, 0xFF);

    ledr = (colorToShow >>  0) & 0xFF;
    ledg = (colorToShow >>  8) & 0xFF;
    ledb = (colorToShow >> 16) & 0xFF;


    // allcolored the same
    for (uint8_t i=0; i < NUM_LIN_LEDS; i++)
    {
        //use axis colors or hue colors computed above
        leds[ledOrderInd[i]].r = ledr;
        leds[ledOrderInd[i]].g = ledg;
        leds[ledOrderInd[i]].b = ledb;
        //clear if going to cycle lights
        leds[ledOrderInd[i]].r  = 0;
        leds[ledOrderInd[i]].g  = 0;
        leds[ledOrderInd[i]].b  = 0;
    }

    // Spin the leds
    if (modeTime - ledPrevIncTime > aveZeroCrossInterval / NUM_LIN_LEDS / REVOLUTIONS_PER_ZERO_CROSSING)
    {
        ledPrevIncTime = modeTime;
        ledcycle += 1;
        if (ledcycle >= NUM_LIN_LEDS) ledcycle = 0;
    }

    // Put color from above in the one LED that should go
    leds[ledOrderInd[ledcycle]].r = ledr;
    leds[ledOrderInd[ledcycle]].g = ledg;
    leds[ledOrderInd[ledcycle]].b = ledb;

    setCMLeds(leds, sizeof(leds));
    // Test if  finished
    if (false)
    {
        // Compute score
        score = 100.0;
        gameover = true;
    }
    maxTimeEnd(&CM_updatedisplay_timer);
}

void ICACHE_FLASH_ATTR cmScoresUpdate(void)
{
    // Do nothing.
}


void ICACHE_FLASH_ATTR cmAutoGameUpdate(void)
{

}

void ICACHE_FLASH_ATTR cmGameoverUpdate(void)
{
}

void ICACHE_FLASH_ATTR cmTitleDisplay(void)
{
	// Clear the display.
    clearDisplay();

    // Shake It
    plotCenteredText(0, 5, 127, "SHAKE-COLOR", RADIOSTARS, WHITE);

    plotCenteredText(0, OLED_HEIGHT/2, 127, (char*)levelName[cmLevel], IBM_VGA_8, WHITE);

    // SCORES   START
    plotText(0, OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)), "SCORES", IBM_VGA_8, WHITE);
    plotText(OLED_WIDTH - getTextWidth("START", IBM_VGA_8), OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)), "START", IBM_VGA_8, WHITE);


}

void ICACHE_FLASH_ATTR cmGameDisplay(void)
{
    // char uiStr[32] = {0};
    // Clear the display
    //clearDisplay();


    if (gameover)
    {
        cmChangeState(CM_GAMEOVER);
    }
}

void ICACHE_FLASH_ATTR cmScoresDisplay(void)
{
    // Clear the display
    clearDisplay();

    plotCenteredText(0, 0, OLED_WIDTH, "HIGH SCORES", IBM_VGA_8, WHITE);

    char uiStr[32] = {0};
    // 1. 99999
    ets_snprintf(uiStr, sizeof(uiStr), "1. %d", highScores[0]);
    plotCenteredText(0, (3*FONT_HEIGHT_TOMTHUMB)+1, OLED_WIDTH, uiStr, TOM_THUMB, WHITE);

    // 2. 99999
    ets_snprintf(uiStr, sizeof(uiStr), "2. %d", highScores[1]);
    plotCenteredText(0, (5*FONT_HEIGHT_TOMTHUMB)+1, OLED_WIDTH, uiStr, TOM_THUMB, WHITE);

    // 3. 99999
    ets_snprintf(uiStr, sizeof(uiStr), "3. %d", highScores[2]);
    plotCenteredText(0, (7*FONT_HEIGHT_TOMTHUMB)+1, OLED_WIDTH, uiStr, TOM_THUMB, WHITE);

    // YOUR LAST SCORE:
    ets_snprintf(uiStr, sizeof(uiStr), "YOUR LAST SCORE: %d", 31415926);
    plotCenteredText(0, (9*FONT_HEIGHT_TOMTHUMB)+1, OLED_WIDTH, uiStr, TOM_THUMB, WHITE);


    //TODO: explicitly add a hold to the text, or is the inverse effect enough.
    // (HOLD) CLEAR SCORES      TITLE
    plotText(1, OLED_HEIGHT - (1 * (FONT_HEIGHT_TOMTHUMB + 1)), "CLEAR SCORES", TOM_THUMB, WHITE);

    // fill the clear scores area depending on how long the button's held down.
    if (clearScoreTimer != 0)
    {
        double holdProgress = ((double)clearScoreTimer / (double)CLEAR_SCORES_HOLD_TIME);
        uint8_t holdFill = (uint8_t)(holdProgress * (getTextWidth("CLEAR SCORES", TOM_THUMB)+2));
        fillDisplayArea(0, (OLED_HEIGHT - (1 * (FONT_HEIGHT_TOMTHUMB + 1))) - 1, holdFill, OLED_HEIGHT, INVERSE);
    }

    plotText(OLED_WIDTH - getTextWidth("TITLE", TOM_THUMB) - 1, OLED_HEIGHT - (1 * (FONT_HEIGHT_TOMTHUMB + 1)), "TITLE", TOM_THUMB, WHITE);
}

void ICACHE_FLASH_ATTR cmGameoverDisplay(void)
{
    switch (cmLevel)
    {
    case BOX_LEVEL:
        danceTimerMode1();
        break;
    case PRACTICE_LEVEL:
        danceTimerMode2();
        break;
    case EASY_LEVEL:
        danceTimerMode3();
        break;
    case MIDDLE_LEVEL:
        danceTimerMode4();
        break;
    case HARD_LEVEL:
        danceTimerMode13();
        break;
    case KILLER_LEVEL:
        danceTimerMode16();
        break;
    case IMPOSSIBLE_LEVEL:
        danceTimerMode17();
        break;
    default:
        break;
    }

    // We don't clear the display because we want the playfield to appear in the background.
    // Draw a centered bordered window.

    //TODO: #define these instead of variables here?
    uint8_t windowXMargin = 18;
    uint8_t windowYMarginTop = 5;
    uint8_t windowYMarginBot = 5;

    uint8_t titleTextYOffset = 5;
    uint8_t highScoreTextYOffset = titleTextYOffset + FONT_HEIGHT_IBMVGA8 + 5;
    uint8_t scoreTextYOffset = highScoreTextYOffset + FONT_HEIGHT_TOMTHUMB + 5;
    uint8_t controlTextYOffset = OLED_HEIGHT - windowYMarginBot - FONT_HEIGHT_TOMTHUMB - 5;
    uint8_t controlTextXPadding = 5;

    // Draw a centered bordered window.
    fillDisplayArea(windowXMargin, windowYMarginTop, OLED_WIDTH - windowXMargin, OLED_HEIGHT - windowYMarginBot, BLACK);
    plotRect(windowXMargin, windowYMarginTop, OLED_WIDTH - windowXMargin, OLED_HEIGHT - windowYMarginBot, WHITE);

    // GAME OVER
    plotCenteredText(windowXMargin, windowYMarginTop + titleTextYOffset, OLED_WIDTH - windowXMargin, "GAME OVER", IBM_VGA_8, WHITE);

    // HIGH SCORE! or YOUR SCORE:
    if (newHighScore)
    {
        plotCenteredText(windowXMargin, windowYMarginTop + highScoreTextYOffset, OLED_WIDTH - windowXMargin, "HIGH SCORE!", TOM_THUMB, WHITE);
    }
    else
    {
        plotCenteredText(windowXMargin, windowYMarginTop + highScoreTextYOffset, OLED_WIDTH - windowXMargin, "YOUR SCORE:", TOM_THUMB, WHITE);
    }

    // 1230495
    char scoreStr[32] = {0};
    ets_snprintf(scoreStr, sizeof(scoreStr), "%d", score);
    plotCenteredText(windowXMargin, windowYMarginTop + scoreTextYOffset, OLED_WIDTH - windowXMargin, scoreStr, IBM_VGA_8, WHITE);

    // TITLE    RESTART
    plotText(windowXMargin + controlTextXPadding, controlTextYOffset, "NEW LEVEL", TOM_THUMB, WHITE);
    plotText(OLED_WIDTH - windowXMargin - getTextWidth("SAME LEVEL", TOM_THUMB) - controlTextXPadding, controlTextYOffset, "SAME LEVEL", TOM_THUMB, WHITE);
}

// helper functions.

/**
 * @brief Initializer for cm, allocates memory for work arrays
 *
 * @param
 * @return pointers to the work array???
 */
void ICACHE_FLASH_ATTR cmNewSetup(void)
{
    //Allocate some working array memory now
    //TODO is memory being freed up appropriately?
    bufNormAccel = (int16_t  *)malloc (sizeof (int16_t ) * NUM_DOTS);
    bufHighPassNormAccel = (int16_t  *)malloc (sizeof (int16_t ) * NUM_DOTS);
    bufXaccel = (int16_t  *)malloc (sizeof (int16_t ) * NUM_DOTS);
    bufLowPassXaccel = (int16_t  *)malloc (sizeof (int16_t ) * NUM_DOTS);
    bufYaccel = (int16_t  *)malloc (sizeof (int16_t ) * NUM_DOTS);
    bufLowPassYaccel = (int16_t  *)malloc (sizeof (int16_t ) * NUM_DOTS);
    bufZaccel = (int16_t  *)malloc (sizeof (int16_t ) * NUM_DOTS);
    bufLowPassZaccel = (int16_t  *)malloc (sizeof (int16_t ) * NUM_DOTS);

    bufNormAccelInsert = 0;
    bufHighPassNormAccelInsert = 0;
    bufXaccelInsert = 0;
    bufLowPassXaccelInsert = 0;
    bufYaccelInsert = 0;
    bufLowPassYaccelInsert = 0;
    bufZaccelInsert = 0;
    bufLowPassZaccelInsert = 0;
    // int16_t i;
    // int16_t startvert = 0;
    memset(leds, 0, sizeof(leds));

    gameover = false;


    REVOLUTIONS_PER_ZERO_CROSSING = 0.5;
    PLOT_SCALE = 32;
    PLOT_SHIFT = 32;
    // do via timer now 10 fps FRAME_RESET = 0 // 0 for 60 fps, 1 for 30 fps, 2 for 20 fps, k for 60/(k+1) fps

    showcount = 0; // used for skipping frames
    adj = SCREEN_BORDER;
    wid = 128 - 2 * SCREEN_BORDER;

    ledcycle = 0;

    lowPassNormAccel = 0;
    lowPassXaccel = 0;
    lowPassYaccel = 0;
    lowPassZaccel = 0;
    smoothNormAccel = 0;
    smoothXaccel = 0;
    smoothYaccel = 0;
    smoothZaccel = 0;
    prevHighPassNormAccel = 0;
    smoothActivity = 0;

    // times float? or use cycles?
    lastzerocrosst = 0;
    ledPrevIncTime = 0;
    aveZeroCrossInterval = 5;
    crossinterval = 5;

    pause = false;
    skipNextCross = true;
    still = true;



    system_print_meminfo();
    os_printf("Free Heap %d\n", system_get_free_heap_size());


}

/**
 * Called when cm is exited or before making new cm
 */
void ICACHE_FLASH_ATTR cmFreeMemory(void)
{
    free(bufNormAccel);
    free(bufHighPassNormAccel);
    free(bufXaccel);
    free(bufLowPassXaccel);
    free(bufYaccel);
    free(bufLowPassYaccel);
    free(bufZaccel);
    free(bufLowPassZaccel);
}


void ICACHE_FLASH_ATTR cmChangeState(cmState_t newState)
{
	prevState = currState;
    currState = newState;
	stateStartTime = system_get_time();
	stateTime = 0;

    switch( currState )
    {
        case CM_TITLE:
            // Clear leds
            memset(leds, 0, sizeof(leds));
            setCMLeds(leds, sizeof(leds));
            break;
        case CM_GAME:
            // All game restart functions happen here.
            //loadHighScores();
            // TODO: should I be seeding this, or re-seeding this, and if so, with what?
            srand((uint32_t)(cmAccel.x + cmAccel.y * 3 + cmAccel.z * 5)); // Seed the random number generator.
            break;
        case CM_AUTO:
            //loadHighScores();
            break;
        case CM_SCORES:
            //loadHighScores();
            clearScoreTimer = 0;
            holdingClearScore = false;
            break;
        case CM_GAMEOVER:
            // Update high score if needed.
            if (prevState != CM_AUTO)
            {
                //newHighScore = updateHighScores(score);
                //if (newHighScore) saveHighScores();
                // Save out the last score.
                //cmSetLastScore(score);
            }
            break;
        default:
            break;
    };
}

bool ICACHE_FLASH_ATTR cmIsButtonPressed(uint8_t button)
{
    return (cmButtonState & button) && !(cmLastButtonState & button);
}

bool ICACHE_FLASH_ATTR cmIsButtonReleased(uint8_t button)
{
    return !(cmButtonState & button) && (cmLastButtonState & button);
}

bool ICACHE_FLASH_ATTR cmIsButtonDown(uint8_t button)
{
    return cmButtonState & button;
}

bool ICACHE_FLASH_ATTR cmIsButtonUp(uint8_t button)
{
    return !(cmButtonState & button);
}


// Draw text centered between x0 and x1.
void ICACHE_FLASH_ATTR plotCenteredText(uint8_t x0, uint8_t y, uint8_t x1, char* text, fonts font, color col)
{
    /*// We only get width info once we've drawn.
    // So we draw the text as inverse to get the width.
    uint8_t textWidth = plotText(x0, y, text, font, INVERSE) - x0 - 1; // minus one accounts for the return being where the cursor is.

    // Then we draw the inverse back over it to restore it.
    plotText(x0, y, text, font, INVERSE);*/
    uint8_t textWidth = getTextWidth(text, font);

    // Calculate the correct x to draw from.
    uint8_t fullWidth = x1 - x0 + 1;
    // NOTE: This may result in strange behavior when the width of the drawn text is greater than the distance between x0 and x1.
    uint8_t widthDiff = fullWidth - textWidth;
    uint8_t centeredX = x0 + (widthDiff / 2);

    // Then we draw the correctly centered text.
    plotText(centeredX, y, text, font, col);
}

uint8_t getTextWidth(char* text, fonts font)
{
    // NOTE: The inverse, inverse is cute, but 2 draw calls, could we draw it outside of the display area but still in bounds of a uint8_t?

    // We only get width info once we've drawn.
    // So we draw the text as inverse to get the width.
    uint8_t textWidth = plotText(0, 0, text, font, INVERSE) - 1; // minus one accounts for the return being where the cursor is.

    // Then we draw the inverse back over it to restore it.
    plotText(0, 0, text, font, INVERSE);
    return textWidth;
}

/*
static void loadHighScores(void)
{
    //memcpy(highScores, cmGetHighScores(),  NUM_CM_HIGH_SCORES * sizeof(uint32_t));
}

static void saveHighScores(void)
{
    //cmSetHighScores(highScores);
}

static bool updateHighScores(uint32_t newScore)
{
    bool highScore = false;
    uint32_t placeScore = newScore;
    for (int i = 0; i < NUM_CM_HIGH_SCORES; i++)
    {
        // Get the current score at this index.
        uint32_t currentScore = highScores[i];

        if (placeScore >= currentScore)
        {
            highScores[i] = placeScore;
            placeScore = currentScore;
            highScore = true;
        }
    }
    return highScore;
}
*/