/*
*	mode_tiltrads.c
*
*	Created on: Aug 2, 2019
*		Author: Jonathan Moriarty
*/

#include <osapi.h>
#include <user_interface.h>
#include <stdlib.h>

#include "user_main.h"	//swadge mode
#include "buttons.h"
#include "oled.h"		//display functions
#include "font.h"		//draw text
#include "bresenham.h"	//draw shapes
#include "linked_list.h" //custom linked list
#include "custom_commands.h" //saving and loading high scores and last scores

//NOTES:
// Decided not to handle cascade clears that result from falling tetrads after clears. Closer to target behavior.

//TODO:
// Add different block fills for different tetrad types?
// Add mode 7 effect for screen sides.
// Add VFX that use Swadge LEDs.
// Add SFX and / or Music.

// Do second pass on accelerometer code.
// Refactor to fix refresh bug and extraneous reference to tetradCounter.
// Refactor to remove unnecessary math operations that are not good on ESP (mod and division?)

// Refine the placement of UI and background elements in the game screen.
// Balance all gameplay and control variables based on feedback. (3-4 minute playtime, t99 round target)
// Test to make sure mode is not a battery killer.
// Test to make sure there are no bugs.

 
//#define NO_STRESS_TRIS // Debug mode that when enabled, stops tetrads from dropping automatically, they will only drop when the drop button is pressed. Useful for testing line clears.

// any defines go here.

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
#define S_TO_MS_FACTOR 1000
#define US_TO_MS_FACTOR 0.001
#define MS_TO_S_FACTOR 0.001

#define CLEAR_LINES_ANIM_TIME (0.25 * S_TO_MS_FACTOR * MS_TO_US_FACTOR)

#define CLEAR_SCORES_HOLD_TIME (5 * S_TO_MS_FACTOR * MS_TO_US_FACTOR)

// playfield settings.
#define GRID_X 44
#define GRID_Y -5
#define GRID_UNIT_SIZE 5
#define GRID_WIDTH 10
#define GRID_HEIGHT 17

#define NEXT_GRID_X 100//97
#define NEXT_GRID_Y 8
#define NEXT_GRID_WIDTH 5
#define NEXT_GRID_HEIGHT 5

#define TUTORIAL_GRID_WIDTH 10
#define TUTORIAL_GRID_HEIGHT 18

#define EMPTY 0

#define TETRAD_SPAWN_ROT 0
#define TETRAD_SPAWN_X 3
#define TETRAD_SPAWN_Y 0
#define TETRAD_GRID_SIZE 4

#define NUM_TETRAD_TYPES 7

// All of these are (* level)
#define SCORE_SINGLE 100
#define SCORE_DOUBLE 300
#define SCORE_TRIPLE 500
#define SCORE_QUAD 800
// This is per cell.
#define SCORE_SOFT_DROP 1
// This is (* count * level)
#define SCORE_COMBO 50

//TODO: spin scoring, multipliers for consecutive difficult line clears?

#define ACCEL_SEG_SIZE 25 // higher value more or less means less sensetive.
#define ACCEL_JITTER_GUARD 14//7 // higher = less sensetive.

#define SOFT_DROP_FACTOR 8
#define LINE_CLEARS_PER_LEVEL 5

#define TITLE_LEVEL 5 // The level used for calculating drop speed on the title screen.

// any enums go here.
typedef enum
{
    TT_TITLE,	// title screen
    TT_GAME,	// play the actual game
    TT_SCORES,	// high scores
    TT_GAMEOVER // game over
} tiltradsState_t;

typedef enum
{
    I_TETRAD=1,	
    O_TETRAD=2,	
    T_TETRAD=3,
    J_TETRAD=4,
    L_TETRAD=5,
    S_TETRAD=6,
    Z_TETRAD=7
} tetradType_t;

//TODO: better names for these?
//TODO: also support other incrmental steps.
typedef enum
{
    RANDOM,//Pure random
    BAG,        //7 Bag
    POOL  //35 Pool with 6 rolls
} tetradRandomizer_t;

tetradRandomizer_t randomizer = POOL;//BAG;

//BAG
int typeBag[NUM_TETRAD_TYPES] = {I_TETRAD, J_TETRAD, L_TETRAD, O_TETRAD, S_TETRAD, T_TETRAD, Z_TETRAD}; 
int bagIndex;

//POOL
int typePool[35];
int typeHistory[4];
int firstType[4] = {I_TETRAD, J_TETRAD, L_TETRAD, T_TETRAD};
list_t * typeOrder;

uint32_t tetradsGrid[GRID_HEIGHT][GRID_WIDTH];

uint32_t nextTetradGrid[NEXT_GRID_HEIGHT][NEXT_GRID_WIDTH];

uint32_t iTetradRotations [4][TETRAD_GRID_SIZE][TETRAD_GRID_SIZE] = 
{
    {{0,0,0,0},
     {1,1,1,1},
     {0,0,0,0},
     {0,0,0,0}},
    {{0,0,1,0},
     {0,0,1,0},
     {0,0,1,0},
     {0,0,1,0}},
    {{0,0,0,0},
     {0,0,0,0},
     {1,1,1,1},
     {0,0,0,0}},
    {{0,1,0,0},
     {0,1,0,0},
     {0,1,0,0},
     {0,1,0,0}}
};

uint32_t oTetradRotations [4][TETRAD_GRID_SIZE][TETRAD_GRID_SIZE] = 
{
    {{0,1,1,0},
     {0,1,1,0},
     {0,0,0,0},
     {0,0,0,0}},
    {{0,1,1,0},
     {0,1,1,0},
     {0,0,0,0},
     {0,0,0,0}},
    {{0,1,1,0},
     {0,1,1,0},
     {0,0,0,0},
     {0,0,0,0}},
    {{0,1,1,0},
     {0,1,1,0},
     {0,0,0,0},
     {0,0,0,0}}
};

uint32_t tTetradRotations [4][TETRAD_GRID_SIZE][TETRAD_GRID_SIZE] = 
{
    {{0,1,0,0},
     {1,1,1,0},
     {0,0,0,0},
     {0,0,0,0}},
    {{0,1,0,0},
     {0,1,1,0},
     {0,1,0,0},
     {0,0,0,0}},
    {{0,0,0,0},
     {1,1,1,0},
     {0,1,0,0},
     {0,0,0,0}},
    {{0,1,0,0},
     {1,1,0,0},
     {0,1,0,0},
     {0,0,0,0}}
};

uint32_t jTetradRotations [4][TETRAD_GRID_SIZE][TETRAD_GRID_SIZE] = 
{
    {{1,0,0,0},
     {1,1,1,0},
     {0,0,0,0},
     {0,0,0,0}},
    {{0,1,1,0},
     {0,1,0,0},
     {0,1,0,0},
     {0,0,0,0}},
    {{0,0,0,0},
     {1,1,1,0},
     {0,0,1,0},
     {0,0,0,0}},
    {{0,1,0,0},
     {0,1,0,0},
     {1,1,0,0},
     {0,0,0,0}}
};

uint32_t lTetradRotations [4][TETRAD_GRID_SIZE][TETRAD_GRID_SIZE] = 
{
    {{0,0,1,0},
     {1,1,1,0},
     {0,0,0,0},
     {0,0,0,0}},
    {{0,1,0,0},
     {0,1,0,0},
     {0,1,1,0},
     {0,0,0,0}},
    {{0,0,0,0},
     {1,1,1,0},
     {1,0,0,0},
     {0,0,0,0}},
    {{1,1,0,0},
     {0,1,0,0},
     {0,1,0,0},
     {0,0,0,0}}
};

uint32_t sTetradRotations [4][TETRAD_GRID_SIZE][TETRAD_GRID_SIZE] = 
{
    {{0,1,1,0},
     {1,1,0,0},
     {0,0,0,0},
     {0,0,0,0}},
    {{0,1,0,0},
     {0,1,1,0},
     {0,0,1,0},
     {0,0,0,0}},
    {{0,0,0,0},
     {0,1,1,0},
     {1,1,0,0},
     {0,0,0,0}},
    {{1,0,0,0},
     {1,1,0,0},
     {0,1,0,0},
     {0,0,0,0}}
};

uint32_t zTetradRotations [4][TETRAD_GRID_SIZE][TETRAD_GRID_SIZE] = 
{
    {{1,1,0,0},
     {0,1,1,0},
     {0,0,0,0},
     {0,0,0,0}},
    {{0,0,1,0},
     {0,1,1,0},
     {0,1,0,0},
     {0,0,0,0}},
    {{0,0,0,0},
     {1,1,0,0},
     {0,1,1,0},
     {0,0,0,0}},
    {{0,1,0,0},
     {1,1,0,0},
     {1,0,0,0},
     {0,0,0,0}}
};

// coordinates on the playfield grid, not the screen.
typedef struct
{
    int c;
    int r;
} coord_t;

/*
    J, L, S, T, Z TETRAD
    0>>1 	( 0, 0) (-1, 0) (-1, 1) ( 0,-2) (-1,-2)
    1>>2 	( 0, 0) ( 1, 0) ( 1,-1) ( 0, 2) ( 1, 2)
    2>>3 	( 0, 0) ( 1, 0) ( 1, 1) ( 0,-2) ( 1,-2)
    3>>0 	( 0, 0) (-1, 0) (-1,-1) ( 0, 2) (-1, 2)

    I TETRAD
    0>>1 	( 0, 0) (-2, 0) ( 1, 0) (-2,-1) ( 1, 2)
    1>>2 	( 0, 0) (-1, 0) ( 2, 0) (-1, 2) ( 2,-1)
    2>>3 	( 0, 0) ( 2, 0) (-1, 0) ( 2, 1) (-1,-2)
    3>>0 	( 0, 0) ( 1, 0) (-2, 0) ( 1,-2) (-2, 1)
*/

coord_t iTetradRotationTests [4][5] =
{
    {{0,0},{-2,0},{1,0},{-2,1},{1,-2}},
    {{0,0},{-1,0},{2,0},{-1,-2},{2,1}},
    {{0,0},{2,0},{-1,0},{2,-1},{-1,2}},
    {{0,0},{1,0},{-2,1},{1,2},{-2,-1}}
};

coord_t otjlszTetradRotationTests [4][5] =
{
    {{0,0},{-1,0},{-1,-1},{0,2},{-1,2}},
    {{0,0},{1,0},{1,1},{0,-2},{1,-2}},
    {{0,0},{1,0},{1,-1},{0,2},{1,2}},
    {{0,0},{-1,0},{-1,1},{0,-2},{-1,-2}}
};

typedef struct
{
    tetradType_t type;
    uint32_t gridValue; // When taking up space on a larger grid of multiple tetrads, used to distinguish tetrads from each other.
    int rotation; 
    coord_t topLeft;
    uint32_t shape[TETRAD_GRID_SIZE][TETRAD_GRID_SIZE];
} tetrad_t;

