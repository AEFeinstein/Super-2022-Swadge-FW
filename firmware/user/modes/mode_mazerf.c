/*
*   mode_mazerf.c
*
*   Created on: 21 Sept 2019
*               Author: bbkiw
*       Refactor of maze using Jonathan Moriarty basic set up
*/

//TODO
// UI advice - nice to be able to retry the same maze
//    abort out of too hard level
// Put level name on home screen (make up appropriate names)
// Changing to next level not intuitive and happens only after goes
// Maybe in end of game have re try same maze or new maze
// back to title screen.
// A way to jump out of maze if too hard (e.g. hardest to very hard)
//    hardest cant be done without touching the walls
// DONE mostly - Improve Score
// DONE HIGH appearing in window overlaps maze - removed
// Maybe flash special pattern when get max score
// Sound
// DONE Maybe have lights in 4 corner of screen light to
//  indicate where to aim for.
// DONE - must traverse corner anti clockwise
//      starting upper left. RED light when hit them
// FIXED BUG in setmazeLeds effectively erases leds if game callback
// PROBABLY FIXED BUG Teleport sometimes. stop this
// DONE see code comment labeled TELPORT FIX
// but when removed would get in constant teleporting
// if safety stop was off. Differs from matlab test
// would be nice to find reason
// will try running with gdb

#include <osapi.h>
#include <user_interface.h>
#include <stdlib.h>
#include "maxtime.h"
#include "user_main.h"  //swadge mode
#include "mode_mazerf.h"
#include "mode_dance.h"
#include "ccconfig.h"
#include "DFT32.h"
#include "buttons.h"
#include "oled.h"       //display functions
#include "font.h"       //draw text
#include "bresenham.h"  //draw shapes
#include "linked_list.h" //custom linked list
#include "custom_commands.h" //saving and loading high scores and last scores
#include "mazegen.h"
#include "math.h"

/*============================================================================
 * Defines
 *==========================================================================*/

//#define MAZE_DEBUG_PRINT
#ifdef MAZE_DEBUG_PRINT
    #include <stdlib.h>
    #define maze_printf(...) os_printf(__VA_ARGS__)
#else
    #define maze_printf(...)
#endif

//#ifndef max
//    #define max(a,b) ((a) > (b) ? (a) : (b))
//#endif
#ifndef min
    #define min(a,b) ((a) < (b) ? (a) : (b))
#endif

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

// update task (Richard maxTime has 28 ms for most complicated maze)
#define UPDATE_TIME_MS 100

// time info.
#define MS_TO_US_FACTOR 1000
#define MS_TO_S_FACTOR 1000
//#define US_TO_MS_FACTOR 0.001

#define CLEAR_SCORES_HOLD_TIME (5 * MS_TO_US_FACTOR * MS_TO_S_FACTOR)

#define NUM_MZ_HIGH_SCORES 3
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

// any enums go here.

typedef enum
{
    MZ_TITLE,   // title screen
    MZ_GAME,    // play the actual game
    MZ_AUTO,    // automataically play the actual game
    MZ_SCORES,  // high scores
    MZ_GAMEOVER // game over
} mazeState_t;

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
uint32_t highScores[NUM_MZ_HIGH_SCORES];
bool newHighScore;

// function prototypes go here.
/*============================================================================
 * Prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR mzInit(void);
void ICACHE_FLASH_ATTR mzDeInit(void);
void ICACHE_FLASH_ATTR mzButtonCallback(uint8_t state, int button, int down);
void ICACHE_FLASH_ATTR mzAccelerometerCallback(accel_t* accel);

// Set up workspace and make random maze
void ICACHE_FLASH_ATTR mzNewMazeSetUp(void);
void ICACHE_FLASH_ATTR mazeFreeMemory(void);

// game loop functions.
void ICACHE_FLASH_ATTR mzUpdate(void* arg);

// handle inputs.
void ICACHE_FLASH_ATTR mzTitleInput(void);
void ICACHE_FLASH_ATTR mzGameInput(void);
void ICACHE_FLASH_ATTR mzScoresInput(void);
void ICACHE_FLASH_ATTR mzGameoverInput(void);

// update any input-unrelated logic.
void ICACHE_FLASH_ATTR mzTitleUpdate(void);
void ICACHE_FLASH_ATTR mzGameUpdate(void);
void ICACHE_FLASH_ATTR mzAutoGameUpdate(void);
void ICACHE_FLASH_ATTR mzScoresUpdate(void);
void ICACHE_FLASH_ATTR mzGameoverUpdate(void);

// draw the frame.
void ICACHE_FLASH_ATTR mzTitleDisplay(void);
void ICACHE_FLASH_ATTR mzGameDisplay(void);
void ICACHE_FLASH_ATTR mzScoresDisplay(void);
void ICACHE_FLASH_ATTR mzGameoverDisplay(void);

// mode state management.
void ICACHE_FLASH_ATTR mzChangeState(mazeState_t newState);

// input checking.
bool ICACHE_FLASH_ATTR mzIsButtonPressed(uint8_t button);
bool ICACHE_FLASH_ATTR mzIsButtonReleased(uint8_t button);
bool ICACHE_FLASH_ATTR mzIsButtonDown(uint8_t button);
bool ICACHE_FLASH_ATTR mzIsButtonUp(uint8_t button);

// drawing functions.
static void ICACHE_FLASH_ATTR plotCenteredText(uint8_t x0, uint8_t y, uint8_t x1, char* text, fonts font, color col);
static uint8_t getTextWidth(char* text, fonts font);

// randomizer operations.
void mzshuffle(int length, int array[length]);
// score operations.
static void loadHighScores(void);
static void saveHighScores(void);
static bool updateHighScores(uint32_t newScore);

// Additional Helper
void ICACHE_FLASH_ATTR setmazeLeds(led_t* ledData, uint8_t ledDataLen);
get_maze_output_t ICACHE_FLASH_ATTR get_maze(uint8_t width, uint8_t height, uint8_t xleft[], uint8_t xright[],
        uint8_t ybot[], uint8_t ytop[], uint8_t xsol[], uint8_t ysol[], float scxcexits[], float scycexits[],
        uint8_t mazescalex, uint8_t mazescaley);
uint8_t ICACHE_FLASH_ATTR intervalsmeet(float a, float c, float b, float d, float e, float f, float param[]);
uint8_t ICACHE_FLASH_ATTR  gonethru(float b_prev[], float b_now[], float p_1[], float p_2[], float rball,
                                    float b_nowadjusted[], float param[]);
int16_t ICACHE_FLASH_ATTR  incrementifnewvert(int16_t nwi, int16_t startind, int16_t endind);
int16_t ICACHE_FLASH_ATTR  incrementifnewhoriz(int16_t nwi, int16_t startind, int16_t endind);
void ICACHE_FLASH_ATTR changeLevel(void);

/*============================================================================
 * Static Const Variables
 *==========================================================================*/

static const uint8_t mazeBrightnesses[] =
{
    0x01,
    0x08,
    0x40,
    0x80,
};


const char* levelName[] = {"BOX", "PRACTICE", "EASY", "MIDDLE", "HARD", "KILLER", "IMPOSSIBLE"};

/*============================================================================
 * Variables
 *==========================================================================*/


// game logic operations.

swadgeMode mazerfMode =
{
    .modeName = "Maze",
    .fnEnterMode = mzInit,
    .fnExitMode = mzDeInit,
    .fnButtonCallback = mzButtonCallback,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = mzAccelerometerCallback
};

