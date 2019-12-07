/*
*   mode_mazerf.c
*
*   Created on: 21 Sept 2019
*               Author: bbkiw
*       Refactor of maze using Jonathan Moriarty basic set up
*/

// Keeps best times for each level in flash
// Reports adjusted time = time in corridors + 1.5 * time touching walls
// Special Led pattern displays for each level
// Auto maze adjusted time is reported (it is the actual time as it never hits the walls)
//   but does not count for record times and does not show special LED pattern
//TODO
// Add Sound
// Could make better choice of led patterns so hard levels get better patterns
// Could unlock the special patterns for display later, it simply are those
//     levels which have a best time stored

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
#include "buzzer.h"
#include "hpatimer.h"

/*============================================================================
 * Defines
 *==========================================================================*/

//#ifndef max
//    #define max(a,b) ((a) > (b) ? (a) : (b))
//#endif
#ifndef min
    #define min(a,b) ((a) < (b) ? (a) : (b))
#endif

// buttons
#define BTN_LEFT_B LEFT
#define BTN_RIGHT_A RIGHT

// controls (title)
#define BTN_TITLE_CHOOSE_LEVEL BTN_LEFT_B
#define BTN_TITLE_START_GAME BTN_RIGHT_A

// controls (game)
#define BTN_GAME_RESTART BTN_RIGHT_A
#define BTN_GAME_AUTO BTN_LEFT_B

// controls (scores)
#define BTN_SCORES_CLEAR_SCORES BTN_LEFT_B
#define BTN_SCORES_START_TITLE BTN_RIGHT_A

// controls (gameover)
#define BTN_GAMEOVER_BEST_TIMES BTN_LEFT_B
#define BTN_GAMEOVER_TITLE BTN_RIGHT_A

// update task (Richard maxTime has 28 ms for most complicated maze)
#define UPDATE_TIME_MS 50

// time info.
#define MS_TO_US_FACTOR 1000
#define S_TO_MS_FACTOR 1000

#define CLEAR_SCORES_HOLD_TIME (5 * MS_TO_US_FACTOR * S_TO_MS_FACTOR)

// #define NUM_MZ_LEVELS 7
#define PRACTICE_LEVEL 0
#define NOVICE_LEVEL 1
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
uint32_t scoreauto; // The current autoscore this game.
uint32_t mzBestTimes[NUM_MZ_LEVELS];
bool nzNewBestTime;

const song_t checkpointSfx RODATA_ATTR =
{
    .notes = {
        {.note = G_5, .timeMs = 150},
        {.note = C_6, .timeMs = 150},
    },
    .numNotes = 2,
    .shouldLoop = false
};

const song_t winSfx RODATA_ATTR =
{
    .notes = {
        {.note = G_5, .timeMs = 100},
        {.note = A_5, .timeMs = 100},
        {.note = B_5, .timeMs = 100},
        {.note = C_6, .timeMs = 100},
    },
    .numNotes = 4,
    .shouldLoop = false
};

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

void ICACHE_FLASH_ATTR mzGameoverLedDisplay(void);
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
void ICACHE_FLASH_ATTR mzLoadBestTimes(void);
void ICACHE_FLASH_ATTR mzSaveBestTimes(void);
bool ICACHE_FLASH_ATTR mzUpdateBestTimes(uint8_t levelInd,  uint32_t newScore);

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
void ICACHE_FLASH_ATTR setLevel(uint8_t mazeLevel);

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


const char* levelName[NUM_MZ_LEVELS] =
{
    "PRACTICE Level",
    "NOVICE Level",
    "EASY Level",
    "MIDDLE Level",
    "HARD Level",
    "KILLER Level",
    "IMPOSSIBLE Level"
};

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
    .fnAccelerometerCallback = mzAccelerometerCallback,
    .menuImageData = mnu_maze_0,
    .menuImageLen = sizeof(mnu_maze_0)
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
float scxcauto;
float scycauto;
float scxcautonext;
float scycautonext;
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
bool didAutoMaze;