// Title screen info.
tetrad_t tutorialTetrad;
uint32_t tutorialTetradsGrid[TUTORIAL_GRID_HEIGHT][TUTORIAL_GRID_WIDTH];

// Score screen info.
uint32_t clearScoreTimer;
bool holdingClearScore;

// Game state info.
tetrad_t activeTetrad;
tetradType_t nextTetradType;
uint32_t tetradCounter; // Used for distinguishing tetrads on the full grid, and for counting how many total tetrads have landed.
uint32_t dropTimer;  // The timer for dropping the current tetrad one level.
uint32_t dropTime; // The amount of time it takes for a tetrad to drop. Changes based on the level.
uint32_t linesClearedTotal; // The number of lines cleared total.
uint32_t linesClearedLastDrop; // The number of lines cleared the last time a tetrad landed. (Used for combos)
uint32_t comboCount; // The combo counter for successive line clears.
uint32_t currentLevel; // The current difficulty level, increments every 10 line clears.
uint32_t score; // The current score this game.
list_t * landedTetrads;
uint32_t highScores[NUM_TT_HIGH_SCORES];
bool newHighScore;

// Score screen ui info.
uint8_t score0X;
uint8_t score1X;
uint8_t score2X;
uint8_t lastScoreX;

// Gameover ui info.
bool drawGameoverTetrad;
uint8_t gameoverScoreX;

// Clear animation info.
bool inClearAnimation;
uint32_t clearTimer;
uint32_t clearTime;
//uint32_t clearAnimFrame;

// Input vars.
accel_t ttAccel = {0};
accel_t ttLastAccel = {0};
accel_t ttLastTestAccel = {0};

uint8_t ttButtonState = 0;
uint8_t ttLastButtonState = 0;

// Timer vars.
static os_timer_t timerHandleUpdate = {0};

uint32_t modeStartTime = 0; // time mode started in microseconds.
uint32_t stateStartTime = 0; // time the most recent state started in microseconds.
uint32_t deltaTime = 0;	// time elapsed since last update in microseconds.
uint32_t modeTime = 0;	// total time the mode has been running in microseconds.
uint32_t stateTime = 0;	// total time the state has been running in microseconds.
uint32_t modeFrames = 0; // total number of frames elapsed in this mode.
uint32_t stateFrames = 0; // total number of frames elapsed in this state.

// Game state.
tiltradsState_t currState = TT_TITLE;

// function prototypes go here.
void ICACHE_FLASH_ATTR ttInit(void);
void ICACHE_FLASH_ATTR ttDeInit(void);
void ICACHE_FLASH_ATTR ttButtonCallback(uint8_t state, int button, int down);
void ICACHE_FLASH_ATTR ttAccelerometerCallback(accel_t* accel);

// game loop functions.
static void ICACHE_FLASH_ATTR ttUpdate(void* arg);

// handle inputs.
void ICACHE_FLASH_ATTR ttTitleInput(void);
void ICACHE_FLASH_ATTR ttGameInput(void);
void ICACHE_FLASH_ATTR ttScoresInput(void);
void ICACHE_FLASH_ATTR ttGameoverInput(void);

// update any input-unrelated logic.
void ICACHE_FLASH_ATTR ttTitleUpdate(void);
void ICACHE_FLASH_ATTR ttGameUpdate(void);
void ICACHE_FLASH_ATTR ttScoresUpdate(void);
void ICACHE_FLASH_ATTR ttGameoverUpdate(void);

// draw the frame.
void ICACHE_FLASH_ATTR ttTitleDisplay(void);
void ICACHE_FLASH_ATTR ttGameDisplay(void);
void ICACHE_FLASH_ATTR ttScoresDisplay(void);
void ICACHE_FLASH_ATTR ttGameoverDisplay(void);

// helper functions.

// mode state management.
void ICACHE_FLASH_ATTR ttChangeState(tiltradsState_t newState);

// input checking.
bool ICACHE_FLASH_ATTR ttIsButtonPressed(uint8_t button);
bool ICACHE_FLASH_ATTR ttIsButtonReleased(uint8_t button);
bool ICACHE_FLASH_ATTR ttIsButtonDown(uint8_t button);
bool ICACHE_FLASH_ATTR ttIsButtonUp(uint8_t button);

// grid management.
void ICACHE_FLASH_ATTR copyGrid(coord_t srcOffset, uint8_t srcWidth, uint8_t srcHeight, uint32_t src[][srcWidth], uint8_t dstWidth, uint8_t dstHeight, uint32_t dst[][dstWidth]);
void ICACHE_FLASH_ATTR transferGrid(coord_t srcOffset, uint8_t srcWidth, uint8_t srcHeight, uint32_t src[][srcWidth], uint8_t dstWidth, uint8_t dstHeight, uint32_t dst[][dstWidth], uint32_t transferVal);
void ICACHE_FLASH_ATTR clearGrid(uint8_t gridWidth, uint8_t gridHeight, uint32_t gridData[][gridWidth]);
void ICACHE_FLASH_ATTR refreshTetradsGrid(uint8_t gridWidth, uint8_t gridHeight, uint32_t gridData[][gridWidth], list_t * fieldTetrads, tetrad_t * movingTetrad, bool includeMovingTetrad);

// tetrad operations.
void ICACHE_FLASH_ATTR rotateTetrad(tetrad_t * tetrad, uint8_t gridWidth, uint8_t gridHeight, uint32_t gridData[][gridWidth]);
void ICACHE_FLASH_ATTR softDropTetrad(void);
void ICACHE_FLASH_ATTR moveTetrad(tetrad_t * tetrad, uint8_t gridWidth, uint8_t gridHeight, uint32_t gridData[][gridWidth]);
bool ICACHE_FLASH_ATTR dropTetrad(tetrad_t * tetrad, uint8_t gridWidth, uint8_t gridHeight, uint32_t gridData[][gridWidth]);
tetrad_t ICACHE_FLASH_ATTR spawnTetrad(tetradType_t type, uint32_t gridValue, coord_t gridCoord, int rotation);
void ICACHE_FLASH_ATTR spawnNextTetrad(tetrad_t * newTetrad, tetradRandomizer_t randomType, uint32_t gridValue, uint8_t gridWidth, uint8_t gridHeight, uint32_t gridData[][gridWidth]);

// drawing functions.
void ICACHE_FLASH_ATTR plotSquare(int x0, int y0, int size, color col);
void ICACHE_FLASH_ATTR plotGrid(int x0, int y0, uint8_t unitSize, uint8_t gridWidth, uint8_t gridHeight, uint32_t gridData[][gridWidth], bool clearLineAnimation, color col);
void ICACHE_FLASH_ATTR plotShape(int x0, int y0, uint8_t unitSize, uint8_t shapeWidth, uint8_t shapeHeight, uint32_t shape[][shapeWidth], color col);
uint8_t ICACHE_FLASH_ATTR plotCenteredText(uint8_t x0, uint8_t y, uint8_t x1, char* text, fonts font, color col);
uint8_t ICACHE_FLASH_ATTR getCenteredTextX(uint8_t x0, uint8_t x1, char* text, fonts font);
uint8_t ICACHE_FLASH_ATTR getTextWidth(char* text, fonts font);

// randomizer operations.
void ICACHE_FLASH_ATTR initTypeOrder(void);
void ICACHE_FLASH_ATTR clearTypeOrder(void);
void ICACHE_FLASH_ATTR deInitTypeOrder(void);
void ICACHE_FLASH_ATTR initTetradRandomizer(tetradRandomizer_t randomType);
int ICACHE_FLASH_ATTR getNextTetradType(tetradRandomizer_t randomType, int index);
void shuffle(int length, int array[length]);

uint32_t ICACHE_FLASH_ATTR getDropTime(uint32_t level);

// score operations.
void loadHighScores(void);
void saveHighScores(void);
bool updateHighScores(uint32_t newScore);

// game logic operations.
void ICACHE_FLASH_ATTR initLandedTetrads(void);
void ICACHE_FLASH_ATTR clearLandedTetrads(void);
void ICACHE_FLASH_ATTR deInitLandedTetrads(void);

void ICACHE_FLASH_ATTR startClearAnimation(int numLineClears);
void ICACHE_FLASH_ATTR stopClearAnimation(void);

bool ICACHE_FLASH_ATTR isLineCleared(int line, uint8_t gridWidth, uint8_t gridHeight, uint32_t gridData[][gridWidth]);
int ICACHE_FLASH_ATTR checkLineClears(uint8_t gridWidth, uint8_t gridHeight, uint32_t gridData[][gridWidth], list_t * fieldTetrads);
int ICACHE_FLASH_ATTR clearLines(uint8_t gridWidth, uint8_t gridHeight, uint32_t gridData[][gridWidth], list_t * fieldTetrads);

bool ICACHE_FLASH_ATTR checkCollision(coord_t newPos, uint8_t shapeWidth, uint8_t shapeHeight, uint32_t shape[][shapeWidth], uint8_t gridWidth, uint8_t gridHeight, uint32_t gridData[][gridWidth], uint32_t selfGridValue);

swadgeMode tiltradsMode = 
{
	.modeName = "Tiltrads",
	.fnEnterMode = ttInit,
	.fnExitMode = ttDeInit,
	.fnButtonCallback = ttButtonCallback,
	.wifiMode = NO_WIFI,
	.fnEspNowRecvCb = NULL,
	.fnEspNowSendCb = NULL,
	.fnAccelerometerCallback = ttAccelerometerCallback
};

void ICACHE_FLASH_ATTR ttInit(void)
{
    // Give us responsive input.
	enableDebounce(false);	
	
	// Reset mode time tracking.
	modeStartTime = system_get_time();
	modeTime = 0;
    modeFrames = 0;

    // Grab any memory we need.
    initLandedTetrads();
    initTypeOrder();

    // Reset state stuff.
	ttChangeState(TT_TITLE);

	// Start the update loop.
    os_timer_disarm(&timerHandleUpdate);
    os_timer_setfn(&timerHandleUpdate, (os_timer_func_t*)ttUpdate, NULL);
    os_timer_arm(&timerHandleUpdate, UPDATE_TIME_MS, 1);
}

void ICACHE_FLASH_ATTR ttDeInit(void)
{
	os_timer_disarm(&timerHandleUpdate);
    deInitLandedTetrads();
    deInitTypeOrder();
}

void ICACHE_FLASH_ATTR ttButtonCallback(uint8_t state, int button __attribute__((unused)), int down __attribute__((unused)))
{
	ttButtonState = state;	// Set the state of all buttons
}

void ICACHE_FLASH_ATTR ttAccelerometerCallback(accel_t* accel)
{
	ttAccel.x = accel->x;	// Set the accelerometer values
    ttAccel.y = accel->y;
    ttAccel.z = accel->z;
}