accel_t mzAccel = {0};
accel_t mzLastAccel = {0};

accel_t mzLastTestAccel = {0};

uint8_t mzButtonState = 0;
uint8_t mzLastButtonState = 0;

uint8_t mazeBrightnessIdx = 2;
led_t leds[NUM_LIN_LEDS] = {{0}};
int maze_ledCount = 0;
static os_timer_t timerHandleUpdate = {0};

static uint32_t modeStartTime = 0; // time mode started in microseconds.
static uint32_t stateStartTime = 0; // time the most recent state started in microseconds.
static uint32_t deltaTime = 0;  // time elapsed since last update.
static uint32_t modeTime = 0;   // total time the mode has been running.
static uint32_t stateTime = 0;  // total time the game has been running.

static mazeState_t currState = MZ_TITLE;
static mazeState_t prevState;


float xAccel;
float yAccel;
float zAccel;
float len;
float scxc;
float scyc;
float scxcprev;
float scycprev;
float scxcexits[4];
float scycexits[4];
exitSpot_t exitInd;
bool exitHit[4];
uint8_t ledExitInd[] = {LED_UPPER_LEFT, LED_LOWER_LEFT, LED_LOWER_RIGHT, LED_UPPER_RIGHT};
float rballused;
float wiggleroom;
int xadj;
int yadj;
int16_t totalcyclestilldone;
uint16_t totalhitstilldone;
bool gameover;

uint8_t mazeLevel = IMPOSSIBLE_LEVEL;
uint8_t width;
uint8_t height; //Maze dimensions must be 1 less than multiples of 4
uint8_t mazescalex = 1;
uint8_t mazescaley = 1;
int16_t numwalls;
int16_t indSolution;
int16_t indSolutionStep;
int16_t numwallstodraw;
uint8_t* xleft = NULL;
uint8_t* xright = NULL;
uint8_t* ytop = NULL;
uint8_t* ybot = NULL;
uint8_t* xsol = NULL;
uint8_t* ysol = NULL;
uint8_t flashcount = 0;
uint8_t flashmax = 2;

float* extendedScaledWallXleft = NULL;
float* extendedScaledWallXright = NULL;
float* extendedScaledWallYtop = NULL;
float* extendedScaledWallYbot = NULL;


void ICACHE_FLASH_ATTR mzInit(void)
{
    // External from mode_dance to set brightness when using dance mode display
    setDanceBrightness(2);
    // Give us reliable button input.
    enableDebounce(false);

    // Reset mode time tracking.
    modeStartTime = system_get_time();
    modeTime = 0;

    // Reset state stuff.
    mzChangeState(MZ_TITLE);


    // Construct Random Maze
    changeLevel(); // will bring in PRACTICE_LEVEL
    mzNewMazeSetUp();

    // Start the update loop.
    os_timer_disarm(&timerHandleUpdate);
    os_timer_setfn(&timerHandleUpdate, (os_timer_func_t*)mzUpdate, NULL);
    os_timer_arm(&timerHandleUpdate, UPDATE_TIME_MS, 1);
}

void ICACHE_FLASH_ATTR mzDeInit(void)
{
    mazeFreeMemory();
    os_timer_disarm(&timerHandleUpdate);
}

void ICACHE_FLASH_ATTR mzButtonCallback(uint8_t state, int button __attribute__((unused)),
                                        int down __attribute__((unused)))
{
    mzButtonState = state;  // Set the state of all buttons
}

void ICACHE_FLASH_ATTR mzAccelerometerCallback(accel_t* accel)
{
    // Set the accelerometer values
    // x coor relates to left right on OLED
    // y coor relates to up down on OLED
    mzAccel.x = accel->y;
    mzAccel.y = accel->x;
    mzAccel.z = accel->z;
}

/**
 * Intermediate function which adjusts brightness and sets the LEDs
 *
 * @param ledData    The LEDs to be scaled, then set
 * @param ledDataLen The length of the LEDs to set
 */
void ICACHE_FLASH_ATTR setmazeLeds(led_t* ledData, uint8_t ledDataLen)
{
    uint8_t i;
    led_t ledsAdjusted[NUM_LIN_LEDS];
    for(i = 0; i < ledDataLen / sizeof(led_t); i++)
    {
        ledsAdjusted[i].r = ledData[i].r / mazeBrightnesses[mazeBrightnessIdx];
        ledsAdjusted[i].g = ledData[i].g / mazeBrightnesses[mazeBrightnessIdx];
        ledsAdjusted[i].b = ledData[i].b / mazeBrightnesses[mazeBrightnessIdx];
    }
    setLeds(ledsAdjusted, ledDataLen);
}

void ICACHE_FLASH_ATTR mzUpdate(void* arg __attribute__((unused)))
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
        case MZ_TITLE:
        {
            mzTitleInput();
            break;
        }
        case MZ_GAME:
        case MZ_AUTO:
        {
            mzGameInput();
            break;
        }
        case MZ_SCORES:
        {
            mzScoresInput();
            break;
        }
        case MZ_GAMEOVER:
        {
            mzGameoverInput();
            break;
        }
        default:
            break;
    };

    // Mark what our inputs were the last time we acted on them.
    mzLastButtonState = mzButtonState;
    mzLastAccel = mzAccel;

    // Handle Game Logic (based on the state)
    switch( currState )
    {
        case MZ_TITLE:
        {
            mzTitleUpdate();
            break;
        }
        case MZ_GAME:
        {
            mzGameUpdate();
            break;
        }
        case MZ_AUTO:
        {
            mzAutoGameUpdate();
            break;
        }
        case MZ_SCORES:
        {
            mzScoresUpdate();
            break;
        }
        case MZ_GAMEOVER:
        {
            mzGameoverUpdate();
            break;
        }
        default:
            break;
    };

    // Handle Drawing Frame (based on the state)
    switch( currState )
    {
        case MZ_TITLE:
        {
            mzTitleDisplay();
            break;
        }
        case MZ_GAME:
        case MZ_AUTO:
        {
            mzGameDisplay();
            break;
        }
        case MZ_SCORES:
        {
            mzScoresDisplay();
            break;
        }
        case MZ_GAMEOVER:
        {
            mzGameoverDisplay();
            break;
        }
        default:
            break;
    };
}

void ICACHE_FLASH_ATTR mzTitleInput(void)
{
    //button a = start game
    if(mzIsButtonPressed(BTN_TITLE_START_GAME))
    {
        mzChangeState(MZ_GAME);
    }
    //button b = go to score screen
    else if(mzIsButtonPressed(BTN_TITLE_START_SCORES))
    {
        mzChangeState(MZ_SCORES);
    }
}