uint8_t mazeLevel = PRACTICE_LEVEL;
uint8_t width;
uint8_t height; //Maze dimensions must be 1 less than multiples of 4
uint8_t mazescalex = 1;
uint8_t mazescaley = 1;
int16_t numwalls;
int16_t indSolution;
int16_t indSolutionStep;
int16_t indSolutionSubStep;
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

bool mazeDrawGalleryUnlock = false;

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
    mazeLevel = getMazeLevel();
    setLevel(mazeLevel);
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
        setMazeLevel(mazeLevel);
        mzChangeState(MZ_GAME);
    }
    //button b = choose a level
    else if(mzIsButtonPressed(BTN_TITLE_CHOOSE_LEVEL))
    {
        mazeLevel++;
        if (mazeLevel > IMPOSSIBLE_LEVEL)
        {
            mazeLevel = PRACTICE_LEVEL;
        }
        setLevel(mazeLevel);
        mazeFreeMemory();
        mzNewMazeSetUp();
    }
}

void ICACHE_FLASH_ATTR setLevel(uint8_t mzLevel)
{
    switch (mzLevel)
    {
        case PRACTICE_LEVEL:
            width = 11;
            height = 7;
            rballused = 4;
            break;
        case NOVICE_LEVEL:
            width = 15;
            height = 7;
            rballused = 4;
            break;
        case EASY_LEVEL:
            width = 19;
            height = 7;
            rballused = 3;
            break;
        case MIDDLE_LEVEL:
            width = 23;
            height = 11;
            rballused = 3;
            break;
        case HARD_LEVEL:
            width = 31;
            height = 15;
            rballused = 3;
            break;
        case KILLER_LEVEL:
            width = 39;
            height = 19;
            rballused = 2;
            break;
        case IMPOSSIBLE_LEVEL:
            width = 63;
            height = 27;
            rballused = 1;
            break;
        default:
            break;
    }
}

void ICACHE_FLASH_ATTR mzGameInput(void)
{
    //button b = abort and restart at same level
    if(mzIsButtonPressed(BTN_GAME_RESTART))
    {
        mazeFreeMemory();
        mzNewMazeSetUp();
        mzChangeState(MZ_GAME);
    }
    //button a = abort and automatically do maze
    else if(mzIsButtonPressed(BTN_GAME_AUTO))
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
            memset(mzBestTimes, 0x0f, NUM_MZ_LEVELS * sizeof(uint32_t));
            mzSaveBestTimes();
            mzLoadBestTimes();
            mzSetLastScore(0x0f0f0f0f);
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
        mazeFreeMemory();
        mzNewMazeSetUp();
        mzChangeState(MZ_TITLE);
    }
}