static void ICACHE_FLASH_ATTR ttUpdate(void* arg __attribute__((unused)))
{
	// Update time tracking.
    // NOTE: delta time is in microseconds.
    // UPDATE time is in milliseconds.

	uint32_t newModeTime = system_get_time() - modeStartTime;
	uint32_t newStateTime = system_get_time() - stateStartTime;
	deltaTime = newModeTime - modeTime;
	modeTime = newModeTime;
	stateTime = newStateTime;
    modeFrames++;
    stateFrames++;

	// Handle Input (based on the state)
	switch( currState )
    {
        case TT_TITLE:
        {
			ttTitleInput();
            break;
        }
        case TT_GAME:
        {
			ttGameInput();
            break;
        }
        case TT_SCORES:
        {
			ttScoresInput();
            break;
        }
        case TT_GAMEOVER:
        {
            ttGameoverInput();
            break;
        }
        default:
            break;
    };

    // Mark what our inputs were the last time we acted on them.
    ttLastButtonState = ttButtonState;
    ttLastAccel = ttAccel;

    // Handle Game Logic (based on the state)
	switch( currState )
    {
        case TT_TITLE:
        {
			ttTitleUpdate();
            break;
        }
        case TT_GAME:
        {
			ttGameUpdate();
            break;
        }
        case TT_SCORES:
        {
			ttScoresUpdate();
            break;
        }
        case TT_GAMEOVER:
        {
            ttGameoverUpdate();
            break;
        }
        default:
            break;
    };

	// Handle Drawing Frame (based on the state)
	switch( currState )
    {
        case TT_TITLE:
        {
			ttTitleDisplay();
            break;
        }
        case TT_GAME:
        {
			ttGameDisplay();
            break;
        }
        case TT_SCORES:
        {
			ttScoresDisplay();
            break;
        }
        case TT_GAMEOVER:
        {
            ttGameoverDisplay();
            break;
        }
        default:
            break;
    };
}

void ICACHE_FLASH_ATTR ttTitleInput(void)
{   
    //button a = start game
    if(ttIsButtonPressed(BTN_TITLE_START_GAME))
    {
        ttChangeState(TT_GAME);
    }
    //button b = go to score screen
    else if(ttIsButtonPressed(BTN_TITLE_START_SCORES))
    {
        ttChangeState(TT_SCORES);
    }

    //accel = tilt something on screen like you would a tetrad.
    moveTetrad(&tutorialTetrad, TUTORIAL_GRID_WIDTH, TUTORIAL_GRID_HEIGHT, tutorialTetradsGrid);
}

void ICACHE_FLASH_ATTR ttGameInput(void)
{
    // Refresh the tetrads grid.
    refreshTetradsGrid(GRID_WIDTH, GRID_HEIGHT, tetradsGrid, landedTetrads, &(activeTetrad), false);

    // Only respond to input when the clear animation isn't running.
    if (!inClearAnimation)
    {
	    //button a = rotate piece
        if(ttIsButtonPressed(BTN_GAME_ROTATE))
        {
            rotateTetrad(&activeTetrad, GRID_WIDTH, GRID_HEIGHT, tetradsGrid);
        }

    #ifdef NO_STRESS_TRIS
        if(ttIsButtonPressed(BTN_GAME_DROP))
        {
            dropTimer = dropTime;
        }
    #else
        //button b = soft drop piece
        if(ttIsButtonDown(BTN_GAME_DROP))
        {
            softDropTetrad();
        }
    #endif

        // Only move tetrads left and right when the fast drop button isn't being held down.
        if(ttIsButtonUp(BTN_GAME_DROP))
        {
            moveTetrad(&activeTetrad, GRID_WIDTH, GRID_HEIGHT, tetradsGrid);
        }
    }
}

void ICACHE_FLASH_ATTR ttScoresInput(void)
{
	//button a = hold to clear scores.
    if(holdingClearScore && ttIsButtonDown(BTN_SCORES_CLEAR_SCORES))
    {
        clearScoreTimer += deltaTime;
        if (clearScoreTimer >= CLEAR_SCORES_HOLD_TIME)
        {
            clearScoreTimer = 0;
            memset(highScores, 0, NUM_TT_HIGH_SCORES * sizeof(uint32_t));
            saveHighScores();
            loadHighScores();
            ttSetLastScore(0);

            char uiStr[32] = {0};
            uint8_t x0 = 0;
            uint8_t x1 = OLED_WIDTH - 1;
            ets_snprintf(uiStr, sizeof(uiStr), "1. %d", highScores[0]);
            score0X = getCenteredTextX(x0, x1, uiStr, TOM_THUMB);
            ets_snprintf(uiStr, sizeof(uiStr), "2. %d", highScores[1]);
            score1X = getCenteredTextX(x0, x1, uiStr, TOM_THUMB);
            ets_snprintf(uiStr, sizeof(uiStr), "3. %d", highScores[2]);
            score2X = getCenteredTextX(x0, x1, uiStr, TOM_THUMB);
            ets_snprintf(uiStr, sizeof(uiStr), "YOUR LAST SCORE: %d", ttGetLastScore());
            lastScoreX = getCenteredTextX(x0, x1, uiStr, TOM_THUMB);
        }
    }
    else if(ttIsButtonUp(BTN_SCORES_CLEAR_SCORES))
    {
        clearScoreTimer = 0;
    }
    // This is added to prevent people holding left from the previous screen from accidentally clearing their scores.
    else if(ttIsButtonPressed(BTN_SCORES_CLEAR_SCORES))
    {
        holdingClearScore = true;
    }
    
    //button b = go to title screen
    if(ttIsButtonPressed(BTN_SCORES_START_TITLE))
    {
        ttChangeState(TT_TITLE);
    }
}

void ICACHE_FLASH_ATTR ttGameoverInput(void)
{
    //button a = start game
    if(ttIsButtonPressed(BTN_GAMEOVER_START_GAME))
    {
        ttChangeState(TT_GAME);
    }
    //button b = go to title screen
    else if(ttIsButtonPressed(BTN_GAMEOVER_START_TITLE))
    {
        ttChangeState(TT_TITLE);
    }
}

void ICACHE_FLASH_ATTR ttTitleUpdate(void)
{
    //Refresh the tetrads grid.
    refreshTetradsGrid(TUTORIAL_GRID_WIDTH, TUTORIAL_GRID_HEIGHT, tutorialTetradsGrid, landedTetrads, &(tutorialTetrad), false);
    
    dropTimer += deltaTime;

    if (dropTimer >= dropTime)
    {
        dropTimer = 0;        

        // If we couldn't drop, then we've landed.
        if (!dropTetrad(&tutorialTetrad, TUTORIAL_GRID_WIDTH, TUTORIAL_GRID_HEIGHT, tutorialTetradsGrid))
        {
            //TODO: have the piece completely removed and replaced with a new falling random piece
          

            // Spawn the next tetrad.
            spawnNextTetrad(&tutorialTetrad, BAG, 1, TUTORIAL_GRID_WIDTH, TUTORIAL_GRID_HEIGHT, tutorialTetradsGrid);

            // Reset the drop info to whatever is appropriate for the current level.
            dropTime = getDropTime(TITLE_LEVEL);
            dropTimer = 0;
        }
    }
}

void ICACHE_FLASH_ATTR ttGameUpdate(void)
{
    //Refresh the tetrads grid.
    refreshTetradsGrid(GRID_WIDTH, GRID_HEIGHT, tetradsGrid, landedTetrads, &(activeTetrad), false);

    // land tetrad
    // update score
    // start clear animation
    // end clear animation
    // clear lines
    // spawn new active tetrad.

    if (inClearAnimation) 
    {
        clearTimer += deltaTime;

        if (clearTimer >= clearTime)
        {
            stopClearAnimation();

            // Actually clear the lines.
            clearLines(GRID_WIDTH, GRID_HEIGHT, tetradsGrid, landedTetrads);

            // Spawn the next tetrad.
            spawnNextTetrad(&activeTetrad, randomizer, tetradCounter+1, GRID_WIDTH, GRID_HEIGHT, tetradsGrid);

            // Reset the drop info to whatever is appropriate for the current level.
            dropTime = getDropTime(currentLevel);
            dropTimer = 0;
        }
    }
    else
    {
    #ifndef NO_STRESS_TRIS
        dropTimer += deltaTime;
    #endif

        if (dropTimer >= dropTime)
        {
            dropTimer = 0;

            if (ttIsButtonDown(BTN_GAME_DROP))
            {
                score += SCORE_SOFT_DROP;
            }

            // If we couldn't drop, then we've landed.
            if (!dropTetrad(&(activeTetrad), GRID_WIDTH, GRID_HEIGHT, tetradsGrid))
            {
                // Land the current tetrad.
                tetrad_t * landedTetrad = malloc(sizeof(tetrad_t));
                landedTetrad->type = activeTetrad.type;
                landedTetrad->rotation = activeTetrad.rotation;
                landedTetrad->topLeft = activeTetrad.topLeft;

                coord_t origin;
                origin.c = 0;
                origin.r = 0;
		        copyGrid(origin, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, activeTetrad.shape, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, landedTetrad->shape);

                push(landedTetrads, landedTetrad);

                tetradCounter++;
                
                // Check for any clears now that the new tetrad has landed.
                uint32_t linesClearedThisDrop = checkLineClears(GRID_WIDTH, GRID_HEIGHT, tetradsGrid, landedTetrads);

                switch( linesClearedThisDrop )
                {
                    case 1: 
                        score += SCORE_SINGLE * (currentLevel+1);
                        break;
                    case 2:
                        score += SCORE_DOUBLE * (currentLevel+1);
                        break;
                    case 3:
                        score += SCORE_TRIPLE * (currentLevel+1);
                        break;
                    case 4:
                        score += SCORE_QUAD * (currentLevel+1);
                        break;
                    default:    // Are more than 4 line clears possible? I don't think so.
                        break;
                }


                // TODO: What is the best way to handle the scoring behavior. Combo only on end, building combo?
                // TODO: Is a clear of 2 lines followed by a clear of 2 lines a combo of 4 or 2?

                // This code assumes building combo, and combos are sums of lines cleared.
                if (linesClearedLastDrop > 0 && linesClearedThisDrop > 0)
                {
                    comboCount += linesClearedThisDrop;
                    score += SCORE_COMBO * comboCount * (currentLevel+1);
                }
                else
                {
                    comboCount = 0;
                }

                // Increase total number of lines cleared.
                linesClearedTotal += linesClearedThisDrop;

                // Update the level if necessary.
                currentLevel = linesClearedTotal / LINE_CLEARS_PER_LEVEL;

                // Keep track of the last number of line clears.
                linesClearedLastDrop = linesClearedThisDrop;

                if (linesClearedThisDrop > 0)
                {
                    // Start the clear animation.
                    startClearAnimation(linesClearedThisDrop);
                }
                else
                {
                    // Spawn the next tetrad.
                    spawnNextTetrad(&activeTetrad, randomizer, tetradCounter+1, GRID_WIDTH, GRID_HEIGHT, tetradsGrid);

                    // Reset the drop info to whatever is appropriate for the current level.
                    dropTime = getDropTime(currentLevel);
                    dropTimer = 0;
                }
            }
            
            // Clear out empty tetrads.
            node_t * current = landedTetrads->last;
            for (int t = landedTetrads->length - 1; t >= 0; t--)
            {
                tetrad_t * currentTetrad = (tetrad_t *)current->val;
                bool empty = true;

                // Go from bottom-to-top on each position of the tetrad.
                for (int tr = TETRAD_GRID_SIZE - 1; tr >= 0; tr--) 
                {
                    for (int tc = 0; tc < TETRAD_GRID_SIZE; tc++) 
                    {
                        if (currentTetrad->shape[tr][tc] != EMPTY) empty = false;
                    }
                }

                // Adjust the current counter.
                current = current->prev;
                
                // Remove the empty tetrad.
                if (empty)
                {
                    tetrad_t * emptyTetrad = remove(landedTetrads, t);
                    free(emptyTetrad);
                }
            }

            refreshTetradsGrid(GRID_WIDTH, GRID_HEIGHT, tetradsGrid, landedTetrads, &(activeTetrad), false);

            // TODO: Do I want to actually do this? This will need update for linked list version.
            // Handle cascade from tetrads that can now fall freely.
            /*bool possibleCascadeClear = false;
            for (int t = 0; t < numLandedTetrads; t++) 
            {
                // If a tetrad could drop, then more clears might have happened.
                if (dropTetrad(&(landedTetrads[t]), GRID_WIDTH, GRID_HEIGHT, tetradsGrid))
                {   
                    possibleCascadeClear = true;
                }
            }


            if (possibleCascadeClear)
            {
                // Check for any clears now that this new
                checkLineClears(GRID_WIDTH, GRID_HEIGHT, tetradsGrid, landedTetrads);
            }*/
        }
    }
}