void ICACHE_FLASH_ATTR changeLevel(void)
{
    switch (mazeLevel)
    {
        case BOX_LEVEL:
            mazeLevel = PRACTICE_LEVEL;
            width = 15;
            height = 7;
            rballused = 4;
            break;
        case PRACTICE_LEVEL:
            mazeLevel = EASY_LEVEL;
            width = 19;
            height = 7;
            rballused = 3;
            break;
        case EASY_LEVEL:
            mazeLevel = MIDDLE_LEVEL;
            width = 23;
            height = 11;
            rballused = 3;
            break;
        case MIDDLE_LEVEL:
            mazeLevel = HARD_LEVEL;
            width = 31;
            height = 15;
            rballused = 3;
            break;
        case HARD_LEVEL:
            mazeLevel = KILLER_LEVEL;
            width = 39;
            height = 19;
            rballused = 2;
            break;
        case KILLER_LEVEL:
            mazeLevel = IMPOSSIBLE_LEVEL;
            width = 63;
            height = 27;
            rballused = 1;
            break;
        case IMPOSSIBLE_LEVEL:
            mazeLevel = BOX_LEVEL;
            width = 7;
            height = 3;
            rballused = 9;
            break;
        default:
            break;
    }
}
void ICACHE_FLASH_ATTR mzGameInput(void)
{
    //button b = abort and restart at same level
    if(mzIsButtonPressed(BTN_GAME_RIGHT))
    {
        mazeFreeMemory();
        mzNewMazeSetUp();
        mzChangeState(MZ_GAME);
    }
    //button a = abort and automatically do maze
    else if(mzIsButtonPressed(BTN_GAME_LEFT))
    {
        mzChangeState(MZ_AUTO);
    }
}

void ICACHE_FLASH_ATTR mzScoresInput(void)
{
    //button a = hold to clear scores.
    if(holdingClearScore && mzIsButtonDown(BTN_SCORES_CLEAR_SCORES))
    {
        clearScoreTimer += deltaTime;
        if (clearScoreTimer >= CLEAR_SCORES_HOLD_TIME)
        {
            clearScoreTimer = 0;
            memset(highScores, 0, NUM_MZ_HIGH_SCORES * sizeof(uint32_t));
            saveHighScores();
            loadHighScores();
            mzSetLastScore(0);
        }
    }
    else if(mzIsButtonUp(BTN_SCORES_CLEAR_SCORES))
    {
        clearScoreTimer = 0;
    }
    // This is added to prevent people holding left from the previous screen from accidentally clearing their scores.
    else if(mzIsButtonPressed(BTN_SCORES_CLEAR_SCORES))
    {
        holdingClearScore = true;
    }

    //button b = go to title screen
    if(mzIsButtonPressed(BTN_SCORES_START_TITLE))
    {
        mzChangeState(MZ_TITLE);
    }
}

void ICACHE_FLASH_ATTR mzGameoverInput(void)
{
    //button a = start game
    if(mzIsButtonPressed(BTN_GAMEOVER_START_GAME))
    {
        mazeFreeMemory();
        mzNewMazeSetUp();
        mzChangeState(MZ_TITLE);
    }
    //button b = go to title screen
    else if(mzIsButtonPressed(BTN_GAMEOVER_START_TITLE))
    {
        changeLevel();
        mazeFreeMemory();
        mzNewMazeSetUp();
        mzChangeState(MZ_TITLE);
    }
}

void ICACHE_FLASH_ATTR mzTitleUpdate(void)
{
}

