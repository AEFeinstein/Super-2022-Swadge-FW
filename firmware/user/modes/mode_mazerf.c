/*
*	mode_mazerf.c
*
*	Created on: 21 Sept 2019
*               Author: bbkiw
*		Refactor of maze using Jonathan Moriarty basic set up
*/

#include <osapi.h>
#include <user_interface.h>
#include "user_main.h"	//swadge mode
#include "buttons.h"
#include "oled.h"		//display functions
#include "font.h"		//draw text
#include "bresenham.h"	//draw shapes
#include "linked_list.h" //custom linked list
#include "custom_commands.h" //saving and loading high scores and last scores
#include "mazegen.h"


/*============================================================================
 * Defines
 *==========================================================================*/
//NOTE in ode_solvers.h is #define of FLOATING float    or double to test
//#define LEN_PENDULUM 1

#define MAZE_DEBUG_PRINT
#ifdef MAZE_DEBUG_PRINT
#include <stdlib.h>
    #define maze_printf(...) os_printf(__VA_ARGS__)
#else
    #define maze_printf(...)
#endif

#ifndef max
    #define max(a,b) ((a) > (b) ? (a) : (b))
#endif
#ifndef min
    #define min(a,b) ((a) < (b) ? (a) : (b))
#endif

#define EMPTY 0

// controls (title)
#define BTN_TITLE_START_SCORES LEFT
#define BTN_TITLE_START_GAME RIGHT

// controls (game)
#define BTN_GAME_ROTATE RIGHT
#define BTN_GAME_DROP LEFT

// controls (scores)
#define BTN_SCORES_CLEAR_SCORES LEFT
#define BTN_SCORES_START_TITLE RIGHT

// controls (gameover)
#define BTN_GAMEOVER_START_TITLE LEFT
#define BTN_GAMEOVER_START_GAME RIGHT

// update task info.
#define UPDATE_TIME_MS 16 

// time info.
#define MS_TO_US_FACTOR 1000
#define MS_TO_S_FACTOR 1000
//#define US_TO_MS_FACTOR 0.001

#define CLEAR_SCORES_HOLD_TIME (5 * MS_TO_US_FACTOR * MS_TO_S_FACTOR)

// playfield settings.

#define GRID_WIDTH 10
#define GRID_HEIGHT 17

// All of these are (* level)
#define SCORE_SINGLE 100
#define SCORE_DOUBLE 300

// This is per cell.
#define SCORE_SOFT_DROP 1
// This is (* count * level)
#define SCORE_COMBO 50

#define NUM_MZ_HIGH_SCORES 3
#define ACCEL_SEG_SIZE 25 // higher value more or less means less sensetive.
#define ACCEL_JITTER_GUARD 14//7 // higher = less sensetive.

// any enums go here.

typedef enum
{
    A,	// title screen
    B,	// play the actual game
    C,	// high scores
    D // game over
} mazeType_t;

typedef enum
{
    MZ_TITLE,	// title screen
    MZ_GAME,	// play the actual game
    MZ_SCORES,	// high scores
    MZ_GAMEOVER // game over
} mazeState_t;

typedef enum
{
    RANDOM,//Pure random
    BAG,        //7 Bag
    POOL  //35 Pool with 6 rolls
} mazeRandomizer_t;

static mazeRandomizer_t randomizer = POOL;//BAG;

//BAG
static int typeBag[3] = {1,2,3}; 
static int bagIndex;

//POOL
static int typePool[35];
static int typeHistory[4];
static int firstType[4] = {1,2,3,4};
list_t * typeOrder;

uint32_t mazesGrid[GRID_HEIGHT][GRID_WIDTH];

uint32_t nextMazeGrid[GRID_HEIGHT][GRID_WIDTH];


// coordinates on the playfield grid, not the screen.
typedef struct
{
    int c;
    int r;
} coord_t;

typedef struct
{
    int c;
    int r;
} maze_t;


// Title screen info.

// Score screen info.
uint32_t clearScoreTimer;
bool holdingClearScore;

// Game state info.
uint32_t currentLevel; // The current difficulty level, increments every 10 line clears.
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