void ICACHE_FLASH_ATTR ttScoresUpdate(void)
{
    // Do nothing.
}

void ICACHE_FLASH_ATTR ttGameoverUpdate(void)
{
    // Do nothing.
}

void ICACHE_FLASH_ATTR ttTitleDisplay(void)
{
	// Clear the display.
    clearDisplay();
    
    // Test drawing some demo-scene type lines for effect.

    // Vertical Lines
    uint8_t leftLineXStart = GRID_X;
    uint8_t rightLineXStart = GRID_X + (GRID_UNIT_SIZE - 1) * GRID_WIDTH;

    double lineSpeed = 1.0;
    
    int firstLineProgressUS = stateTime % (int)(lineSpeed * S_TO_MS_FACTOR * MS_TO_US_FACTOR);
    double firstLineProgress = (double)firstLineProgressUS / (double)(lineSpeed * S_TO_MS_FACTOR * MS_TO_US_FACTOR);

    int secondLineProgressUS = (int)(stateTime + ((lineSpeed/2) * S_TO_MS_FACTOR * MS_TO_US_FACTOR)) % (int)(lineSpeed * S_TO_MS_FACTOR * MS_TO_US_FACTOR);
    double secondLineProgress = (double)secondLineProgressUS / (double)(lineSpeed * S_TO_MS_FACTOR * MS_TO_US_FACTOR);

    uint8_t leftLineXProgress = firstLineProgress * GRID_X;
    uint8_t rightLineXProgress = firstLineProgress * (OLED_WIDTH - rightLineXStart);
    plotLine(leftLineXStart - leftLineXProgress, 0, leftLineXStart - leftLineXProgress, OLED_HEIGHT, WHITE);
    plotLine(rightLineXStart + rightLineXProgress, 0, rightLineXStart + rightLineXProgress, OLED_HEIGHT, WHITE);

    leftLineXProgress = secondLineProgress * GRID_X;
    rightLineXProgress = secondLineProgress * (OLED_WIDTH - rightLineXStart);
    plotLine(leftLineXStart - leftLineXProgress, 0, leftLineXStart - leftLineXProgress, OLED_HEIGHT, WHITE);
    plotLine(rightLineXStart + rightLineXProgress, 0, rightLineXStart + rightLineXProgress, OLED_HEIGHT, WHITE);


    // Horizontal Lines

    uint8_t midY = OLED_HEIGHT / 2;
    
    uint8_t lineOneStartY = OLED_HEIGHT / 4;
    uint8_t lineOneEndY = 0;

    uint8_t lineTwoStartY = 3 * (OLED_HEIGHT / 4);
    uint8_t lineTwoEndY = OLED_HEIGHT;

    // Left
    plotLine(0, midY, GRID_X, midY, WHITE);
    plotLine(0, lineOneEndY, GRID_X, lineOneStartY, WHITE);
    plotLine(0, lineTwoEndY, GRID_X, lineTwoStartY, WHITE);

    // Right
    plotLine(rightLineXStart, midY, OLED_WIDTH, midY, WHITE);
    plotLine(rightLineXStart, lineOneStartY, OLED_WIDTH, lineOneEndY, WHITE);
    plotLine(rightLineXStart, lineTwoStartY, OLED_WIDTH, lineTwoEndY, WHITE);

    // SCORES   START
    // TODO: make these #defines
    uint8_t scoresAreaX0 = 0;
    uint8_t scoresAreaY0 = OLED_HEIGHT - (FONT_HEIGHT_TOMTHUMB + 3);
    uint8_t scoresAreaX1 = 23;//getTextWidth("SCORES", TOM_THUMB);
    uint8_t scoresAreaY1 = OLED_HEIGHT - 1; //minus 1 is not here.
    fillDisplayArea(scoresAreaX0, scoresAreaY0, scoresAreaX1, scoresAreaY1, BLACK);
    uint8_t scoresTextX = 0;
    uint8_t scoresTextY = OLED_HEIGHT - (FONT_HEIGHT_TOMTHUMB + 1);
    plotText(scoresTextX, scoresTextY, "SCORES", TOM_THUMB, WHITE);
    
    uint8_t startAreaX0 = OLED_WIDTH - 19 - 2;//getTextWidth("START", TOM_THUMB) - 2;
    uint8_t startAreaY0 = OLED_HEIGHT - (FONT_HEIGHT_TOMTHUMB + 3);
    uint8_t startAreaX1 = OLED_WIDTH - 1; //minus 1 is not here.
    uint8_t startAreaY1 = OLED_HEIGHT - 1; //minus 1 is not here.
    fillDisplayArea(startAreaX0, startAreaY0, startAreaX1, startAreaY1, BLACK);
    uint8_t startTextX = OLED_WIDTH - 19;//getTextWidth("START", TOM_THUMB);
    uint8_t startTextY = OLED_HEIGHT - (FONT_HEIGHT_TOMTHUMB + 1);    
    plotText(startTextX, startTextY, "START", TOM_THUMB, WHITE);

    // Clear the grid data (may not want to do this every frame)
    refreshTetradsGrid(TUTORIAL_GRID_WIDTH, TUTORIAL_GRID_HEIGHT, tutorialTetradsGrid, landedTetrads, &(tutorialTetrad), true);

    // Draw the active tetrad.
    plotShape(GRID_X + tutorialTetrad.topLeft.c * (GRID_UNIT_SIZE - 1), GRID_Y + tutorialTetrad.topLeft.r * (GRID_UNIT_SIZE - 1), GRID_UNIT_SIZE, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, tutorialTetrad.shape, WHITE);

    // Draw the background grid. NOTE: (make sure everything that needs to be in tetradsGrid is in there now).
    plotGrid(GRID_X, GRID_Y, GRID_UNIT_SIZE, TUTORIAL_GRID_WIDTH, TUTORIAL_GRID_HEIGHT, tutorialTetradsGrid, false, WHITE);

    // TILTRADS

    // getTextWidth("TILTRADS", RADIOSTARS) = 87
    // textEnd = 109

    uint8_t titleAreaX0 = 109 - 87 - 2;
    uint8_t titleAreaY0 = (OLED_HEIGHT / 2) - FONT_HEIGHT_RADIOSTARS - 3;
    uint8_t titleAreaX1 = 109 - 1; 
    uint8_t titleAreaY1 = (OLED_HEIGHT / 2) - 1; 
    fillDisplayArea(titleAreaX0, titleAreaY0, titleAreaX1, titleAreaY1, BLACK);
    
    uint8_t titleTextX = 21;
    uint8_t titleTextY = (OLED_HEIGHT / 2) - FONT_HEIGHT_RADIOSTARS - 2;
    plotText(titleTextX, titleTextY, "TILTRADS", RADIOSTARS, WHITE);

    /*char accelStr[32] = {0};
    ets_snprintf(accelStr, sizeof(accelStr), "WT:%d", textEnd);
    plotText(0, 0, accelStr, IBM_VGA_8, WHITE);*/

    // Display the acceleration on the display
    /*char accelStr[32] = {0};

    ets_snprintf(accelStr, sizeof(accelStr), "L:%d", leftLineXProgress);
    plotText(0, OLED_HEIGHT - (3 * (FONT_HEIGHT_IBMVGA8 + 1)), accelStr, IBM_VGA_8, WHITE);

    ets_snprintf(accelStr, sizeof(accelStr), "R:%d", rightLineXProgress);
    plotText(0, OLED_HEIGHT - (2 * (FONT_HEIGHT_IBMVGA8 + 1)), accelStr, IBM_VGA_8, WHITE);*/

    /*ets_snprintf(accelStr, sizeof(accelStr), "Z:%d", ttAccel.z);
    plotText(0, OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)), accelStr, IBM_VGA_8, WHITE);*/

    // Draw text in the bottom corners to direct which buttons will do what.
    //drawPixel(OLED_WIDTH - 1, OLED_HEIGHT-15, WHITE);
    //drawPixel(0, OLED_HEIGHT-15, WHITE);
}