void ICACHE_FLASH_ATTR mzGameUpdate(void)
{
    float param[2];
    bool gonethruany;
    static struct maxtime_t maze_updatedisplay_timer = { .name = "maze_updateDisplay"};


    maxTimeBegin(&maze_updatedisplay_timer);
    //#define USE_SMOOTHED_ACCEL
#ifdef USE_SMOOTHED_ACCEL
    // Smooth accelerometer readings
    xAccel = 0.9 * xAccel + 0.1 * mzAccel.x;
    yAccel = 0.9 * yAccel + 0.1 * mzAccel.y;
    zAccel = 0.9 * zAccel + 0.1 * mzAccel.z;
#else
    xAccel = mzAccel.x;
    yAccel = mzAccel.y;
    zAccel = mzAccel.z;
#endif



#define GETVELOCITY
#ifdef GETVELOCITY
    // (Smoothed) Accelerometer determines velocity and does one euler step with dt = 1000.0 / UPDATE_TIME_MS
    const float dt = (float)UPDATE_TIME_MS / MS_TO_S_FACTOR;
    // Note smoothed accelerometer causes inertia making bit harder to control
    scxc = scxcprev + dt * xAccel;
    scyc = scycprev + dt * yAccel;
#else
    // (Smoothed) accelerometer determines position on screen
    // NOTE if not smoothed very rough motions
    // want -63 to 63 to go approx from 0 to 124 for scxc and 60 to 0 for scyc
    scxc = xAccel + 62; //xAccel/63 * 62 + 62
    scyc = yAccel / 2 + 30; //yAccel/63  + 30
#endif


    // increment count and don't start moving till becomes zero
    totalcyclestilldone++;

    if (totalcyclestilldone < 0)
    {
        scxc = scxcprev;
        scyc = scycprev;
    }


    /**************************************
    //Keep ball within maze boundaries.
    **************************************/

    maze_printf("ENTRY\n from (%d, %d) to (%d, %d)\n", (int)(100 * scxcprev), (int)(100 * scycprev), (int)(100 * scxc),
                (int)(100 * scyc));

    // Take at most two passes to find first hit of wall and adjust if modified directions hits second time
    int16_t iused = -1;
    int16_t imin = -1;
    bool hitwall = false;

    for (uint8_t k = 0; k < 2; k++)
    {
        float b_prev[2] = {scxcprev, scycprev};
        float b_now[2] = {scxc, scyc};
        float b_nowadjusteduse[2] = {scxc, scyc};
        float closestintersection[] = {0, 0};
        gonethruany = false;
        float smin = 2;
        for (int16_t i = 0; i < numwalls; i++)
        {
            if (i == iused)
            {
                continue;
            }

            float p_1[2] = {extendedScaledWallXleft[i], extendedScaledWallYbot[i]};
            float p_2[2] = {extendedScaledWallXright[i], extendedScaledWallYtop[i]};
            float b_nowadjusted[2];
            if ( gonethru(b_prev, b_now, p_1, p_2, rballused, b_nowadjusted, param) )
            {
                gonethruany = true;
                hitwall = true;

                //DEBUG
                maze_printf(" Pass %d Wall %d: (%d, %d) to (%d, %d)\n from prev:(%d, %d) to now: (%d, %d)\n t=%d, s=%d, new now:(%d, %d)\n",
                            k, i,  (int)(100 * p_1[0]), (int)(100 * p_1[1]), (int)(100 * p_2[0]), (int)(100 * p_2[1]),
                            (int)(100 * b_prev[0]), (int)(100 * b_prev[1]), (int)(100 * b_now[0]), (int)(100 * b_now[1]),
                            (int)(100 * param[0]), (int)(100 * param[1]), (int)(100 * b_nowadjusted[0]), (int)(100 * b_nowadjusted[1]));
#ifdef MAZE_DEBUG_PRINT
                int xmeet1, xmeet2, ymeet1, ymeet2;
                xmeet1 = 100 * (b_prev[0] + param[1] * (b_now[0] - b_prev[0]));
                ymeet1 = 100 * (b_prev[1] + param[1] * (b_now[1] - b_prev[1]));
                xmeet2 = 100 * (p_1[0] + param[0] * (p_2[0] - p_1[0]));
                ymeet2 = 100 * (p_1[1] + param[0] * (p_2[1] - p_1[1]));
                maze_printf("on motion vector meet (%d, %d) =? (%d, %d) on boundary\n\n", xmeet1, ymeet1, xmeet2, ymeet2);
#endif
                if (param[1] < smin)
                {
                    smin = param[1];
                    b_nowadjusteduse[0] = b_nowadjusted[0];
                    b_nowadjusteduse[1] = b_nowadjusted[1];
                    closestintersection[0] = b_prev[0] + param[1] * (b_now[0] - b_prev[0]);
                    closestintersection[1] = b_prev[1] + param[1] * (b_now[1] - b_prev[1]);
                    imin = i;
                }
            }
        } // end loop in i going thru walls checking to find closest intersection
        iused = imin;
        if (gonethruany)
        {
            //if (k==1)
            if (true)
            {
                scxc = b_nowadjusteduse[0];
                scyc = b_nowadjusteduse[1];
                scxcprev = closestintersection[0];
                scycprev = closestintersection[1];
            }
            else
            {
                scxc = closestintersection[0];
                scyc = closestintersection[1];
            }
        }
        else
        {
            break;
        }
        maze_printf("pass %d end at (%d, %d)\n", k, (int)(100 * scxc), (int)(100 * scyc));
        maze_printf("    closest intersection at wall %d with s = %d at (%d, %d)\n", iused, (int)(100 * smin),
                    (int)(100 * closestintersection[0]), (int)(100 * closestintersection[1]));

    } // end two try loop

    // balls new coordinates scxc, scyc were adjusted if gonethruany is true
    // can keep a score of time to complete totalcyclestilldone
    // and time in contact with walls, totalhitstilldone
    // can flash LED when touch

    // Wall and Corner Hit Counts and Indicators
    if (hitwall)
    {
        leds[LED_UPPER_MID].g = 127;
        leds[LED_UPPER_MID].r = 127;
        totalhitstilldone++;
    }
    else
    {
        leds[LED_UPPER_MID].g = 0;
        leds[LED_UPPER_MID].r = 0;
    }

    if (gonethruany) //hit corner!
    {
        leds[LED_LOWER_MID].g = 32;
        leds[LED_LOWER_MID].r = 90;
    }
    else
    {
        leds[LED_LOWER_MID].g = 0;
        leds[LED_LOWER_MID].r = 0;
    }


    // force values within outer wall
    // boundary walls should handle this
    // Get rare teleportation
    // but don't seem to always do so this hack
    // with TELEPORT FIX dont need this hack
    /* DEBUG try to observe telportation
        scxc = min(scxc, scxcexit);
        scxc = max(scxc, rballused);
        scyc = min(scyc, scycexit);
        scyc = max(scyc, rballused);
    */
    // Update previous location for next cycle

    scxcprev = scxc;
    scycprev = scyc;
    // Show green LED in corner heading towards
    leds[ledExitInd[exitInd]].g = 127;

    // Test if at exit (bottom right corner) thus finished
    if ((round(scxc) == round(scxcexits[exitInd])) && (round(scyc) == round(scycexits[exitInd])))
    {
        leds[ledExitInd[exitInd]].r = 255;
        leds[ledExitInd[exitInd]].g = 0;
        exitInd += 1;
        if (exitInd > UPPER_RIGHT)
        {
            // Compute score
            // Best performance is fast but not rolling along walls
            // So time rolling is totalhitstilldone, while time
            // moving in corridor without touching is totalcyclestilldone - totalhitstilldone
            // Adjusted time is (totalcyclestilldone - totalhitstilldone) + penaltyFactor * totalhitstilldone
            //   = totalcyclestilldone + (penaltyFactor - 1) * totalhitstilldone
            // wiggleroom + 1 is used for penaltyFactor (more room to wiggle the greater the factor)
            // score proportional to square of number of walls and inversely proportional adjusted time
            float totalTime = UPDATE_TIME_MS  * (float)totalcyclestilldone / MS_TO_S_FACTOR;
            float rollingTime = UPDATE_TIME_MS  * (float)totalhitstilldone / MS_TO_S_FACTOR;
            float incorridorTime = totalTime - rollingTime;
            float adjustedTime = incorridorTime + wiggleroom * rollingTime;
            os_printf("Time to complete maze %d, in corridor %d on walls %d adj %d \n", (int)(100 * totalTime),
                      (int)(100 * incorridorTime), (int)(100 * rollingTime), (int)(100 * adjustedTime));

            score = 100.0 * numwallstodraw * numwallstodraw / adjustedTime;
            os_printf("Score %d, Cycles to complete maze %d, time on walls %d\n", score, totalcyclestilldone, totalhitstilldone);
            gameover = true;
            // Clear Wall and Corner Hit Indicators
            leds[LED_UPPER_MID].g = 0;
            leds[LED_UPPER_MID].r = 0;
            leds[LED_LOWER_MID].g = 0;
            leds[LED_LOWER_MID].r = 0;

        }
    }

    maxTimeEnd(&maze_updatedisplay_timer);
}

void ICACHE_FLASH_ATTR mzAutoGameUpdate(void)
{
    //bool gonethruany;

    scxc = xsol[indSolutionStep];
    scyc = ysol[indSolutionStep];


    // increment count and don't start moving till becomes zero
    totalcyclestilldone++;

    if (totalcyclestilldone < 0)
    {
        scxc = scxcprev;
        scyc = scycprev;
    }
    else
    {
        scxc = xsol[indSolutionStep];
        scyc = ysol[indSolutionStep];
        indSolutionStep++;
        if (indSolutionStep > indSolution)
        {
            gameover = true;
        }
    }

    maze_printf("ENTRY\n from (%d, %d) to (%d, %d)\n", (int)(100 * scxcprev), (int)(100 * scycprev), (int)(100 * scxc),
                (int)(100 * scyc));

    // Update previous location for next cycle

    scxcprev = scxc;
    scycprev = scyc;
    // Show green LED in corner heading towards
    leds[ledExitInd[exitInd]].g = 127;

    // Test if at exit (bottom right corner) thus finished
    if ((round(scxc) == round(scxcexits[exitInd])) && (round(scyc) == round(scycexits[exitInd])))
    {
        leds[ledExitInd[exitInd]].r = 255;
        leds[ledExitInd[exitInd]].g = 0;
        exitInd += 1;
        if (exitInd > UPPER_RIGHT)
        {
            // Compute score
            // Best performance is fast but not rolling along walls
            // So time rolling is totalhitstilldone, while time
            // moving in corridor without touching is totalcyclestilldone - totalhitstilldone
            // Adjusted time is (totalcyclestilldone - totalhitstilldone) + penaltyFactor * totalhitstilldone
            //   = totalcyclestilldone + (penaltyFactor - 1) * totalhitstilldone
            // wiggleroom + 1 is used for penaltyFactor (more room to wiggle the greater the factor)
            // score proportional to square of number of walls and inversely proportional adjusted time
            float totalTime = UPDATE_TIME_MS  * (float)totalcyclestilldone / MS_TO_S_FACTOR;
            float rollingTime = 0; //UPDATE_TIME_MS  * (float)totalhitstilldone / MS_TO_S_FACTOR;
            float incorridorTime = totalTime - rollingTime;
            float adjustedTime = incorridorTime + wiggleroom * rollingTime;
            os_printf("Time to auto complete maze %d, in corridor %d on walls %d adj %d \n", (int)(100 * totalTime),
                      (int)(100 * incorridorTime), (int)(100 * rollingTime), (int)(100 * adjustedTime));

            score = 100.0 * numwallstodraw * numwallstodraw / adjustedTime;
            os_printf("Score %d, Cycles to complete maze %d, time on walls %d\n", score, totalcyclestilldone, totalhitstilldone);
            gameover = true;
            // Clear Wall and Corner Hit Indicators
            leds[LED_UPPER_MID].g = 0;
            leds[LED_UPPER_MID].r = 0;
            leds[LED_LOWER_MID].g = 0;
            leds[LED_LOWER_MID].r = 0;

        }
    }
}