void ICACHE_FLASH_ATTR mzGameoverInput(void)
{
    //button a = start game
    if(mzIsButtonPressed(BTN_GAMEOVER_TITLE))
    {
        mazeFreeMemory();
        mzNewMazeSetUp();
        mzChangeState(MZ_TITLE);
    }
    //button b = go to title screen
    else if(mzIsButtonPressed(BTN_GAMEOVER_BEST_TIMES))
    {
        mzChangeState(MZ_SCORES);
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
    // (Smoothed) Accelerometer determines velocity and does one euler step with dt
    const float dt = (float)UPDATE_TIME_MS / S_TO_MS_FACTOR;
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
        if(exitInd <= UPPER_RIGHT)
        {
            startBuzzerSong(&checkpointSfx);
        }
        else
        {
            startBuzzerSong(&winSfx);
            // Compute score
            // Best performance is fast but not rolling along walls
            // So time rolling is totalhitstilldone, while time
            // moving in corridor without touching is totalcyclestilldone - totalhitstilldone
            // Adjusted time is (totalcyclestilldone - totalhitstilldone) + penaltyFactor * totalhitstilldone
            //   = totalcyclestilldone + (penaltyFactor - 1) * totalhitstilldone
            // wiggleroom + 1 is used for penaltyFactor (more room to wiggle the greater the factor)
            // score proportional to square of number of walls and inversely proportional adjusted time
            float totalTime = UPDATE_TIME_MS  * (float)totalcyclestilldone / S_TO_MS_FACTOR;
            float rollingTime = UPDATE_TIME_MS  * (float)totalhitstilldone / S_TO_MS_FACTOR;
            float incorridorTime = totalTime - rollingTime;
            // Too severe a penalty
            //float adjustedTime = incorridorTime + wiggleroom * rollingTime;
            //here use 50 %
            float adjustedTime = incorridorTime + 1.5 * rollingTime;
            maze_printf("Time to complete maze %d, in corridor %d on walls %d adj %d ratio %d \n", (int)(100 * totalTime),
                        (int)(100 * incorridorTime), (int)(100 * rollingTime), (int)(100 * adjustedTime),
                        (int)(100 * adjustedTime / indSolution));

            // could scale by length of solution
            //score = 10000 * adjustedTime / indSolution;
            score = adjustedTime;

            gameover = true;
            didAutoMaze = false;
            // Clear Wall and Corner Hit Indicators
            leds[LED_UPPER_MID].g = 0;
            leds[LED_UPPER_MID].r = 0;
            leds[LED_LOWER_MID].g = 0;
            leds[LED_LOWER_MID].r = 0;

        }
    }

    maxTimeEnd(&maze_updatedisplay_timer);
}

#define NUM_SUB_STEPS 5
void ICACHE_FLASH_ATTR mzAutoGameUpdate(void)
{
    // increment count
    totalcyclestilldone++;

    if (indSolutionSubStep == 0)
    {
        scxcauto = xsol[indSolutionStep];
        scycauto = ysol[indSolutionStep];

        indSolutionStep++;
        if (indSolutionStep < indSolution)
        {
            scxcautonext = xsol[indSolutionStep];
            scycautonext = ysol[indSolutionStep];
        }
    }

    scxc = scxcauto + indSolutionSubStep * (scxcautonext - scxcauto) / NUM_SUB_STEPS;
    scyc = scycauto + indSolutionSubStep * (scycautonext - scycauto) / NUM_SUB_STEPS;

    indSolutionSubStep++;
    indSolutionSubStep %= NUM_SUB_STEPS;

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
            float totalTime = UPDATE_TIME_MS  * (float)totalcyclestilldone / S_TO_MS_FACTOR;
            float rollingTime = UPDATE_TIME_MS  * (float)totalhitstilldone / S_TO_MS_FACTOR;
            float incorridorTime = totalTime - rollingTime;
            float adjustedTime = incorridorTime + 1.5 * rollingTime;
            maze_printf("Auto Time to complete maze %d, in corridor %d on walls %d adj %d ratio %d \n", (int)(100 * totalTime),
                        (int)(100 * incorridorTime), (int)(100 * rollingTime), (int)(100 * adjustedTime),
                        (int)(100 * adjustedTime / indSolution));
            scoreauto = adjustedTime;
            // Auto game does not get counted for best times!
            score = 0x0f0f0f0f;
            gameover = true;
            didAutoMaze = true;
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

    // Title
    plotCenteredText(0, 5, OLED_WIDTH, "Maze", RADIOSTARS, WHITE);

    // Level
    plotCenteredText(0, OLED_HEIGHT / 2, 127, (char*)levelName[mazeLevel], IBM_VGA_8, WHITE);

    // Button labels
    plotText(0,
             OLED_HEIGHT - (1 * (FONT_HEIGHT_TOMTHUMB + 1)),
             "LEVEL",
             TOM_THUMB, WHITE);
    plotText(OLED_WIDTH - 20,
             OLED_HEIGHT - (1 * (FONT_HEIGHT_TOMTHUMB + 1)),
             "START",
             TOM_THUMB, WHITE);
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
            plotRect(round(scxc) + xadj - 1, round(scyc) + yadj - 1, round(scxc) + xadj + 1, round(scyc) + yadj + 1, WHITE);
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

    nzNewBestTime = score > mzBestTimes[0];
    //plotCenteredText(0, 0, 10, nzNewBestTime ? "HIGH (NEW!)" : "HIGH", TOM_THUMB, WHITE);
    plotText(0, 59, "AUTO", TOM_THUMB, WHITE);
    plotText(OLED_WIDTH - getTextWidth("RESTART", TOM_THUMB), 59, "RESTART", TOM_THUMB, WHITE);
    ets_snprintf(uiStr, sizeof(uiStr), "%d secs", totalcyclestilldone * UPDATE_TIME_MS / S_TO_MS_FACTOR);
    plotCenteredText(0, 59, OLED_WIDTH, uiStr, TOM_THUMB, WHITE);


    if (gameover)
    {
        mzChangeState(MZ_GAMEOVER);
    }
}