void ICACHE_FLASH_ATTR ttGameDisplay(void)
{
    // Clear the display
    clearDisplay();

    // Draw the active tetrad.
    plotShape(GRID_X + activeTetrad.topLeft.c * (GRID_UNIT_SIZE - 1), GRID_Y + activeTetrad.topLeft.r * (GRID_UNIT_SIZE - 1), GRID_UNIT_SIZE, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, activeTetrad.shape, WHITE);   
    
    // Draw all the landed tetrads.
    node_t * current = landedTetrads->first;
    for (int t = 0; t < landedTetrads->length; t++) 
    {
        tetrad_t * currentTetrad = (tetrad_t *)current->val;
        plotShape(GRID_X + currentTetrad->topLeft.c * (GRID_UNIT_SIZE - 1), GRID_Y + currentTetrad->topLeft.r * (GRID_UNIT_SIZE - 1), GRID_UNIT_SIZE, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, currentTetrad->shape, WHITE);
        current = current->next;
    }

    // Clear the grid data (may not want to do this every frame)
    refreshTetradsGrid(GRID_WIDTH, GRID_HEIGHT, tetradsGrid, landedTetrads, &(activeTetrad), true);

    // Draw the background grid. NOTE: (make sure everything that needs to be in tetradsGrid is in there now).
    plotGrid(GRID_X, GRID_Y, GRID_UNIT_SIZE, GRID_WIDTH, GRID_HEIGHT, tetradsGrid, inClearAnimation, WHITE);
    //clearAnimFrame = !clearAnimFrame;
    
    // NEXT
    uint8_t nextHeaderTextX = 103;
    plotText(nextHeaderTextX, 0, "NEXT", TOM_THUMB, WHITE);
    //plotCenteredText(GRID_X + (GRID_UNIT_SIZE * GRID_WIDTH), 0, OLED_WIDTH, "NEXT", TOM_THUMB, WHITE);

    // Draw the next tetrad.
    coord_t nextTetradPoint;
    nextTetradPoint.c = 1;
    nextTetradPoint.r = 1;
    tetrad_t nextTetrad = spawnTetrad(nextTetradType, tetradCounter+1, nextTetradPoint, 0);
    plotShape(NEXT_GRID_X + nextTetradPoint.c * (GRID_UNIT_SIZE - 1), NEXT_GRID_Y + nextTetradPoint.r * (GRID_UNIT_SIZE - 1), GRID_UNIT_SIZE, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, nextTetrad.shape, WHITE);

    // Draw the grid holding the next tetrad.
    clearGrid(NEXT_GRID_WIDTH, NEXT_GRID_HEIGHT, nextTetradGrid);
    copyGrid(nextTetrad.topLeft, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, nextTetrad.shape, NEXT_GRID_WIDTH, NEXT_GRID_HEIGHT, nextTetradGrid);
    plotGrid(NEXT_GRID_X, NEXT_GRID_Y, GRID_UNIT_SIZE, NEXT_GRID_WIDTH, NEXT_GRID_HEIGHT, nextTetradGrid, false, WHITE);

    // Draw the UI.
    char uiStr[32] = {0};

    newHighScore = score > highScores[0];
    
    //HIGH
    uint8_t highScoreHeaderTextX = newHighScore ? 3 : 15;
    plotText(highScoreHeaderTextX, 0, newHighScore ? "HIGH (NEW!)" : "HIGH", TOM_THUMB, WHITE);
    //plotCenteredText(0, 0, GRID_X, newHighScore ? "HIGH (NEW!)" : "HIGH", TOM_THUMB, WHITE);
    //99999    
    ets_snprintf(uiStr, sizeof(uiStr), "%d", newHighScore ? score : highScores[0]);
    plotCenteredText(0, (FONT_HEIGHT_TOMTHUMB + 1), GRID_X, uiStr, TOM_THUMB, WHITE);

    //SCORE
    uint8_t scoreHeaderTextX = 13;
    plotText(scoreHeaderTextX, (3*FONT_HEIGHT_TOMTHUMB), "SCORE", TOM_THUMB, WHITE);
    //plotCenteredText(0, (3*FONT_HEIGHT_TOMTHUMB), GRID_X, "SCORE", TOM_THUMB, WHITE);
    //99999
    ets_snprintf(uiStr, sizeof(uiStr), "%d", score);
    plotCenteredText(0, (4*FONT_HEIGHT_TOMTHUMB)+1, GRID_X, uiStr, TOM_THUMB, WHITE);

    //LINES
    uint8_t linesHeaderTextX = 13;
    plotText(linesHeaderTextX, (6*FONT_HEIGHT_TOMTHUMB), "LINES", TOM_THUMB, WHITE);    
    //plotCenteredText(0, (6*FONT_HEIGHT_TOMTHUMB), GRID_X, "LINES", TOM_THUMB, WHITE);
    // 999
    ets_snprintf(uiStr, sizeof(uiStr), "%d", linesClearedTotal);
    plotCenteredText(0, (7*FONT_HEIGHT_TOMTHUMB)+1, GRID_X, uiStr, TOM_THUMB, WHITE);

    //LEVEL
    uint8_t levelHeaderTextX = 13;
    plotText(levelHeaderTextX, (9*FONT_HEIGHT_TOMTHUMB), "LEVEL", TOM_THUMB, WHITE);
    //plotCenteredText(0, (9*FONT_HEIGHT_TOMTHUMB), GRID_X, "LEVEL", TOM_THUMB, WHITE);
    // 99
    ets_snprintf(uiStr, sizeof(uiStr), "%d", (currentLevel+1)); // Levels are displayed with 1 as the base level.
    plotCenteredText(0, (10*FONT_HEIGHT_TOMTHUMB)+1, GRID_X, uiStr, TOM_THUMB, WHITE);


    // Debug: FPS counter
    /*double seconds = ((double)stateTime * (double)US_TO_MS_FACTOR * (double)MS_TO_S_FACTOR);
    int fps = (int)((double)stateFrames / seconds);
    ets_snprintf(uiStr, sizeof(uiStr), "FPS: %d", fps);
    plotText(OLED_WIDTH - getTextWidth(uiStr, TOM_THUMB) - 1, OLED_HEIGHT - (1 * (FONT_HEIGHT_TOMTHUMB + 1)), uiStr, TOM_THUMB, WHITE);*/
}

void ICACHE_FLASH_ATTR ttScoresDisplay(void)
{
    // Clear the display
    clearDisplay();

    // HIGH SCORES
    uint8_t headerTextX = 22;
    uint8_t headerTextY = 0;
    plotText(headerTextX, headerTextY, "HIGH SCORES", IBM_VGA_8, WHITE);

    char uiStr[32] = {0};
    // 1. 99999
    ets_snprintf(uiStr, sizeof(uiStr), "1. %d", highScores[0]);
    plotText(score0X, (3*FONT_HEIGHT_TOMTHUMB)+1, uiStr, TOM_THUMB, WHITE);
    //plotCenteredText(0, (3*FONT_HEIGHT_TOMTHUMB)+1, OLED_WIDTH - 1, uiStr, TOM_THUMB, WHITE);

    // 2. 99999
    ets_snprintf(uiStr, sizeof(uiStr), "2. %d", highScores[1]);
    plotText(score1X, (5*FONT_HEIGHT_TOMTHUMB)+1, uiStr, TOM_THUMB, WHITE);
    //plotCenteredText(0, (5*FONT_HEIGHT_TOMTHUMB)+1, OLED_WIDTH - 1, uiStr, TOM_THUMB, WHITE);

    // 3. 99999
    ets_snprintf(uiStr, sizeof(uiStr), "3. %d", highScores[2]);
    plotText(score2X, (7*FONT_HEIGHT_TOMTHUMB)+1, uiStr, TOM_THUMB, WHITE);
    //plotCenteredText(0, (7*FONT_HEIGHT_TOMTHUMB)+1, OLED_WIDTH - 1, uiStr, TOM_THUMB, WHITE);

    // YOUR LAST SCORE:
    ets_snprintf(uiStr, sizeof(uiStr), "YOUR LAST SCORE: %d", ttGetLastScore());
    plotText(lastScoreX, (9*FONT_HEIGHT_TOMTHUMB)+1, uiStr, TOM_THUMB, WHITE);
    //plotCenteredText(0, (9*FONT_HEIGHT_TOMTHUMB)+1, OLED_WIDTH - 1, uiStr, TOM_THUMB, WHITE);
    

    // CLEAR SCORES
    uint8_t clearScoresTextX = 1;
    uint8_t clearScoresTextY = OLED_HEIGHT - (FONT_HEIGHT_TOMTHUMB + 1);
    plotText(clearScoresTextX, clearScoresTextY, "CLEAR SCORES", TOM_THUMB, WHITE);

    // fill the clear scores area depending on how long the button's held down.
    if (clearScoreTimer != 0)
    {
        double holdProgress = ((double)clearScoreTimer / (double)CLEAR_SCORES_HOLD_TIME);
        uint8_t holdAreaX0 = 0;
        uint8_t holdAreaY0 = (OLED_HEIGHT - (FONT_HEIGHT_TOMTHUMB + 1)) - 1;
        uint8_t holdAreaX1 = (uint8_t)(holdProgress * (47+2));
        uint8_t holdAreaY1 = OLED_HEIGHT - 1;
        fillDisplayArea(holdAreaX0, holdAreaY0, holdAreaX1, holdAreaY1, INVERSE);
    }

    // TITLE
    uint8_t titleTextX = OLED_WIDTH - 19 - 1;
    uint8_t titleTextY = OLED_HEIGHT - (FONT_HEIGHT_TOMTHUMB + 1);
    plotText(titleTextX, titleTextY, "TITLE", TOM_THUMB, WHITE);
}

void ICACHE_FLASH_ATTR ttGameoverDisplay(void)
{
    // We don't clear the display because we want the playfield to appear in the background.

    // Flash the active tetrad that was the killing tetrad.
    if (drawGameoverTetrad)
    {
        plotShape(GRID_X + activeTetrad.topLeft.c * (GRID_UNIT_SIZE - 1), GRID_Y + activeTetrad.topLeft.r * (GRID_UNIT_SIZE - 1), GRID_UNIT_SIZE, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, activeTetrad.shape, WHITE);
        drawGameoverTetrad = false;
    }

    plotShape(GRID_X + activeTetrad.topLeft.c * (GRID_UNIT_SIZE - 1), GRID_Y + activeTetrad.topLeft.r * (GRID_UNIT_SIZE - 1), GRID_UNIT_SIZE, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, activeTetrad.shape, INVERSE);

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
    plotText(29, windowYMarginTop + titleTextYOffset, "GAME OVER", IBM_VGA_8, WHITE);
    //plotCenteredText(windowXMargin, windowYMarginTop + titleTextYOffset, OLED_WIDTH - windowXMargin, "GAME OVER", IBM_VGA_8, WHITE);

    // HIGH SCORE! or YOUR SCORE:
    if (newHighScore)
    {
        plotText(44, windowYMarginTop + highScoreTextYOffset, "HIGH SCORE!", TOM_THUMB, WHITE);
        //plotCenteredText(windowXMargin, windowYMarginTop + highScoreTextYOffset, OLED_WIDTH - windowXMargin, "HIGH SCORE!", TOM_THUMB, WHITE);
    }
    else
    {
        plotText(44, windowYMarginTop + highScoreTextYOffset, "YOUR SCORE:", TOM_THUMB, WHITE);
        //plotCenteredText(windowXMargin, windowYMarginTop + highScoreTextYOffset, OLED_WIDTH - windowXMargin, "YOUR SCORE:", TOM_THUMB, WHITE);
    }

    // 1230495
    char scoreStr[32] = {0};
    ets_snprintf(scoreStr, sizeof(scoreStr), "%d", score);
    plotText(gameoverScoreX, windowYMarginTop + scoreTextYOffset, scoreStr, IBM_VGA_8, WHITE);
    //plotCenteredText(windowXMargin, windowYMarginTop + scoreTextYOffset, OLED_WIDTH - windowXMargin, scoreStr, IBM_VGA_8, WHITE);

    // TITLE    RESTART
    plotText(windowXMargin + controlTextXPadding, controlTextYOffset, "TITLE", TOM_THUMB, WHITE);
    plotText(OLED_WIDTH - windowXMargin - 27 - controlTextXPadding, controlTextYOffset, "RESTART", TOM_THUMB, WHITE);
}