void ICACHE_FLASH_ATTR mzScoresUpdate(void)
{
    // Do nothing.
}

void ICACHE_FLASH_ATTR mzGameoverUpdate(void)
{
}

void ICACHE_FLASH_ATTR mzTitleDisplay(void)
{
    // Clear the display.
    clearDisplay();

    // MAG MAZE
    plotText(20, 5, "MAG MAZE", RADIOSTARS, WHITE);

    plotCenteredText(0, OLED_HEIGHT / 2, 127, (char*)levelName[mazeLevel], IBM_VGA_8, WHITE);

    // SCORES   START
    plotText(0, OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)), "SCORES", IBM_VGA_8, WHITE);
    plotText(OLED_WIDTH - getTextWidth("START", IBM_VGA_8), OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)), "START",
             IBM_VGA_8, WHITE);


}

void ICACHE_FLASH_ATTR mzGameDisplay(void)
{
    char uiStr[32] = {0};
    // Clear the display
    clearDisplay();

    // Draw all walls of maze adjusted to be centered on screen
    for (int16_t i = 0; i < numwallstodraw; i++)
    {
        plotLine(mazescalex * xleft[i] + xadj, mazescaley * ybot[i] + yadj, mazescalex * xright[i] + xadj,
                 mazescaley * ytop[i] + yadj, WHITE);
    }


    // Show all exits
    for (uint8_t i = 0; i < 4; i++)
    {
        plotCircle(round(scxcexits[i]) + xadj, round(scycexits[i]) + yadj, 2, WHITE);
    }
    // Blink exit currently heading for
    if (flashcount >= flashmax / 2)
    {
        plotCircle(round(scxcexits[exitInd]) + xadj, round(scycexits[exitInd]) + yadj, 2, INVERSE);
    }


    // Draw the ball ajusted to fit in maze centered on screen
    // And just touch boundaries

    for (uint8_t rad = 2; rad < (int)rballused; rad++)
    {
        plotCircle(round(scxc) + xadj, round(scyc) + yadj, rad, WHITE);
    }
    // adds flashing box around center of ball
    //(for IMPOSSIBLE_LEVEL the ball is just one pixel, so this helps find it)

    if (rballused > 0)
    {
        if (mazeLevel >= KILLER_LEVEL)
        {
            if (flashcount < flashmax / 2)
            {
                plotRect(round(scxc) + xadj - 1, round(scyc) + yadj - 1, round(scxc) + xadj + 1, round(scyc) + yadj + 1, WHITE);
            }
        }
        else
        {
            if (flashcount >= flashmax / 2)
            {
                plotRect(round(scxc) + xadj - 1, round(scyc) + yadj - 1, round(scxc) + xadj + 1, round(scyc) + yadj + 1, WHITE);
            }
        }
    }
    if (flashcount < flashmax / 2)
    {
        plotCircle(round(scxc) + xadj, round(scyc) + yadj, 0, WHITE);
    }
    flashcount++;
    if (flashcount > flashmax)
    {
        flashcount = 0;
    }

    setmazeLeds(leds, sizeof(leds));

    newHighScore = score > highScores[0];
    //plotCenteredText(0, 0, 10, newHighScore ? "HIGH (NEW!)" : "HIGH", TOM_THUMB, WHITE);
    plotText(0, 59, "AUTO", TOM_THUMB, WHITE);
    plotText(OLED_WIDTH - getTextWidth("RESTART", TOM_THUMB), 59, "RESTART", TOM_THUMB, WHITE);
    ets_snprintf(uiStr, sizeof(uiStr), "%d secs", totalcyclestilldone * UPDATE_TIME_MS / MS_TO_S_FACTOR);
    plotCenteredText(0, 59, OLED_WIDTH, uiStr, TOM_THUMB, WHITE);


    if (gameover)
    {
        mzChangeState(MZ_GAMEOVER);
    }
}

void ICACHE_FLASH_ATTR mzScoresDisplay(void)
{
    // Clear the display
    clearDisplay();

    plotCenteredText(0, 0, OLED_WIDTH, "HIGH SCORES", IBM_VGA_8, WHITE);

    char uiStr[32] = {0};
    // 1. 99999
    ets_snprintf(uiStr, sizeof(uiStr), "1. %d", highScores[0]);
    plotCenteredText(0, (3 * FONT_HEIGHT_TOMTHUMB) + 1, OLED_WIDTH, uiStr, TOM_THUMB, WHITE);

    // 2. 99999
    ets_snprintf(uiStr, sizeof(uiStr), "2. %d", highScores[1]);
    plotCenteredText(0, (5 * FONT_HEIGHT_TOMTHUMB) + 1, OLED_WIDTH, uiStr, TOM_THUMB, WHITE);

    // 3. 99999
    ets_snprintf(uiStr, sizeof(uiStr), "3. %d", highScores[2]);
    plotCenteredText(0, (7 * FONT_HEIGHT_TOMTHUMB) + 1, OLED_WIDTH, uiStr, TOM_THUMB, WHITE);

    // YOUR LAST SCORE:
    ets_snprintf(uiStr, sizeof(uiStr), "YOUR LAST SCORE: %d", mzGetLastScore());
    plotCenteredText(0, (9 * FONT_HEIGHT_TOMTHUMB) + 1, OLED_WIDTH, uiStr, TOM_THUMB, WHITE);


    //TODO: explicitly add a hold to the text, or is the inverse effect enough.
    // (HOLD) CLEAR SCORES      TITLE
    plotText(1, OLED_HEIGHT - (1 * (FONT_HEIGHT_TOMTHUMB + 1)), "CLEAR SCORES", TOM_THUMB, WHITE);

    // fill the clear scores area depending on how long the button's held down.
    if (clearScoreTimer != 0)
    {
        double holdProgress = ((double)clearScoreTimer / (double)CLEAR_SCORES_HOLD_TIME);
        uint8_t holdFill = (uint8_t)(holdProgress * (getTextWidth("CLEAR SCORES", TOM_THUMB) + 2));
        fillDisplayArea(0, (OLED_HEIGHT - (1 * (FONT_HEIGHT_TOMTHUMB + 1))) - 1, holdFill, OLED_HEIGHT, INVERSE);
    }

    plotText(OLED_WIDTH - getTextWidth("TITLE", TOM_THUMB) - 1, OLED_HEIGHT - (1 * (FONT_HEIGHT_TOMTHUMB + 1)), "TITLE",
             TOM_THUMB, WHITE);
}