// game loop functions.
static void ICACHE_FLASH_ATTR mzUpdate(void* arg);

// handle inputs.
void ICACHE_FLASH_ATTR mzTitleInput(void);
void ICACHE_FLASH_ATTR mzGameInput(void);
void ICACHE_FLASH_ATTR mzScoresInput(void);
void ICACHE_FLASH_ATTR mzGameoverInput(void);

// update any input-unrelated logic.
void ICACHE_FLASH_ATTR mzTitleUpdate(void);
void ICACHE_FLASH_ATTR mzGameUpdate(void);
void ICACHE_FLASH_ATTR mzScoresUpdate(void);
void ICACHE_FLASH_ATTR mzGameoverUpdate(void);

// draw the frame.
void ICACHE_FLASH_ATTR mzTitleDisplay(void);
void ICACHE_FLASH_ATTR mzGameDisplay(void);
void ICACHE_FLASH_ATTR mzScoresDisplay(void);
void ICACHE_FLASH_ATTR mzGameoverDisplay(void);

// helper functions.

// mode state management.
void ICACHE_FLASH_ATTR mzChangeState(mazeState_t newState);

// input checking.
bool ICACHE_FLASH_ATTR mzIsButtonPressed(uint8_t button);
bool ICACHE_FLASH_ATTR mzIsButtonReleased(uint8_t button);
bool ICACHE_FLASH_ATTR mzIsButtonDown(uint8_t button);
bool ICACHE_FLASH_ATTR mzIsButtonUp(uint8_t button);

// grid management.

// drawing functions.
static void ICACHE_FLASH_ATTR plotSquare(int x0, int y0, int size, color col);
static void ICACHE_FLASH_ATTR plotGrid(int x0, int y0, uint8_t unitSize, uint8_t gridWidth, uint8_t gridHeight, uint32_t gridData[][gridWidth], color col);
static void ICACHE_FLASH_ATTR plotShape(int x0, int y0, uint8_t unitSize, uint8_t shapeWidth, uint8_t shapeHeight, uint32_t shape[][shapeWidth], color col);
static void ICACHE_FLASH_ATTR plotCenteredText(uint8_t x0, uint8_t y, uint8_t x1, char* text, fonts font, color col);
static uint8_t getTextWidth(char* text, fonts font);

// randomizer operations.
static void ICACHE_FLASH_ATTR initTypeOrder(void);
static void ICACHE_FLASH_ATTR clearTypeOrder(void);
static void ICACHE_FLASH_ATTR deInitTypeOrder(void);
static void ICACHE_FLASH_ATTR initMazeRandomizer(mazeRandomizer_t randomType);
static int ICACHE_FLASH_ATTR getNextMazeType(mazeRandomizer_t randomType, int index);
static void shuffle(int length, int array[length]);

static uint32_t ICACHE_FLASH_ATTR getDropTime(uint32_t level);

// score operations.
static void loadHighScores(void);
static void saveHighScores(void);
static bool updateHighScores(uint32_t newScore);

// game logic operations.
static void ICACHE_FLASH_ATTR initLandedMazes(void);
static void ICACHE_FLASH_ATTR clearLandedMazes(void);
static void ICACHE_FLASH_ATTR deInitLandedMazes(void);

static bool ICACHE_FLASH_ATTR isLineCleared(int line, uint8_t gridWidth, uint8_t gridHeight, uint32_t gridData[][gridWidth]);
static int ICACHE_FLASH_ATTR checkLineClears(uint8_t gridWidth, uint8_t gridHeight, uint32_t gridData[][gridWidth]);

static bool ICACHE_FLASH_ATTR checkCollision(coord_t newPos, uint8_t shapeWidth, uint8_t shapeHeight, uint32_t shape[][shapeWidth], uint8_t gridWidth, uint8_t gridHeight, uint32_t gridData[][gridWidth], uint32_t selfGridValue);