void ICACHE_FLASH_ATTR mzScoresDisplay(void)
{
    if (! didAutoMaze)
    {
        mzGameoverLedDisplay();
    }
    // Clear the display
    clearDisplay();

    plotCenteredText(0, 0, OLED_WIDTH, "BEST ADJUSTED TIMES", TOM_THUMB, WHITE);

    char uiStr[32] = {0};
    // 1. 99999
    if (mzBestTimes[0] < 100000)
    {
        ets_snprintf(uiStr, sizeof(uiStr), "P %d", mzBestTimes[0]);
    }
    else
    {
        ets_snprintf(uiStr, sizeof(uiStr), "P -----");
    }
    plotText(0, (2 * FONT_HEIGHT_TOMTHUMB) - 3, uiStr, IBM_VGA_8, WHITE);

    // 2. 99999
    if (mzBestTimes[1] < 100000)
    {
        ets_snprintf(uiStr, sizeof(uiStr), "N %d", mzBestTimes[1]);
    }
    else
    {
        ets_snprintf(uiStr, sizeof(uiStr), "N -----");
    }
    plotText(0, (2 * FONT_HEIGHT_TOMTHUMB) + (FONT_HEIGHT_IBMVGA8) - 2, uiStr, IBM_VGA_8, WHITE);

    // 3. 99999
    if (mzBestTimes[2] < 100000)
    {
        ets_snprintf(uiStr, sizeof(uiStr), "E %d", mzBestTimes[2]);
    }
    else
    {
        ets_snprintf(uiStr, sizeof(uiStr), "E -----");
    }
    plotText(0, (2 * FONT_HEIGHT_TOMTHUMB) + 2 * (FONT_HEIGHT_IBMVGA8), uiStr, IBM_VGA_8, WHITE);

    // 4. 99999
    if (mzBestTimes[3] < 100000)
    {
        ets_snprintf(uiStr, sizeof(uiStr), "M %d", mzBestTimes[3]);
    }
    else
    {
        ets_snprintf(uiStr, sizeof(uiStr), "M -----");
    }
    plotText(0, (2 * FONT_HEIGHT_TOMTHUMB) + 3 * (FONT_HEIGHT_IBMVGA8) + 2, uiStr, IBM_VGA_8, WHITE);

    // 5. 99999
    if (mzBestTimes[4] < 100000)
    {
        ets_snprintf(uiStr, sizeof(uiStr), "H %d", mzBestTimes[4]);
    }
    else
    {
        ets_snprintf(uiStr, sizeof(uiStr), "H -----");
    }
    plotText(64, (2 * FONT_HEIGHT_TOMTHUMB) - 3, uiStr, IBM_VGA_8, WHITE);

    // 6. 99999
    if (mzBestTimes[5] < 100000)
    {
        ets_snprintf(uiStr, sizeof(uiStr), "K %d", mzBestTimes[5]);
    }
    else
    {
        ets_snprintf(uiStr, sizeof(uiStr), "K -----");
    }
    plotText(64, (2 * FONT_HEIGHT_TOMTHUMB) + (FONT_HEIGHT_IBMVGA8) - 2, uiStr, IBM_VGA_8, WHITE);

    // 7. 99999
    if (mzBestTimes[6] < 100000)
    {
        ets_snprintf(uiStr, sizeof(uiStr), "I %d", mzBestTimes[6]);
    }
    else
    {
        ets_snprintf(uiStr, sizeof(uiStr), "I -----");
    }
    plotText(64, (2 * FONT_HEIGHT_TOMTHUMB) + 2 * (FONT_HEIGHT_IBMVGA8), uiStr, IBM_VGA_8, WHITE);

    //TODO: explicitly add a hold to the text, or is the inverse effect enough.
    // (HOLD) CLEAR TIMES      TITLE
    plotText(1, OLED_HEIGHT - (1 * (FONT_HEIGHT_TOMTHUMB + 1)), "CLEAR TIMES", TOM_THUMB, WHITE);

    // fill the clear scores area depending on how long the button's held down.
    if (clearScoreTimer != 0)
    {
        double holdProgress = ((double)clearScoreTimer / (double)CLEAR_SCORES_HOLD_TIME);
        uint8_t holdFill = (uint8_t)(holdProgress * (getTextWidth("CLEAR TIMES", TOM_THUMB) + 2));
        fillDisplayArea(0, (OLED_HEIGHT - (1 * (FONT_HEIGHT_TOMTHUMB + 1))) - 1, holdFill, OLED_HEIGHT, INVERSE);
    }

    plotText(OLED_WIDTH - getTextWidth("TITLE", TOM_THUMB) - 1, OLED_HEIGHT - (1 * (FONT_HEIGHT_TOMTHUMB + 1)), "TITLE",
             TOM_THUMB, WHITE);
}