// helper functions.

void ICACHE_FLASH_ATTR ttChangeState(tiltradsState_t newState)
{
	currState = newState;
	stateStartTime = system_get_time();
	stateTime = 0;
    stateFrames = 0;

    // Used for cache of ui anchors.
    uint8_t x0 = 0;
    uint8_t x1 = 0;
    char uiStr[32] = {0};

    switch( currState )
    {
        case TT_TITLE:
            
            clearLandedTetrads();

            // Get a random tutorial tetrad.
            initTetradRandomizer(BAG);
            nextTetradType = (tetradType_t)getNextTetradType(BAG, 0);
            clearGrid(GRID_WIDTH, GRID_HEIGHT, tutorialTetradsGrid);
            spawnNextTetrad(&tutorialTetrad, BAG, 1, TUTORIAL_GRID_WIDTH, TUTORIAL_GRID_HEIGHT, tutorialTetradsGrid);

            // Reset the drop info to whatever is appropriate for the current level.
            dropTime = getDropTime(TITLE_LEVEL);
            dropTimer = 0;

            
            break;
        case TT_GAME:
            // All game restart functions happen here.
            clearGrid(GRID_WIDTH, GRID_HEIGHT, tetradsGrid);
            clearGrid(NEXT_GRID_WIDTH, NEXT_GRID_HEIGHT, nextTetradGrid);
            tetradCounter = 0;
            clearLandedTetrads();
            linesClearedTotal = 0;
            linesClearedLastDrop = 0;
            comboCount = 0;
            currentLevel = 0;
            score = 0;
            loadHighScores();
            // TODO: should I be seeding this, or re-seeding this, and if so, with what?
            srand((uint32_t)(ttAccel.x + ttAccel.y * 3 + ttAccel.z * 5)); // Seed the random number generator.
            initTetradRandomizer(randomizer);
            nextTetradType = (tetradType_t)getNextTetradType(randomizer, tetradCounter);
            spawnNextTetrad(&activeTetrad, randomizer, tetradCounter+1, GRID_WIDTH, GRID_HEIGHT, tetradsGrid);
            // Reset the drop info to whatever is appropriate for the current level.
            dropTime = getDropTime(currentLevel);
            dropTimer = 0;

            // Reset animation info.
            stopClearAnimation();

            break;
        case TT_SCORES:
            loadHighScores();

            x0 = 0;
            x1 = OLED_WIDTH - 1;
            ets_snprintf(uiStr, sizeof(uiStr), "1. %d", highScores[0]);
            score0X = getCenteredTextX(x0, x1, uiStr, TOM_THUMB);
            ets_snprintf(uiStr, sizeof(uiStr), "2. %d", highScores[1]);
            score1X = getCenteredTextX(x0, x1, uiStr, TOM_THUMB);
            ets_snprintf(uiStr, sizeof(uiStr), "3. %d", highScores[2]);
            score2X = getCenteredTextX(x0, x1, uiStr, TOM_THUMB);
            ets_snprintf(uiStr, sizeof(uiStr), "YOUR LAST SCORE: %d", ttGetLastScore());
            lastScoreX = getCenteredTextX(x0, x1, uiStr, TOM_THUMB);

            clearScoreTimer = 0;
            holdingClearScore = false;
            break;
        case TT_GAMEOVER:
            // Update high score if needed.
            newHighScore = updateHighScores(score);
            if (newHighScore) saveHighScores();
            
            // Save out the last score.
            ttSetLastScore(score);

            // Set var for gameover tetrad effect.
            drawGameoverTetrad = true;

            // Get the correct offset for the high score.
            x0 = 18;
            x1 = OLED_WIDTH - x0;
            ets_snprintf(uiStr, sizeof(uiStr), "%d", score);
            gameoverScoreX = getCenteredTextX(x0, x1, uiStr, IBM_VGA_8);
            break;
        default:
            break;
    };
}

bool ICACHE_FLASH_ATTR ttIsButtonPressed(uint8_t button)
{
    //TODO: can btn events get lost this way?
    return (ttButtonState & button) && !(ttLastButtonState & button);
}

bool ICACHE_FLASH_ATTR ttIsButtonReleased(uint8_t button)
{
    //TODO: can btn events get lost this way?
    return !(ttButtonState & button) && (ttLastButtonState & button);
}

bool ICACHE_FLASH_ATTR ttIsButtonDown(uint8_t button)
{
    //TODO: can btn events get lost this way?
    return ttButtonState & button;
}

bool ICACHE_FLASH_ATTR ttIsButtonUp(uint8_t button)
{
    //TODO: can btn events get lost this way?
    return !(ttButtonState & button);
}

void ICACHE_FLASH_ATTR copyGrid(coord_t srcOffset, uint8_t srcWidth, uint8_t srcHeight, uint32_t src[][srcWidth], uint8_t dstWidth, uint8_t dstHeight, uint32_t dst[][dstWidth])
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

void ICACHE_FLASH_ATTR transferGrid(coord_t srcOffset, uint8_t srcWidth, uint8_t srcHeight, uint32_t src[][srcWidth], uint8_t dstWidth, uint8_t dstHeight, uint32_t dst[][dstWidth], uint32_t transferVal)
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

void ICACHE_FLASH_ATTR clearGrid(uint8_t gridWidth, uint8_t gridHeight, uint32_t gridData[][gridWidth])
{
    for (int y = 0; y < gridHeight; y++)
    {
        for (int x = 0; x < gridWidth; x++)
        {
            gridData[y][x] = EMPTY;
        }
    }
}

// NOTE: the grid value of every tetrad is reassigned on refresh to fix a bug that occurs where every 3 tetrads seems to ignore collision, cause unknown.
void ICACHE_FLASH_ATTR refreshTetradsGrid(uint8_t gridWidth, uint8_t gridHeight, uint32_t gridData[][gridWidth], list_t * fieldTetrads, tetrad_t * movingTetrad, bool includeMovingTetrad)
{
    clearGrid(gridWidth, gridHeight, gridData);

    node_t * current = fieldTetrads->first;
    for (int t = 0; t < fieldTetrads->length; t++) 
    {
        tetrad_t * currentTetrad = (tetrad_t *)current->val;
        currentTetrad->gridValue = t+1;
        transferGrid(currentTetrad->topLeft, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, currentTetrad->shape, gridWidth, gridHeight, gridData, currentTetrad->gridValue); 
        current = current->next;
    }

    if (includeMovingTetrad)
    {
        movingTetrad->gridValue = fieldTetrads->length+1;
        transferGrid(movingTetrad->topLeft, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, movingTetrad->shape, gridWidth, gridHeight, gridData, movingTetrad->gridValue);
    }
}

// This assumes only complete tetrads can be rotated.
void ICACHE_FLASH_ATTR rotateTetrad(tetrad_t * tetrad, uint8_t gridWidth, uint8_t gridHeight, uint32_t gridData[][gridWidth])
{
    int newRotation = tetrad->rotation + 1;
    bool rotationClear = false;

    switch (tetrad->type)
    {
        case I_TETRAD:
            for (int i = 0; i < 5; i++)
            {
                if (!rotationClear)
                {
                    coord_t testPoint;
                    testPoint.r = tetrad->topLeft.r + iTetradRotationTests[tetrad->rotation % 4][i].r;
                    testPoint.c = tetrad->topLeft.c + iTetradRotationTests[tetrad->rotation % 4][i].c;
                    rotationClear = !checkCollision(testPoint, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, iTetradRotations[newRotation % 4], gridWidth, gridHeight, gridData, tetrad->gridValue);
                    if (rotationClear) tetrad->topLeft = testPoint;
                }
            }            
            break;
        case O_TETRAD:
            rotationClear = true;//!checkCollision(tetrad->topLeft, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, oTetradRotations[newRotation % 4], gridWidth, gridHeight, gridData, tetrad->gridValue);
	        break;
        case T_TETRAD:
            for (int i = 0; i < 5; i++)
            {
                if (!rotationClear)
                {
                    coord_t testPoint;
                    testPoint.r = tetrad->topLeft.r + otjlszTetradRotationTests[tetrad->rotation % 4][i].r;
                    testPoint.c = tetrad->topLeft.c + otjlszTetradRotationTests[tetrad->rotation % 4][i].c;
                    rotationClear = !checkCollision(testPoint, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, tTetradRotations[newRotation % 4], gridWidth, gridHeight, gridData, tetrad->gridValue);
                    if (rotationClear) tetrad->topLeft = testPoint;
                }
            }
            break;
        case J_TETRAD:
            for (int i = 0; i < 5; i++)
            {
                if (!rotationClear)
                {
                    coord_t testPoint;
                    testPoint.r = tetrad->topLeft.r + otjlszTetradRotationTests[tetrad->rotation % 4][i].r;
                    testPoint.c = tetrad->topLeft.c + otjlszTetradRotationTests[tetrad->rotation % 4][i].c;
                    rotationClear = !checkCollision(testPoint, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, jTetradRotations[newRotation % 4], gridWidth, gridHeight, gridData, tetrad->gridValue);
                    if (rotationClear) tetrad->topLeft = testPoint;
                }
            }
            break;
        case L_TETRAD:
            for (int i = 0; i < 5; i++)
            {
                if (!rotationClear)
                {
                    coord_t testPoint;
                    testPoint.r = tetrad->topLeft.r + otjlszTetradRotationTests[tetrad->rotation % 4][i].r;
                    testPoint.c = tetrad->topLeft.c + otjlszTetradRotationTests[tetrad->rotation % 4][i].c;
                    rotationClear = !checkCollision(testPoint, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, lTetradRotations[newRotation % 4], gridWidth, gridHeight, gridData, tetrad->gridValue);
                    if (rotationClear) tetrad->topLeft = testPoint;
                }
            }
            break;
        case S_TETRAD:
            for (int i = 0; i < 5; i++)
            {
                if (!rotationClear)
                {
                    coord_t testPoint;
                    testPoint.r = tetrad->topLeft.r + otjlszTetradRotationTests[tetrad->rotation % 4][i].r;
                    testPoint.c = tetrad->topLeft.c + otjlszTetradRotationTests[tetrad->rotation % 4][i].c;
                    rotationClear = !checkCollision(testPoint, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, sTetradRotations[newRotation % 4], gridWidth, gridHeight, gridData, tetrad->gridValue);
                    if (rotationClear) tetrad->topLeft = testPoint;
                }
            }
            break;
        case Z_TETRAD:
            for (int i = 0; i < 5; i++)
            {
                if (!rotationClear)
                {
                    coord_t testPoint;
                    testPoint.r = tetrad->topLeft.r + otjlszTetradRotationTests[tetrad->rotation % 4][i].r;
                    testPoint.c = tetrad->topLeft.c + otjlszTetradRotationTests[tetrad->rotation % 4][i].c;
                    rotationClear = !checkCollision(testPoint, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, zTetradRotations[newRotation % 4], gridWidth, gridHeight, gridData, tetrad->gridValue);
                    if (rotationClear) tetrad->topLeft = testPoint;
                }
            }
            break;
        default:
            break;
    }

    if (rotationClear)
    {
        // Actually rotate the tetrad.
        tetrad->rotation = newRotation;
        coord_t origin;
        origin.c = 0;
        origin.r = 0;
        switch (tetrad->type)
        {
            case I_TETRAD:
			    copyGrid(origin, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, iTetradRotations[tetrad->rotation % 4], TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, tetrad->shape);
                break;
            case O_TETRAD:
		        copyGrid(origin, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, oTetradRotations[tetrad->rotation % 4], TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, tetrad->shape);
                break;
            case T_TETRAD:
		        copyGrid(origin, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, tTetradRotations[tetrad->rotation % 4], TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, tetrad->shape);
                break;
            case J_TETRAD:
		        copyGrid(origin, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, jTetradRotations[tetrad->rotation % 4], TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, tetrad->shape);
                break;
            case L_TETRAD:
		        copyGrid(origin, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, lTetradRotations[tetrad->rotation % 4], TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, tetrad->shape);
                break;
            case S_TETRAD:
		        copyGrid(origin, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, sTetradRotations[tetrad->rotation % 4], TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, tetrad->shape);
                break;
            case Z_TETRAD:
		        copyGrid(origin, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, zTetradRotations[tetrad->rotation % 4], TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, tetrad->shape);
                break;
            default:
                break;
        }
    }
}