swadgeMode mazerfMode = 
{
	.modeName = "Mazerf",
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

static os_timer_t timerHandleUpdate = {0};

static uint32_t modeStartTime = 0; // time mode started in microseconds.
static uint32_t stateStartTime = 0; // time the most recent state started in microseconds.
static uint32_t deltaTime = 0;	// time elapsed since last update.
static uint32_t modeTime = 0;	// total time the mode has been running.
static uint32_t stateTime = 0;	// total time the game has been running.

static mazeState_t currState = MZ_TITLE;

void ICACHE_FLASH_ATTR mzInit(void)
{
    // Give us responsive input.
	enableDebounce(false);	
	
	// Reset mode time tracking.
	modeStartTime = system_get_time();
	modeTime = 0;

	// Reset state stuff.
	mzChangeState(MZ_TITLE);

    // Grab any memory we need.
    //initLandedMazes();
    //initTypeOrder();

	// Start the update loop.
    os_timer_disarm(&timerHandleUpdate);
    os_timer_setfn(&timerHandleUpdate, (os_timer_func_t*)mzUpdate, NULL);
    os_timer_arm(&timerHandleUpdate, UPDATE_TIME_MS, 1);
}

void ICACHE_FLASH_ATTR mzDeInit(void)
{
	os_timer_disarm(&timerHandleUpdate);
    //deInitLandedMazes();
    //deInitTypeOrder();
}

void ICACHE_FLASH_ATTR mzButtonCallback(uint8_t state, int button __attribute__((unused)), int down __attribute__((unused)))
{
	mzButtonState = state;	// Set the state of all buttons
}

void ICACHE_FLASH_ATTR mzAccelerometerCallback(accel_t* accel)
{
    mzAccel.x = accel->x;	// Set the accelerometer values
    mzAccel.y = accel->y;
    mzAccel.z = accel->z;
}

static void ICACHE_FLASH_ATTR mzUpdate(void* arg __attribute__((unused)))
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

void ICACHE_FLASH_ATTR mzGameInput(void)
{
    //Refresh the mazes grid.
    //refreshMazesGrid(false);

	//button a = rotate piece
    //if(mzIsButtonPressed(BTN_GAME_ROTATE))
    //{
    //}

    //button b = soft drop piece
    //if(mzIsButtonDown(BTN_GAME_DROP))
    //{
    //    softDropMaze();
    //}

    // Only move mazes left and right when the fast drop button isn't being held down.
    //if(mzIsButtonUp(BTN_GAME_DROP))
    //{
    //}
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
        mzChangeState(MZ_GAME);
    }
    //button b = go to title screen
    else if(mzIsButtonPressed(BTN_GAMEOVER_START_TITLE))
    {
        mzChangeState(MZ_TITLE);
    }
}

void ICACHE_FLASH_ATTR mzTitleUpdate(void)
{
}

void ICACHE_FLASH_ATTR mzGameUpdate(void)
{
    //Refresh the mazes grid.
    //refreshMazesGrid(false);
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

    // TILTRADS
    plotText(20, 5, "MAG MAZE", RADIOSTARS, WHITE);
    
    // SCORES   START
    plotText(0, OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)), "SCORES", IBM_VGA_8, WHITE);
    plotText(OLED_WIDTH - getTextWidth("START", IBM_VGA_8), OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)), "START", IBM_VGA_8, WHITE);


}

void ICACHE_FLASH_ATTR mzGameDisplay(void)
{
    // Clear the display
    clearDisplay();

    // Draw the active maze.
    
    // Clear the grid data (may not want to do this every frame)
    //refreshMazesGrid(true);

    // Draw the background grid. NOTE: (make sure everything that needs to be in mazesGrid is in there now).
 
    newHighScore = score > highScores[0];
    plotCenteredText(0, 0, 10, newHighScore ? "HIGH (NEW!)" : "HIGH", TOM_THUMB, WHITE);
}

void ICACHE_FLASH_ATTR mzScoresDisplay(void)
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
    ets_snprintf(uiStr, sizeof(uiStr), "YOUR LAST SCORE: %d", mzGetLastScore());
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