void ICACHE_FLASH_ATTR mzGameoverDisplay(void)
{
    switch (mazeLevel)
    {
        case BOX_LEVEL:
            danceTimerMode1(NULL);
            break;
        case PRACTICE_LEVEL:
            danceTimerMode2(NULL);
            break;
        case EASY_LEVEL:
            danceTimerMode3(NULL);
            break;
        case MIDDLE_LEVEL:
            danceTimerMode4(NULL);
            break;
        case HARD_LEVEL:
            danceTimerMode13(NULL);
            break;
        case KILLER_LEVEL:
            danceTimerMode16(NULL);
            break;
        case IMPOSSIBLE_LEVEL:
            danceTimerMode17(NULL);
            break;
        default:
            break;
    }
    //random_dance_mode(); //didn't work



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
    plotCenteredText(windowXMargin, windowYMarginTop + titleTextYOffset, OLED_WIDTH - windowXMargin, "GAME OVER", IBM_VGA_8,
                     WHITE);

    // HIGH SCORE! or YOUR SCORE:
    if (newHighScore)
    {
        plotCenteredText(windowXMargin, windowYMarginTop + highScoreTextYOffset, OLED_WIDTH - windowXMargin, "HIGH SCORE!",
                         TOM_THUMB, WHITE);
    }
    else
    {
        plotCenteredText(windowXMargin, windowYMarginTop + highScoreTextYOffset, OLED_WIDTH - windowXMargin, "YOUR SCORE:",
                         TOM_THUMB, WHITE);
    }

    // 1230495
    char scoreStr[32] = {0};
    ets_snprintf(scoreStr, sizeof(scoreStr), "%d", score);
    plotCenteredText(windowXMargin, windowYMarginTop + scoreTextYOffset, OLED_WIDTH - windowXMargin, scoreStr, IBM_VGA_8,
                     WHITE);

    // TITLE    RESTART
    plotText(windowXMargin + controlTextXPadding, controlTextYOffset, "NEW LEVEL", TOM_THUMB, WHITE);
    plotText(OLED_WIDTH - windowXMargin - getTextWidth("SAME LEVEL", TOM_THUMB) - controlTextXPadding, controlTextYOffset,
             "SAME LEVEL", TOM_THUMB, WHITE);
}

// helper functions.

/**
 * @brief Initializer for maze, allocates memory for work arrays
 *
 * @param
 * @return pointers to the work array???
 */
void ICACHE_FLASH_ATTR mzNewMazeSetUp(void)
{
    //Allocate some working array memory now
    //TODO is memory being freed up appropriately?
    xleft = (uint8_t*)malloc (sizeof (uint8_t) * MAXNUMWALLS);
    xright = (uint8_t*)malloc (sizeof (uint8_t) * MAXNUMWALLS);
    ytop = (uint8_t*)malloc (sizeof (uint8_t) * MAXNUMWALLS);
    ybot = (uint8_t*)malloc (sizeof (uint8_t) * MAXNUMWALLS);
    xsol = (uint8_t*)malloc (sizeof (uint8_t) * width * (height + 1) / 2);
    ysol = (uint8_t*)malloc (sizeof (uint8_t) * width * (height + 1) / 2);
    int16_t i;
    int16_t startvert = 0;
    get_maze_output_t out;

    totalcyclestilldone = -4 * MS_TO_S_FACTOR / UPDATE_TIME_MS;
    totalhitstilldone = 0;
    gameover = false;
    memset(leds, 0, sizeof(leds));


    system_print_meminfo();
    os_printf("Free Heap %d\n", system_get_free_heap_size());

    mazescalex = 127 / width;
    mazescaley = 63 / height;

    // This is the biggest rballused could be and then the ball will completely fill some corridors
    //    so for such a maze, it would be impossible to avoid touch walls.
    // Compute number of maximum pixels between ball and wall.
    wiggleroom = 2 * (min(mazescalex, mazescaley) - rballused);

    os_printf("width:%d, height:%d mscx:%d mscy:%d rball:%d wiggleroom %d\n", width, height, mazescalex, mazescaley,
              (int)rballused, (int)wiggleroom);


    // initial position in center
    scxcprev = (float)mazescalex * (width - 1) / 2.0;
    scycprev = (float)mazescaley * (height - 1) / 2.0;

    // set these accelerometer readings consistant with starting at scxcprev and scycprev
    // ONLY needed if smoothing used and depends on using position or velocity
    //xAccel = rballused - 64;
    //yAccel = -2 * (rballused - 30);

    // TODO will require touch each corner
    // exit is bottom right corner

    exitInd = UPPER_LEFT;
    for (exitSpot_t ix = 0; ix < 4; ix++)
    {
        exitHit[ix] = false;
    }

    // compute exits
    scxcexits[UPPER_LEFT] = rballused;
    scycexits[UPPER_LEFT] = rballused;
    scxcexits[LOWER_LEFT]  = rballused;
    scycexits[LOWER_LEFT] = mazescaley * (height - 1) - rballused;
    scxcexits[LOWER_RIGHT] = mazescalex * (width - 1) - rballused;
    scycexits[LOWER_RIGHT] = mazescaley * (height - 1) - rballused;
    scxcexits[UPPER_RIGHT] = mazescalex * (width - 1) - rballused;
    scycexits[UPPER_RIGHT] = rballused;

    xadj = 0.5 + (127 - scxcexits[LOWER_RIGHT] - rballused) / 2;
    //yadj = 0.5 + (63 - scycexits[LOWER_RIGHT] - rballused)/2;
    yadj = 0;
    os_printf("initpt (%d, %d)\n", (int)scxcprev, (int)scycprev);
    for (i = 0; i < 4; i++)
    {
        os_printf("exit corner %d (%d, %d)\n", i, (int)scxcexits[i], (int)scycexits[i]);
    }



    // get_maze allocates more memory, makes a random maze giving walls and solution to maze
    // and then deallocates memory
    out = get_maze(width, height, xleft, xright, ybot, ytop, xsol, ysol, scxcexits, scycexits, mazescalex, mazescaley);
    numwalls = out.indwall;
    indSolution = out.indSolution;
    // xleft, xright, ybot, ytop are lists of boundary intervals making maze
    if (numwalls > MAXNUMWALLS)
    {
        os_printf("numwalls = %d exceeds MAXNUMWALLS = %d", numwalls, MAXNUMWALLS);
    }
    numwallstodraw = numwalls;

    // print scaled walls
    for (i = 0; i < numwalls; i++)
    {
        maze_printf("i %d (%d, %d) to (%d, %d)\n", i, mazescalex * xleft[i], mazescaley * ybot[i], mazescalex * xright[i],
                    mazescaley * ytop[i]);
    }

    // print solutions
    os_printf("Solution ________________\n");
    for (i = 0; i < indSolution; i++)
    {
        os_printf("(%d, %d) -> ", xsol[i], ysol[i]);
    }

    //Allocate some more working array memory now
    //TODO is memory being freed up appropriately?

    extendedScaledWallXleft = (float*)malloc (sizeof (float) * MAXNUMWALLS);
    extendedScaledWallXright = (float*)malloc (sizeof (float) * MAXNUMWALLS);
    extendedScaledWallYtop = (float*)malloc (sizeof (float) * MAXNUMWALLS);
    extendedScaledWallYbot = (float*)malloc (sizeof (float) * MAXNUMWALLS);

    os_printf("After Working Arrays allocated Free Heap %d\n", system_get_free_heap_size());
    // extend the scaled walls
    // extend walls by slightlyLessThanOne*rball and compute possible extra stopper walls
    // ONLY for horizontal and vertical walls. Could do for arbitrary but
    // would first need to compute perpendicular vectors and use them.
    // extending by rball I think guarantees no passing thru corners, but also
    // causes sticking above T junctions in maze.
    // NOTE using slightlyLessThanOne*rball prevents sticking but has very small probability
    // to telport at corners extend walls
    // Also if rball is such that it fits tightly in corridors, this also makes it hard to get thru gaps
    //    and could be relaxed to 0.75 maybe
    const float slightlyLessThanOne = 1.0 - 1.0 / 128.; // used 0.99
    for (i = 0; i < numwalls; i++)
    {
        if (mazescaley * ybot[i] == mazescaley * ytop[i]) // horizontal wall
        {
            extendedScaledWallYbot[i] = mazescaley * ybot[i];
            extendedScaledWallYtop[i] = mazescaley * ytop[i];
            extendedScaledWallXleft[i] = mazescalex * xleft[i] - slightlyLessThanOne * rballused;
            extendedScaledWallXright[i] = mazescalex * xright[i] + slightlyLessThanOne * rballused;
        }
        else
        {
            if ((mazescaley * ybot[i] < mazescaley * ytop[i]) && (startvert == 0))
            {
                startvert = i;
            }
            extendedScaledWallXleft[i] = mazescalex * xleft[i];
            extendedScaledWallXright[i] = mazescalex * xright[i];
            extendedScaledWallYbot[i] = mazescaley * ybot[i] - slightlyLessThanOne * rballused;
            extendedScaledWallYtop[i] = mazescaley * ytop[i] + slightlyLessThanOne * rballused;
        }
    }
    int16_t nwi = numwalls; //new wall index starts here
    // A wall that extends into the interior and not joined to another wall needs a small
    // 'stopper' wall to cross it near the end.
    // find and keep only stopper walls that are not contained in any of the extended walls
    maze_printf("startvert = %d\n", startvert);

    for (i = 0; i < numwalls; i++)
    {
        if (mazescaley * ybot[i] == mazescaley * ytop[i]) // horizontal wall
        {
            // possible extra vertical walls crossing either end
            extendedScaledWallXleft[nwi] = mazescalex * xleft[i];
            extendedScaledWallYbot[nwi] = mazescaley * ybot[i] - slightlyLessThanOne * rballused;
            extendedScaledWallXright[nwi] = mazescalex * xleft[i];
            extendedScaledWallYtop[nwi] = mazescaley * ybot[i] + slightlyLessThanOne * rballused;
            nwi = incrementifnewvert(nwi, startvert,
                                     numwalls); //, extendedScaledWallXleft, extendedScaledWallYbot, extendedScaledWallXright, extendedScaledWallYtop);
            extendedScaledWallXleft[nwi] = mazescalex * xright[i];
            extendedScaledWallYbot[nwi] = mazescaley * ybot[i] - slightlyLessThanOne * rballused;
            extendedScaledWallXright[nwi] = mazescalex * xright[i];
            extendedScaledWallYtop[nwi] = mazescaley * ybot[i] + slightlyLessThanOne * rballused;
            nwi = incrementifnewvert(nwi, startvert,
                                     numwalls); //, extendedScaledWallXleft, extendedScaledWallYbot, extendedScaledWallXright, extendedScaledWallYtop);
        }
        else
        {
            // possible extra horizontal walls crossing either end
            extendedScaledWallXleft[nwi] = mazescalex * xleft[i] - slightlyLessThanOne * rballused;
            extendedScaledWallYbot[nwi] = mazescaley * ybot[i];
            extendedScaledWallXright[nwi] = mazescalex * xleft[i] + slightlyLessThanOne * rballused;
            extendedScaledWallYtop[nwi] = mazescaley * ybot[i];
            nwi = incrementifnewhoriz(nwi, 0,
                                      startvert); //, extendedScaledWallXleft, extendedScaledWallYbot, extendedScaledWallXright, extendedScaledWallYtop);
            extendedScaledWallXleft[nwi] = mazescalex * xleft[i] - slightlyLessThanOne * rballused;
            extendedScaledWallYbot[nwi] = mazescaley * ytop[i];
            extendedScaledWallXright[nwi] = mazescalex * xleft[i] + slightlyLessThanOne * rballused;
            extendedScaledWallYtop[nwi] = mazescaley * ytop[i];
            nwi = incrementifnewhoriz(nwi, 0,
                                      startvert); //, extendedScaledWallXleft, extendedScaledWallYbot, extendedScaledWallXright, extendedScaledWallYtop);
        }
    }
    if (nwi > MAXNUMWALLS)
    {
        os_printf("nwi = %d exceeds MAXNUMWALLS = %d", nwi, MAXNUMWALLS);
    }
    maze_printf("orginal numwalls = %d, with stoppers have %d\n", numwalls, nwi);
    // update numwalls
    numwalls = nwi;
}