void ICACHE_FLASH_ATTR softDropTetrad()
{
    //TODO: is this the best way to handle this? 2*normal is reference
    dropTimer += deltaTime * SOFT_DROP_FACTOR;
}

void ICACHE_FLASH_ATTR moveTetrad(tetrad_t * tetrad, uint8_t gridWidth, uint8_t gridHeight, uint32_t gridData[][gridWidth])
{
    // 0 = min top left
    // 9 = max top left    
    // 3 is the center of top left in normal tetris

    // TODO: this behavior exhibits jittering at the borders of 2 zones, how to prevent this?
    // save the last accel, and if didn't change by a certain threshold, then don't recaculate the value.

    int yMod = ttAccel.y / ACCEL_SEG_SIZE;
    
    coord_t targetPos;
    targetPos.r = tetrad->topLeft.r;
    targetPos.c = yMod + TETRAD_SPAWN_X;
    
    // Attempt to prevent jittering for gradual movements.
    if ((targetPos.c == tetrad->topLeft.c + 1 || 
        targetPos.c == tetrad->topLeft.c - 1) &&
        abs(ttAccel.y - ttLastTestAccel.y) <= ACCEL_JITTER_GUARD)
    {
        targetPos = tetrad->topLeft;
    }
    else
    {
        ttLastTestAccel = ttAccel;
    }

    bool moveClear = true;
    while (targetPos.c != tetrad->topLeft.c && moveClear) 
    {
        coord_t movePos = tetrad->topLeft;

        movePos.c = targetPos.c > movePos.c ? movePos.c + 1 : movePos.c - 1;
        
        if (checkCollision(movePos, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, tetrad->shape, gridWidth, gridHeight, gridData, tetrad->gridValue))
        {
            moveClear = false;
        }
        else
        {
            tetrad->topLeft = movePos;
        }
    }
}

bool ICACHE_FLASH_ATTR dropTetrad(tetrad_t * tetrad, uint8_t gridWidth, uint8_t gridHeight, uint32_t gridData[][gridWidth])
{
    coord_t dropPos = tetrad->topLeft;
    dropPos.r++;
    bool dropSuccess = !checkCollision(dropPos, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, tetrad->shape, gridWidth, gridHeight, gridData, tetrad->gridValue);

    // Move the tetrad down if it's clear to do so.
    if (dropSuccess)
    {
        tetrad->topLeft = dropPos;
    }
    return dropSuccess;
}

tetrad_t ICACHE_FLASH_ATTR spawnTetrad(tetradType_t type, uint32_t gridValue, coord_t gridCoord, int rotation)
{
    tetrad_t tetrad;
    tetrad.type = type;
    tetrad.gridValue = gridValue;
    tetrad.rotation = rotation;
    tetrad.topLeft = gridCoord;
    coord_t origin;
    origin.c = 0;
    origin.r = 0;
    switch (tetrad.type)
    {
        case I_TETRAD:
			copyGrid(origin, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, iTetradRotations[rotation], TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, tetrad.shape);
            break;
        case O_TETRAD:
		    copyGrid(origin, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, oTetradRotations[rotation], TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, tetrad.shape);
            break;
        case T_TETRAD:
		    copyGrid(origin, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, tTetradRotations[rotation], TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, tetrad.shape);
            break;
        case J_TETRAD:
		    copyGrid(origin, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, jTetradRotations[rotation], TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, tetrad.shape);
            break;
        case L_TETRAD:
		    copyGrid(origin, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, lTetradRotations[rotation], TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, tetrad.shape);
            break;
        case S_TETRAD:
		    copyGrid(origin, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, sTetradRotations[rotation], TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, tetrad.shape);
            break;
        case Z_TETRAD:
		    copyGrid(origin, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, zTetradRotations[rotation], TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, tetrad.shape);
            break;
        default:
            break;
    }
    return tetrad;
}

void ICACHE_FLASH_ATTR spawnNextTetrad(tetrad_t * newTetrad, tetradRandomizer_t randomType, uint32_t gridValue, uint8_t gridWidth, uint8_t gridHeight, uint32_t gridData[][gridWidth])
{
    coord_t spawnPos;
    spawnPos.c = TETRAD_SPAWN_X;
    spawnPos.r = TETRAD_SPAWN_Y;
    *newTetrad = spawnTetrad(nextTetradType, gridValue, spawnPos, TETRAD_SPAWN_ROT);
    nextTetradType = (tetradType_t)getNextTetradType(randomType, tetradCounter);

    // Check if this is blocked, if it is, the game is over.
    if (checkCollision(newTetrad->topLeft, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, newTetrad->shape, gridWidth, gridHeight, gridData, newTetrad->gridValue))
    {
        ttChangeState(TT_GAMEOVER);
    }
    // If the game isn't over, move the initial tetrad to where it should be based on the accelerometer.
    else
    {
        moveTetrad(newTetrad, gridWidth, gridHeight, gridData);
    }
}

void ICACHE_FLASH_ATTR plotSquare(int x0, int y0, int size, color col)
{
    plotRect(x0, y0, x0 + (size - 1), y0 + (size - 1), col);
}

void ICACHE_FLASH_ATTR plotGrid(int x0, int y0, uint8_t unitSize, uint8_t gridWidth, uint8_t gridHeight, uint32_t gridData[][gridWidth], bool clearLineAnimation, color col)
{
    // Draw the border
    plotRect(x0, y0, x0 + (unitSize - 1) * gridWidth, y0 + (unitSize - 1) * gridHeight, col);

    // Draw points for grid (maybe disable when not debugging)
    for (int y = 0; y < gridHeight; y++)
    {
        // Draw lines that are cleared.
        if (clearLineAnimation && isLineCleared(y, gridWidth, gridHeight, gridData)) 
        {
            fillDisplayArea(x0, y0 + ((unitSize - 1) * y), x0 + ((unitSize - 1) * gridWidth), y0 + ((unitSize - 1) * (y+1)), WHITE);
        }

        for (int x = 0; x < gridWidth; x++) 
        {
            // Draw a centered pixel on empty grid units.
            //if (gridData[y][x] == EMPTY) drawPixel(x0 + x * (unitSize - 1) + (unitSize / 2), y0 + y * (unitSize - 1) + (unitSize / 2), WHITE);
        }
    }
}

void ICACHE_FLASH_ATTR plotShape(int x0, int y0, uint8_t unitSize, uint8_t shapeWidth, uint8_t shapeHeight, uint32_t shape[][shapeWidth], color col)
{   
    //TODO: different fill patterns?
    for (int y = 0; y < shapeHeight; y++)
    {
        for (int x = 0; x < shapeWidth; x++)
        {
            if (shape[y][x] != EMPTY) 
            {
                drawPixel(x0 + x * (unitSize - 1) + (unitSize / 2), y0 + y * (unitSize - 1) + (unitSize / 2), col);
                //plotSquare(x0+x*unitSize, y0+y*unitSize, unitSize);
                //top
                if (y == 0 || shape[y-1][x] == EMPTY)
                {
                    plotLine(x0 + x * (unitSize-1),     y0 + y * (unitSize-1), 
                             x0 + (x+1) * (unitSize-1), y0 + y * (unitSize-1), col);   
                }
                //bot
                if (y == shapeHeight-1 || shape[y+1][x] == EMPTY)
                {
                    plotLine(x0 + x * (unitSize-1),     y0 + (y+1) * (unitSize-1), 
                             x0 + (x+1) * (unitSize-1), y0 + (y+1) * (unitSize-1), col);   
                }
                
                //left
                if (x == 0 || shape[y][x-1] == EMPTY)
                {
                    plotLine(x0 + x * (unitSize-1), y0 + y * (unitSize-1), 
                             x0 + x * (unitSize-1), y0 + (y+1) * (unitSize-1), col);   
                }
                //right
                if (x == shapeWidth-1 || shape[y][x+1] == EMPTY)
                {
                    plotLine(x0 + (x+1) * (unitSize-1), y0 + y * (unitSize-1), 
                             x0 + (x+1) * (unitSize-1), y0 + (y+1) * (unitSize-1), col);   
                }
            }
        }
    }
}

// Draw text centered between x0 and x1.
uint8_t ICACHE_FLASH_ATTR plotCenteredText(uint8_t x0, uint8_t y, uint8_t x1, char* text, fonts font, color col)
{
    uint8_t centeredX = getCenteredTextX(x0, x1, text, font);

    // Then we draw the correctly centered text.
    uint8_t cursorEnd = plotText(centeredX, y, text, font, col);
    return cursorEnd;
}

uint8_t ICACHE_FLASH_ATTR getCenteredTextX(uint8_t x0, uint8_t x1, char* text, fonts font)
{
    uint8_t textWidth = getTextWidth(text, font);

    // Calculate the correct x to draw from.
    uint8_t fullWidth = x1 - x0 + 1;
    // NOTE: This may result in strange behavior when the width of the drawn text is greater than the distance between x0 and x1.
    uint8_t widthDiff = fullWidth - textWidth; 
    uint8_t centeredX = x0 + (widthDiff / 2);
    return centeredX;
}

uint8_t ICACHE_FLASH_ATTR getTextWidth(char* text, fonts font)
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