void ICACHE_FLASH_ATTR mzGameoverLedDisplay(void)
{
    switch (mazeLevel)
    {
        case PRACTICE_LEVEL:
            danceTimers[1].timerFn(NULL);
            break;
        case NOVICE_LEVEL:
            danceTimers[2].timerFn(NULL);
            break;
        case EASY_LEVEL:
            danceTimers[3].timerFn(NULL);
            break;
        case MIDDLE_LEVEL:
            danceTimers[4].timerFn(NULL);
            break;
        case HARD_LEVEL:
            danceTimers[11].timerFn(NULL);
            break;
        case KILLER_LEVEL:
            danceTimers[14].timerFn(NULL);
            break;
        case IMPOSSIBLE_LEVEL:
            danceTimers[15].timerFn(NULL);
            break;
        default:
            break;
    }
}
void ICACHE_FLASH_ATTR mzGameoverDisplay(void)
{
    if (!didAutoMaze)
    {
        mzGameoverLedDisplay();
    }
    // We don't clear the display because we want the playfield to appear in the background.
    // Draw a centered bordered window.

    //TODO: #define these instead of variables here?
    uint8_t windowXMargin = 1;
    uint8_t windowYMarginTop = 25;
    uint8_t windowYMarginBot = 0;

    uint8_t titleTextYOffset = 2;
    //uint8_t highScoreTextYOffset = titleTextYOffset + FONT_HEIGHT_IBMVGA8 + 5;
    //uint8_t scoreTextYOffset = highScoreTextYOffset + FONT_HEIGHT_TOMTHUMB + 5;
    uint8_t controlTextYOffset = OLED_HEIGHT - windowYMarginBot - FONT_HEIGHT_TOMTHUMB - 5;
    uint8_t controlTextXPadding = 5;

    // Draw a centered bordered window.
    if (didAutoMaze)
    {
        fillDisplayArea(windowXMargin, windowYMarginTop, OLED_WIDTH - windowXMargin, OLED_HEIGHT - windowYMarginBot, BLACK);
        plotRect(windowXMargin, windowYMarginTop, OLED_WIDTH - windowXMargin, OLED_HEIGHT - windowYMarginBot, WHITE);

        plotCenteredText(windowXMargin, windowYMarginTop + titleTextYOffset, OLED_WIDTH - windowXMargin,
                         "MAZE SOLVED AUTOMATICALLY IN ",
                         TOM_THUMB, WHITE);

        // 1230495
        char scoreStr[32] = {0};
        ets_snprintf(scoreStr, sizeof(scoreStr), "%d seconds", scoreauto);
        plotCenteredText(windowXMargin, windowYMarginTop + titleTextYOffset + FONT_HEIGHT_TOMTHUMB + 5,
                         OLED_WIDTH - windowXMargin, scoreStr,
                         IBM_VGA_8,
                         WHITE);
    }
    else
    {
        // If the gallery image was unlocked
        if (true == mazeDrawGalleryUnlock)
        {
            // Show a message that it was unlocked
            fillDisplayArea(
                windowXMargin,
                windowYMarginTop - FONT_HEIGHT_IBMVGA8 - 7,
                OLED_WIDTH - windowXMargin,
                windowYMarginTop,
                BLACK
            );
            plotRect(
                windowXMargin,
                windowYMarginTop - FONT_HEIGHT_IBMVGA8 - 7,
                OLED_WIDTH - windowXMargin,
                windowYMarginTop,
                WHITE
            );
            plotCenteredText(0, windowYMarginTop - FONT_HEIGHT_IBMVGA8 - 3, OLED_WIDTH, "GALLERY UNLOCK", IBM_VGA_8, WHITE);
        }

        fillDisplayArea(windowXMargin, windowYMarginTop, OLED_WIDTH - windowXMargin, OLED_HEIGHT - windowYMarginBot, BLACK);
        plotRect(windowXMargin, windowYMarginTop, OLED_WIDTH - windowXMargin, OLED_HEIGHT - windowYMarginBot, WHITE);

        plotCenteredText(windowXMargin, windowYMarginTop + titleTextYOffset, OLED_WIDTH - windowXMargin,
                         "YOUR ADJUSTED TIME IS",
                         TOM_THUMB, WHITE);

        // 1230495
        char scoreStr[32] = {0};
        ets_snprintf(scoreStr, sizeof(scoreStr), "%d seconds", score);
        plotCenteredText(windowXMargin, windowYMarginTop + titleTextYOffset + FONT_HEIGHT_TOMTHUMB + 5,
                         OLED_WIDTH - windowXMargin, scoreStr,
                         IBM_VGA_8,
                         WHITE);
    }
    // TITLE    RESTART
    plotText(windowXMargin + controlTextXPadding, controlTextYOffset, "BEST TIMES", TOM_THUMB, WHITE);
    plotText(OLED_WIDTH - windowXMargin - getTextWidth("TITLE", TOM_THUMB) - controlTextXPadding,
             controlTextYOffset,
             "TITLE", TOM_THUMB, WHITE);
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

    // One second pause before ball starts to move
    totalcyclestilldone = -1 * S_TO_MS_FACTOR / UPDATE_TIME_MS;
    totalhitstilldone = 0;
    gameover = false;
    memset(leds, 0, sizeof(leds));


    system_print_meminfo();
    maze_printf("Free Heap %d\n", system_get_free_heap_size());

    mazescalex = 127 / width;
    mazescaley = 63 / height;

    // This is the biggest rballused could be and then the ball will completely fill some corridors
    //    so for such a maze, it would be impossible to avoid touch walls.
    // Compute number of maximum pixels between ball and wall.
    wiggleroom = 2 * (min(mazescalex, mazescaley) - rballused);

    maze_printf("width:%d, height:%d mscx:%d mscy:%d rball:%d wiggleroom %d\n", width, height, mazescalex, mazescaley,
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
    maze_printf("initpt (%d, %d)\n", (int)scxcprev, (int)scycprev);
    for (i = 0; i < 4; i++)
    {
        maze_printf("exit corner %d (%d, %d)\n", i, (int)scxcexits[i], (int)scycexits[i]);
    }



    // get_maze allocates more memory, makes a random maze giving walls and solution to maze
    // and then deallocates memory
    out = get_maze(width, height, xleft, xright, ybot, ytop, xsol, ysol, scxcexits, scycexits, mazescalex, mazescaley);
    numwalls = out.indwall;
    indSolution = out.indSolution;
    // xleft, xright, ybot, ytop are lists of boundary intervals making maze
    if (numwalls > MAXNUMWALLS)
    {
        maze_printf("numwalls = %d exceeds MAXNUMWALLS = %d", numwalls, MAXNUMWALLS);
    }
    numwallstodraw = numwalls;

    // print scaled walls
    for (i = 0; i < numwalls; i++)
    {
        maze_printf("i %d (%d, %d) to (%d, %d)\n", i, mazescalex * xleft[i], mazescaley * ybot[i], mazescalex * xright[i],
                    mazescaley * ytop[i]);
    }

    // print solutions
    maze_printf("Solution ________________\n");
    for (i = 0; i < indSolution; i++)
    {
        maze_printf("(%d, %d) -> ", xsol[i], ysol[i]);
    }

    //Allocate some more working array memory now
    //TODO is memory being freed up appropriately?

    extendedScaledWallXleft = (float*)malloc (sizeof (float) * MAXNUMWALLS);
    extendedScaledWallXright = (float*)malloc (sizeof (float) * MAXNUMWALLS);
    extendedScaledWallYtop = (float*)malloc (sizeof (float) * MAXNUMWALLS);
    extendedScaledWallYbot = (float*)malloc (sizeof (float) * MAXNUMWALLS);

    maze_printf("After Working Arrays allocated Free Heap %d\n", system_get_free_heap_size());
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
        maze_printf("nwi = %d exceeds MAXNUMWALLS = %d", nwi, MAXNUMWALLS);
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
    mazeDrawGalleryUnlock = false;

    switch( currState )
    {
        case MZ_TITLE:
            // Clear leds
            memset(leds, 0, sizeof(leds));
            setmazeLeds(leds, sizeof(leds));
            break;
        case MZ_GAME:
            // All game restart functions happen here.
            mzLoadBestTimes();
            // TODO: should I be seeding this, or re-seeding this, and if so, with what?
            srand((uint32_t)(mzAccel.x + mzAccel.y * 3 + mzAccel.z * 5)); // Seed the random number generator.
            break;
        case MZ_AUTO:
            mzLoadBestTimes();
            indSolutionStep = 0;
            indSolutionSubStep = 0;
            totalcyclestilldone = 0;
            exitInd = UPPER_LEFT;
            break;
        case MZ_SCORES:
            mzLoadBestTimes();
            clearScoreTimer = 0;
            holdingClearScore = false;
            break;
        case MZ_GAMEOVER:
            // Update high score if needed.
            if (prevState != MZ_AUTO)
            {
                // If an impossible maze was solved
                if (IMPOSSIBLE_LEVEL == mazeLevel)
                {
                    // If the gallery image was just unlocked
                    if(true == unlockGallery(3))
                    {
                        mazeDrawGalleryUnlock = true;
                    }
                }

                nzNewBestTime = mzUpdateBestTimes(mazeLevel, score);
                if (nzNewBestTime)
                {
                    mzSaveBestTimes();
                }
                // Save the last score.
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
        maze_printf("very small NEGATIVE motion parameter s = %d/10000 so said goes thru\n", (int)(10000 * s));
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

    //maze_printf("%d ", (int)(1000*testdir));

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


void ICACHE_FLASH_ATTR mzLoadBestTimes(void)
{
    memcpy(mzBestTimes, mzGetBestTimes(),  NUM_MZ_LEVELS * sizeof(uint32_t));
}

void ICACHE_FLASH_ATTR mzSaveBestTimes(void)
{
    mzSetBestTimes(mzBestTimes);
}

bool ICACHE_FLASH_ATTR mzUpdateBestTimes(uint8_t levelInd, uint32_t newScore)
{
    bool bestTime = false;
    // Get the current score at this index.
    uint32_t currentScore = mzBestTimes[levelInd];

    if (newScore < currentScore)
    {
        mzBestTimes[levelInd] = newScore;
        bestTime = true;
    }

    return bestTime;
}