/**
 * Called when maze is exited or before making new maze
 */
void ICACHE_FLASH_ATTR mazeFreeMemory(void)
{
    free(xleft);
    free(xright);
    free(ytop);
    free(ybot);
    free(xsol);
    free(ysol);
    free(extendedScaledWallXleft);
    free(extendedScaledWallXright);
    free(extendedScaledWallYbot);
    free(extendedScaledWallYtop);
}


void ICACHE_FLASH_ATTR mzChangeState(mazeState_t newState)
{
    prevState = currState;
    currState = newState;
    stateStartTime = system_get_time();
    stateTime = 0;

    switch( currState )
    {
        case MZ_TITLE:
            // Clear leds
            memset(leds, 0, sizeof(leds));
            setmazeLeds(leds, sizeof(leds));
            break;
        case MZ_GAME:
            // All game restart functions happen here.
            loadHighScores();
            // TODO: should I be seeding this, or re-seeding this, and if so, with what?
            srand((uint32_t)(mzAccel.x + mzAccel.y * 3 + mzAccel.z * 5)); // Seed the random number generator.
            break;
        case MZ_AUTO:
            loadHighScores();
            indSolutionStep = 0;
            totalcyclestilldone = 0;
            exitInd = UPPER_LEFT;
            break;
        case MZ_SCORES:
            loadHighScores();
            clearScoreTimer = 0;
            holdingClearScore = false;
            break;
        case MZ_GAMEOVER:
            // Update high score if needed.
            if (prevState != MZ_AUTO)
            {
                newHighScore = updateHighScores(score);
                if (newHighScore)
                {
                    saveHighScores();
                }
                // Save out the last score.
                mzSetLastScore(score);
            }
            break;
        default:
            break;
    };
}

bool ICACHE_FLASH_ATTR mzIsButtonPressed(uint8_t button)
{
    return (mzButtonState & button) && !(mzLastButtonState & button);
}

bool ICACHE_FLASH_ATTR mzIsButtonReleased(uint8_t button)
{
    return !(mzButtonState & button) && (mzLastButtonState & button);
}

bool ICACHE_FLASH_ATTR mzIsButtonDown(uint8_t button)
{
    return mzButtonState & button;
}

bool ICACHE_FLASH_ATTR mzIsButtonUp(uint8_t button)
{
    return !(mzButtonState & button);
}


int16_t ICACHE_FLASH_ATTR  incrementifnewvert(int16_t nwi, int16_t startind, int16_t endind)
{
    // nwi is new vertical wall index
    // increment nwi only if no extended vertical walls contain it.
    for (int16_t i = startind; i < endind; i++)
    {
        if ((extendedScaledWallXright[nwi] == extendedScaledWallXright[i])
                && (extendedScaledWallYbot[i] <= extendedScaledWallYbot[nwi])
                && (extendedScaledWallYtop[i] >= extendedScaledWallYtop[nwi]))
        {
            // found containing extended vertical wall
            return nwi;
        }
    }
    return nwi + 1;
}