void ICACHE_FLASH_ATTR initTetradRandomizer(tetradRandomizer_t randomType)
{
    switch (randomType)
    {
        case RANDOM:
            break;
        case BAG:
            bagIndex = 0;
            shuffle(NUM_TETRAD_TYPES, typeBag);
            break;
        case POOL:
            {
                // Initialize the tetrad type pool, 5 of each type.
                for (int i = 0; i < 5; i++)
                {
                    for (int j = 0; j < NUM_TETRAD_TYPES; j++)
                    {
                        typePool[i * NUM_TETRAD_TYPES + j] = j+1;
                    }
                }

                // Clear the history.
                for (int i = 0; i < 4; i++)
                {
                    typeHistory[i] = 0;
                }

                // Populate the history with initial values.
                typeHistory[0] = S_TETRAD;
                typeHistory[1] = Z_TETRAD;
                typeHistory[2] = S_TETRAD;

                // Clear the order list.
                clearTypeOrder();
            }
            break;
        default:
            break;
    }
}

int ICACHE_FLASH_ATTR getNextTetradType(tetradRandomizer_t randomType, int index)
{
    int nextType = EMPTY;
    switch (randomType)
    {
        case RANDOM:
            nextType = (rand() % NUM_TETRAD_TYPES) + 1;
            break;
        case BAG:
            nextType = typeBag[bagIndex];
            bagIndex++;
            if (bagIndex >= NUM_TETRAD_TYPES) 
            {
                initTetradRandomizer(randomType);
            }
            break;
        case POOL:
            {
                // First piece special conditions.
                if (index == 0)
                {
                    nextType = firstType[rand() % 4];
                    typeHistory[3] = nextType;
                }
                else
                {
                    // The pool index of the next piece.
                    int i;

                    // Roll for piece.
                    for (int r = 0; r < 6; r++) 
                    {
                        i = rand() % 35;
                        nextType = typePool[i];
                        
                        bool inHistory = false;
                        for (int h = 0; h < 4; h++) 
                        {
                            if (typeHistory[h] == nextType) inHistory = true;
                        }
                        
                        if (!inHistory || r == 5)
                        {
                            break;
                        }

                        if (typeOrder->length > 0) typePool[i] = *((int *)typeOrder->first->val);
                    }

                    // Update piece order.
                    node_t * current = typeOrder->last;
                    for (int j = typeOrder->length - 1; j >= 0; j--) 
                    {
                        // Get the current value.
                        int * currentType = (int *)current->val;
                        
                        // Update current in case we remove this node.
                        current = current->prev;

                        // Remove this node and free its value if it matches.
                        if (*currentType == nextType) 
                        {
                            free(remove(typeOrder, j));
                        }
                    }
                    int * newOrderType = malloc(sizeof(int));
                    *newOrderType = nextType;
                    push(typeOrder, newOrderType);

                    typePool[i] = *((int *)typeOrder->first->val);

                    // Update history.
                    for (int h = 0; h < 4; h++) 
                    {
                        if (h == 3)
                        {
                            typeHistory[h] = nextType;
                        }
                        else
                        {
                            typeHistory[h] = typeHistory[h+1];
                        }
                    }
                }
            }
            break;
        default:
            break;
    }
    return nextType;
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

uint32_t ICACHE_FLASH_ATTR getDropTime(uint32_t level)
{
    uint32_t dropTimeFrames = 0; 
    
    switch (level)
    {
        case 0:
			dropTimeFrames = 48;
            break;
        case 1:
			dropTimeFrames = 43;
            break;
        case 2:
			dropTimeFrames = 38;
            break;
        case 3:
			dropTimeFrames = 33;
            break;
        case 4:
			dropTimeFrames = 28;
            break;
        case 5:
			dropTimeFrames = 23;
            break;
        case 6:
			dropTimeFrames = 18;
            break;
        case 7:
			dropTimeFrames = 13;
            break;
        case 8:
			dropTimeFrames = 8;
            break;
        case 9:
			dropTimeFrames = 6;
            break;
        case 10:
        case 11:
        case 12:
			dropTimeFrames = 5;
            break;
        case 13:
        case 14:
        case 15:
			dropTimeFrames = 4;
            break;
        case 16:
        case 17:
        case 18:
			dropTimeFrames = 3;
            break;
        case 19:
        case 20:
        case 21:
        case 22:
        case 23:
        case 24:
        case 25:
        case 26:
        case 27:
        case 28:
			dropTimeFrames = 2;
            break;
        case 29:
			dropTimeFrames = 1;
            break;
        default:
            break;
    }
    
    // We need the time in microseconds.
    return dropTimeFrames * UPDATE_TIME_MS * MS_TO_US_FACTOR;
}

void loadHighScores(void)
{
    memcpy(highScores, ttGetHighScores(),  NUM_TT_HIGH_SCORES * sizeof(uint32_t));
}

void saveHighScores(void)
{
    ttSetHighScores(highScores);
}

bool updateHighScores(uint32_t newScore)
{
    bool highScore = false;
    uint32_t placeScore = newScore;
    for (int i = 0; i < NUM_TT_HIGH_SCORES; i++)
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

void ICACHE_FLASH_ATTR initLandedTetrads()
{
    landedTetrads = malloc(sizeof(list_t));
    landedTetrads->first = NULL;
    landedTetrads->last = NULL;
    landedTetrads->length = 0;
}

void ICACHE_FLASH_ATTR clearLandedTetrads()
{
    // Free all tetrads in the list.
    node_t * current = landedTetrads->first;
    while (current != NULL)
    {
        free(current->val);
        current->val = NULL;
        current = current->next;
    }
    // Free the node containers for the list.
    clear(landedTetrads);
}

void ICACHE_FLASH_ATTR deInitLandedTetrads()
{
    clearLandedTetrads();

    // Finally free the list itself.
    free(landedTetrads);
    landedTetrads = NULL;
}

void ICACHE_FLASH_ATTR startClearAnimation(int numLineClears)
{
    inClearAnimation = true;
    //clearAnimFrame = 1;
    clearTimer = 0;
    clearTime = CLEAR_LINES_ANIM_TIME;
}

void ICACHE_FLASH_ATTR stopClearAnimation()
{
    inClearAnimation = false;
    //clearAnimFrame = 1;
    clearTimer = 0;
    clearTime = 0;
}

bool ICACHE_FLASH_ATTR isLineCleared(int line, uint8_t gridWidth, uint8_t gridHeight __attribute__((unused)), uint32_t gridData[][gridWidth])
{
    bool clear = true;
    for (int c = 0; c < gridWidth; c++) 
    {
        if (gridData[line][c] == EMPTY) clear = false;
    }
    return clear;
}

int ICACHE_FLASH_ATTR checkLineClears(uint8_t gridWidth, uint8_t gridHeight, uint32_t gridData[][gridWidth], list_t * fieldTetrads)
{
    //Refresh the tetrads grid before checking for any clears.
    refreshTetradsGrid(gridWidth, gridHeight, gridData, fieldTetrads, NULL, false);

    int lineClears = 0;

    int currRow = gridHeight - 1;

    // Go through every row bottom-to-top.
    while (currRow >= 0) 
    {
        if (isLineCleared(currRow, gridWidth, gridHeight, gridData))
        {
            lineClears++;
        }
        currRow--;
    }

    return lineClears;
}

int ICACHE_FLASH_ATTR clearLines(uint8_t gridWidth, uint8_t gridHeight, uint32_t gridData[][gridWidth], list_t * fieldTetrads)
{
    //Refresh the tetrads grid before checking for any clears.
    refreshTetradsGrid(gridWidth, gridHeight, gridData, fieldTetrads, NULL, false);

    int lineClears = 0;

    int currRow = gridHeight - 1;

    // Go through every row bottom-to-top.
    while (currRow >= 0) 
    {
        if (isLineCleared(currRow, gridWidth, gridHeight, gridData))
        {
            lineClears++;
            
            node_t * current = fieldTetrads->last;
            // Update the positions of compositions of any effected tetrads.       
            for (int t = fieldTetrads->length - 1; t >= 0; t--)
            {
                tetrad_t * currentTetrad = (tetrad_t *)current->val;
                bool aboveClear = true;

                // Go from bottom-to-top on each position of the tetrad.
                for (int tr = TETRAD_GRID_SIZE - 1; tr >= 0; tr--) 
                {
                    for (int tc = 0; tc < TETRAD_GRID_SIZE; tc++) 
                    {
                        // Check where we are on the grid.
                        coord_t gridPos;
                        gridPos.r = currentTetrad->topLeft.r + tr;
                        gridPos.c = currentTetrad->topLeft.c + tc;
                        
                        // If any part of the tetrad (even empty) exists at the clear line, don't adjust its position downward.
                        if (gridPos.r >= currRow) aboveClear = false;

                        // If something exists at that position...
                        if (!aboveClear && currentTetrad->shape[tr][tc] != EMPTY) 
                        {
                            // Completely remove tetrad pieces on the cleared row.
                            if (gridPos.r == currRow)
                            {
                                currentTetrad->shape[tr][tc] = EMPTY;
                            }
                            // Move all the pieces of tetrads that are above the cleared row down by one.
                            else if (gridPos.r < currRow)
                            {
                                //TODO: What if it cannot be moved down anymore in its local grid? Can this happen?
                                if (tr < TETRAD_GRID_SIZE - 1)
                                {
                                    // Copy the current space into the space below it.                                        
                                    currentTetrad->shape[tr+1][tc] = currentTetrad->shape[tr][tc];
                                    
                                    // Empty the current space.
                                    currentTetrad->shape[tr][tc] = EMPTY;
                                }
                            }
                        }
                    }
                }

                // Move tetrads entirely above the cleared line down by one.
                if (aboveClear && currentTetrad->topLeft.r < currRow)
                {
                    currentTetrad->topLeft.r++;
                }

                // Adjust the current counter.
                current = current->prev;
            }
            
            // Before we check against the gridData of all tetrads again, we need to rebuilt an accurate version.
            refreshTetradsGrid(gridWidth, gridHeight, gridData, fieldTetrads, NULL, false);
        }
        else
        {
            currRow--;
        }
    }

    return lineClears;
}

// what is the best way to handle collisions above the grid space?
bool ICACHE_FLASH_ATTR checkCollision(coord_t newPos, uint8_t shapeWidth, uint8_t shapeHeight __attribute__((unused)), uint32_t shape[][shapeWidth], uint8_t gridWidth, uint8_t gridHeight, uint32_t gridData[][gridWidth], uint32_t selfGridValue)
{
    for (int r = 0; r < TETRAD_GRID_SIZE; r++)
    {
        for (int c = 0; c < TETRAD_GRID_SIZE; c++)
        {
            if (shape[r][c] != EMPTY)
            {
                if (newPos.r + r >= gridHeight || 
                    newPos.c + c >= gridWidth || 
                    newPos.c + c < 0 || 
                    (gridData[newPos.r + r][newPos.c + c] != EMPTY &&
                     gridData[newPos.r + r][newPos.c + c] != selfGridValue)) // Don't check collision with yourself.
                {
                    return true;
                }
            }
        }
    }
    return false;
}