void ICACHE_FLASH_ATTR mzGameoverDisplay(void)
{
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
    plotText(windowXMargin + controlTextXPadding, controlTextYOffset, "TITLE", TOM_THUMB, WHITE);
    plotText(OLED_WIDTH - windowXMargin - getTextWidth("RESTART", TOM_THUMB) - controlTextXPadding, controlTextYOffset, "RESTART", TOM_THUMB, WHITE);
}

// helper functions.

void ICACHE_FLASH_ATTR mzChangeState(mazeState_t newState)
{
	currState = newState;
	stateStartTime = system_get_time();
	stateTime = 0;

    switch( currState )
    {
        case MZ_TITLE:          
            break;
        case MZ_GAME:
            // All game restart functions happen here.
            //clearGrid(GRID_WIDTH, GRID_HEIGHT, mazesGrid);
            loadHighScores();
            // TODO: should I be seeding this, or re-seeding this, and if so, with what?
            srand((uint32_t)(mzAccel.x + mzAccel.y * 3 + mzAccel.z * 5)); // Seed the random number generator.
            initMazeRandomizer(randomizer);        
            break;
        case MZ_SCORES:
            loadHighScores();
            clearScoreTimer = 0;
            holdingClearScore = false;
            break;
        case MZ_GAMEOVER:
            // Update high score if needed.
            newHighScore = updateHighScores(score);
            if (newHighScore) saveHighScores();
            // Save out the last score.
            mzSetLastScore(score);
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

/*
static void ICACHE_FLASH_ATTR copyGrid(coord_t srcOffset, uint8_t srcWidth, uint8_t srcHeight, uint32_t src[][srcWidth], uint8_t dstWidth, uint8_t dstHeight, uint32_t dst[][dstWidth])
{
    for (int r = 0; r < srcHeight; r++)
    {
        for (int c = 0; c < srcWidth; c++)
        {
            int dstC = c + srcOffset.c;
            int dstR = r + srcOffset.r;
            if (dstC < dstWidth && dstR < dstHeight) dst[dstR][dstC] = src[r][c];
        }
    }
}

static void ICACHE_FLASH_ATTR transferGrid(coord_t srcOffset, uint8_t srcWidth, uint8_t srcHeight, uint32_t src[][srcWidth], uint8_t dstWidth, uint8_t dstHeight, uint32_t dst[][dstWidth], uint32_t transferVal)
{
    for (int r = 0; r < srcHeight; r++)
    {
        for (int c = 0; c < srcWidth; c++)
        {
            int dstC = c + srcOffset.c;
            int dstR = r + srcOffset.r;
            if (dstC < dstWidth && dstR < dstHeight) 
            {
                if (src[r][c] != EMPTY)
                {
                    dst[dstR][dstC] = transferVal;
                }
            }
        }
    }
}

static void ICACHE_FLASH_ATTR clearGrid(uint8_t gridWidth, uint8_t gridHeight, uint32_t gridData[][gridWidth])
{
    for (int y = 0; y < gridHeight; y++)
    {
        for (int x = 0; x < gridWidth; x++)
        {
            gridData[y][x] = EMPTY;
        }
    }
}

void ICACHE_FLASH_ATTR refreshMazesGrid(bool includeActive)
{
}

// This assumes only complete mazes can be rotated.
void ICACHE_FLASH_ATTR rotateMaze(maze_t * maze)
{
}

void ICACHE_FLASH_ATTR softDropMaze()
{
}

void ICACHE_FLASH_ATTR moveMaze(maze_t * maze)
{
}

bool ICACHE_FLASH_ATTR dropMaze(maze_t * maze)
{
    return 1;
}
*/


//maze_t ICACHE_FLASH_ATTR spawnMaze(mazeType_t type, uint32_t gridValue, coord_t gridCoord, int rotation)
//{
//    maze_t x;
//    return x;
//}

//void ICACHE_FLASH_ATTR spawnNextMaze(maze_t * newMaze, mazeRandomizer_t randomType, uint32_t gridValue)
//{
//}

void ICACHE_FLASH_ATTR plotSquare(int x0, int y0, int size, color col)
{
    plotRect(x0, y0, x0 + (size - 1), y0 + (size - 1), col);
}

void ICACHE_FLASH_ATTR plotGrid(int x0, int y0, uint8_t unitSize, uint8_t gridWidth, uint8_t gridHeight, uint32_t gridData[][gridWidth], color col)
{
    // Draw the border
    plotRect(x0, y0, x0 + (unitSize - 1) * gridWidth, y0 + (unitSize - 1) * gridHeight, col);

    // Draw points for grid (maybe disable when not debugging)
    for (int y = 0; y < gridHeight; y++)
    {
        for (int x = 0; x < gridWidth; x++) 
        {
            //if (gridData[y][x] == EMPTY) drawPixel(x0 + x * (unitSize - 1) + (unitSize / 2), y0 + y * (unitSize - 1) + (unitSize / 2), WHITE);
        }
    }
}

void ICACHE_FLASH_ATTR plotShape(int x0, int y0, uint8_t unitSize, uint8_t shapeWidth, uint8_t shapeHeight, uint32_t shape[][shapeWidth], color col)
{   
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

void ICACHE_FLASH_ATTR initTypeOrder()
{
    typeOrder = malloc(sizeof(list_t));
    typeOrder->first = NULL;
    typeOrder->last = NULL;
    typeOrder->length = 0;
}

void ICACHE_FLASH_ATTR clearTypeOrder()
{
    // Free all ints in the list.
    node_t * current = typeOrder->first;
    while (current != NULL)
    {
        free(current->val);
        current->val = NULL;
        current = current->next;
    }
    // Free the node containers for the list.
    clear(typeOrder);
}

void ICACHE_FLASH_ATTR deInitTypeOrder()
{
    clearTypeOrder();

    // Finally free the list itself.
    free(typeOrder);
    typeOrder = NULL;
}

void ICACHE_FLASH_ATTR initMazeRandomizer(mazeRandomizer_t randomType)
{
    switch (randomType)
    {
        case RANDOM:
            break;
        case BAG:
            bagIndex = 0;
            shuffle(3, typeBag);
            break;
        case POOL:
            {
                // Initialize the maze type pool, 5 of each type.
                for (int i = 0; i < 5; i++)
                {
                    for (int j = 0; j < 3; j++)
                    {
                        typePool[i * 3 + j] = j+1;
                    }
                }

                // Clear the history.
                for (int i = 0; i < 4; i++)
                {
                    typeHistory[i] = 0;
                }

                // Populate the history with initial values.
                typeHistory[0] = 1;
                typeHistory[1] = 2;
                typeHistory[2] = 3;

                // Clear the order list.
                clearTypeOrder();
            }
            break;
        default:
            break;
    }
}

int ICACHE_FLASH_ATTR getNextMazeType(mazeRandomizer_t randomType, int index)
{
}

// Fisher–Yates Shuffle
void shuffle(int length, int array[length])
{
    for (int i = length-1; i > 0; i--) 
    { 
        // Pick a random index from 0 to i 
        int j = rand() % (i+1); 
  
        // Swap array[i] with the element at random index 
        int temp = array[i]; 
        array[i] = array[j]; 
        array[j] = temp; 
    } 
}

//uint32_t ICACHE_FLASH_ATTR getDropTime(uint32_t level)
//{
//}

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
/*
void ICACHE_FLASH_ATTR initLandedMazes()
{
}

void ICACHE_FLASH_ATTR clearLandedMazes()
{
}

void ICACHE_FLASH_ATTR deInitLandedMazes()
{
}

bool ICACHE_FLASH_ATTR isLineCleared(int line, uint8_t gridWidth, uint8_t gridHeight __attribute__((unused)), uint32_t gridData[][gridWidth])
{
}

int ICACHE_FLASH_ATTR checkLineClears(uint8_t gridWidth, uint8_t gridHeight, uint32_t gridData[][gridWidth])
{
}

// what is the best way to handle collisions above the grid space?
bool ICACHE_FLASH_ATTR checkCollision(coord_t newPos, uint8_t shapeWidth, uint8_t shapeHeight __attribute__((unused)), uint32_t shape[][shapeWidth], uint8_t gridWidth, uint8_t gridHeight, uint32_t gridData[][gridWidth], uint32_t selfGridValue)
{
}
*/