int16_t ICACHE_FLASH_ATTR  incrementifnewhoriz(int16_t nwi, int16_t startind, int16_t endind)
{
    // nwi is  new horizontal wall index
    // increment nwi only if no extended horizontal walls contain it.
    for (int16_t i = startind; i < endind; i++)
    {
        if ((extendedScaledWallYtop[nwi] == extendedScaledWallYtop[i])
                && (extendedScaledWallXleft[i] <= extendedScaledWallXleft[nwi])
                && (extendedScaledWallXright[i] >= extendedScaledWallXright[nwi]))
        {
            // found containing extended horizontal wall
            return nwi;
        }
    }
    return nwi + 1;
}

/**
 * Linear Alg Find Intersection of line segments
 */

uint8_t ICACHE_FLASH_ATTR intervalsmeet(float a, float c, float b, float d, float e, float f, float param[])
{
    // given two points p_1, p_2 in the (x,y) plane specifying a line interval from p_1 to p_2
    //.    parameterized by t
    // a moving object which was at b_prev and is currently at b_now
    // returns true if the object crossed the line interval
    // this can also be useful if want the line interval to be a barrier by
    //    reverting to b_prev
    // the column vector [a,c] is p_2 - p_1
    // the column vector [b,d] is b_now - b_prev
    // the column vector [e,f] is vector from p_1 to b_prev =  b_prev - p_1
    // looking for parametric solution to
    // p_1 + t(p_2 - p_1) = b_prev + s(b_now - b_prev) with 0<= t,s <= 1
    // if (a -b)(t)  (e)
    //    (c -d)(s)  (f) has unique solution with 0<= t,s <= 1
    // param = [t, s]
    // returns True

    float det = -a * d + b * c;
    if (det == 0)
    {
        return false;
    }
    float t = (-e * d + f * b) / det; // t is param of interval
    float s = (a * f - c * e) / det; //s is param of interval from b_prev to b_now
    param[0] = t;
    param[1] = s;
    //if ((t < -0.05) || (t > 1.05)) return false;
    //if ((s < -0.05) || (s > 1.05)) return false;
    if ((t < 0) || (t > 1))
    {
        return false;    // wall parameter out of bounds
    }

    //if ((s < 0) || (s > 1)) return false; // cause teleportation
    //TELEPORT FIX
    if ((s < -0.001) || (s > 1))
    {
        return false;    // motion parameter out of bounds
    }
    //maze_printf("t = %d, s = %d\n", (int8_t) (100*t), (int8_t) (100*s));
    if (s < 0)
    {
        os_printf("very small NEGATIVE motion parameter s = %d/10000 so said goes thru\n", (int)(10000 * s));
    }
    return true;
}

uint8_t ICACHE_FLASH_ATTR  gonethru(float b_prev[], float b_now[], float p_1[], float p_2[], float rball,
                                    float b_nowadjusted[], float param[])
{
    // given two points p_1, p_2 in the (x,y) plane specifying a line interval from p_1 to p_2
    // a moving object (ball of radius rball, or point if rball is None)
    // whos center was at b_prev and is currently at b_now
    // returns true if the balls leading (in direction of travel) boundary crossed the line interval
    // this can also be useful if want the line interval to be a barrier by
    //    reverting to b_prev
    // b_nowadjusted is mutable list which is the point moved back to inside boundary
    float pperp[2];
    // param vector if an intersection {t, s} where t parameterizes from p_1 to p_2 and s b_prev to b_now
    //float param[2];
    uint8_t didgothru;

    b_nowadjusted[0] = b_now[0];
    b_nowadjusted[1] = b_now[1];
    //Find vector perpendicular to the wall
    //TODO could compute ppperp in mazeEnterMode to save time but take more space
    pperp[0] = p_2[1] - p_1[1];
    pperp[1] = p_1[0] - p_2[0];

    float pperplen = sqrt(pperp[0] * pperp[0] + pperp[1] * pperp[1]);

    if (pperplen == 0.0)
    {
        // should never happen here as using walls which all have pos length
        maze_printf("Zero length wall!\n");
        return false;
    }

    pperp[0] = pperp[0] / pperplen; // make unit vector
    pperp[1] = pperp[1] / pperplen; // make unit vector

    // dot product with pperp gives component of motion at right angles to wall
    float testdir = pperp[0] * (b_now[0] - b_prev[0]) + pperp[1] * (b_now[1] - b_prev[1]);
    // Originally had if (testdir == 0.00) return false; // cause of teleportation
    //TELEPORT FIX
    if (fabs(testdir) < 0.001)
    {
        return false;
    }

    //os_printf("%d ", (int)(1000*testdir));

    if (testdir > 0) // > for leading edge , < for trailing edge
    {
        didgothru =  intervalsmeet(p_2[0] - p_1[0], p_2[1] - p_1[1], b_now[0] - b_prev[0], b_now[1] - b_prev[1],
                                   b_prev[0] + rball * pperp[0] - p_1[0], b_prev[1] + rball * pperp[1] - p_1[1], param);
    }
    else
    {
        didgothru = intervalsmeet(p_2[0] - p_1[0], p_2[1] - p_1[1], b_now[0] - b_prev[0], b_now[1] - b_prev[1],
                                  b_prev[0] - rball * pperp[0] - p_1[0], b_prev[1] - rball * pperp[1] - p_1[1], param);
    }

    if (didgothru)
    {
        // Adjust back to previous point ie ignore movement - TOO choppy
        //b_nowadjusted[0] = b_prev[0];
        //b_nowadjusted[1] = b_prev[1];
        // Could adjust to point of contact with interval but still choppy

        // Adjust to roll along interval if would have gone thru
        // gives b_now projected onto line parallel to wall shifted by rball
        b_nowadjusted[0] = b_now[0] + (1.0 - param[1]) * (- testdir * pperp[0]);
        b_nowadjusted[1] = b_now[1] + (1.0 - param[1]) * (- testdir * pperp[1]);

        //This adjustment gives b_now  projected onto line parallel to wall
        //thru b_now
        //May have gone thru (many) other walls
        //Keeps away from wall
        //b_nowadjusted[0] = b_now[0] - testdir * pperp[0];
        //b_nowadjusted[1] = b_now[1] - testdir * pperp[1];




    }
    return didgothru;
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
    uint8_t textWidth = plotText(0, 0, text, font,
                                 INVERSE) - 1; // minus one accounts for the return being where the cursor is.

    // Then we draw the inverse back over it to restore it.
    plotText(0, 0, text, font, INVERSE);
    return textWidth;
}




// Fisher–Yates Shuffle
void mzshuffle(int length, int array[length])
{
    for (int i = length - 1; i > 0; i--)
    {
        // Pick a random index from 0 to i
        int j = rand() % (i + 1);

        // Swap array[i] with the element at random index
        int temp = array[i];
        array[i] = array[j];
        array[j] = temp;
    }
}


static void loadHighScores(void)
{
    memcpy(highScores, mzGetHighScores(),  NUM_MZ_HIGH_SCORES * sizeof(uint32_t));
}

static void saveHighScores(void)
{
    mzSetHighScores(highScores);
}

static bool updateHighScores(uint32_t newScore)
{
    bool highScore = false;
    uint32_t placeScore = newScore;
    for (int i = 0; i < NUM_MZ_HIGH_SCORES; i++)
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
