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

// https://gamedevelopment.tutsplus.com/tutorials/implementing-tetris-collision-detection--gamedev-852
// https://simon.lc/the-history-of-tetris-randomizers

//MODE TODO for Thursday Demo:
// 1. Handle cascade piece falling from clears.
// 3. Better piece randomizer.
// 4. Drop time decreasing as time goes on to increase difficulty.
// 5. Observed bug with empty spaces appearing below line clears (I think).


// any defines go here.
#define BTN_START_GAME RIGHT
#define BTN_START_SCORES LEFT
#define BTN_START_TITLE LEFT
#define BTN_ROTATE RIGHT
#define BTN_DROP LEFT

// update task info.
#define UPDATE_TIME_MS 16 

// playfield settings.
#define GRID_X 45
#define GRID_Y -5
#define GRID_UNIT_SIZE 5
#define GRID_WIDTH 10
#define GRID_HEIGHT 17

#define NEXT_GRID_X 97
#define NEXT_GRID_Y 3
#define NEXT_GRID_WIDTH 5
#define NEXT_GRID_HEIGHT 5

#define EMPTY 0
#define MAX_TETRADS 160

#define TETRAD_SPAWN_ROT 0
#define TETRAD_SPAWN_X 3
#define TETRAD_SPAWN_Y 0//-1
#define TETRAD_GRID_SIZE 4

#define ACCEL_SEG_SIZE 25 // higher vale more or less means less sensetive.
#define ACCEL_JITTER_GUARD 7 // higher = less sensetive.

// any enums go here.
typedef enum
{
    TT_TITLE,	// title screen
    TT_GAME,	// play the actual game
    TT_SCORES	// high scores / game over screen
	//TODO: does this need a transition state?
} tiltradsState_t;

uint8_t tetradsGrid[GRID_HEIGHT][GRID_WIDTH];

uint8_t nextTetradGrid[NEXT_GRID_HEIGHT][NEXT_GRID_WIDTH];

uint8_t iTetradRotations [4][TETRAD_GRID_SIZE][TETRAD_GRID_SIZE] = 
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

uint8_t oTetradRotations [4][TETRAD_GRID_SIZE][TETRAD_GRID_SIZE] = 
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

uint8_t tTetradRotations [4][TETRAD_GRID_SIZE][TETRAD_GRID_SIZE] = 
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

uint8_t jTetradRotations [4][TETRAD_GRID_SIZE][TETRAD_GRID_SIZE] = 
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

uint8_t lTetradRotations [4][TETRAD_GRID_SIZE][TETRAD_GRID_SIZE] = 
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

uint8_t sTetradRotations [4][TETRAD_GRID_SIZE][TETRAD_GRID_SIZE] = 
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

uint8_t zTetradRotations [4][TETRAD_GRID_SIZE][TETRAD_GRID_SIZE] = 
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

typedef struct
{
    tetradType_t type;
    int rotation; 
    coord_t topLeft;
    uint8_t shape[TETRAD_GRID_SIZE][TETRAD_GRID_SIZE];
} tetrad_t;

tetrad_t activeTetrad;

tetradType_t nextTetradType;

int numLandedTetrads;
tetrad_t landedTetrads[MAX_TETRADS];

uint32_t dropTimer = 0;  // The last time a tetrad dropped.
uint32_t dropTime = 1000000;   // The amount of time it takes for a tetrad to drop.

accel_t ttLastTestAccel = {0};

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

// update any input-unrelated logic.
void ICACHE_FLASH_ATTR ttTitleUpdate(void);
void ICACHE_FLASH_ATTR ttGameUpdate(void);
void ICACHE_FLASH_ATTR ttScoresUpdate(void);

// draw the frame.
void ICACHE_FLASH_ATTR ttTitleDisplay(void);
void ICACHE_FLASH_ATTR ttGameDisplay(void);
void ICACHE_FLASH_ATTR ttScoresDisplay(void);

// helper functions.
void ICACHE_FLASH_ATTR ttChangeState(tiltradsState_t newState);
bool ICACHE_FLASH_ATTR ttIsButtonPressed(uint8_t button);
bool ICACHE_FLASH_ATTR ttIsButtonDown(uint8_t button);
bool ICACHE_FLASH_ATTR ttIsButtonUp(uint8_t button);
void ICACHE_FLASH_ATTR copyGrid(coord_t srcOffset, uint8_t srcWidth, uint8_t srcHeight, uint8_t src[][srcWidth], uint8_t dstWidth, uint8_t dstHeight, uint8_t dst[][dstWidth]);
void ICACHE_FLASH_ATTR transferGrid(coord_t srcOffset, uint8_t srcWidth, uint8_t srcHeight, uint8_t src[][srcWidth], uint8_t dstWidth, uint8_t dstHeight, uint8_t dst[][dstWidth], uint8_t transferVal);
void ICACHE_FLASH_ATTR clearGrid(uint8_t gridWidth, uint8_t gridHeight, uint8_t gridData[][gridWidth]);
void ICACHE_FLASH_ATTR rotateTetrad(void);
void ICACHE_FLASH_ATTR dropTetrad(void);
void ICACHE_FLASH_ATTR moveTetrad(void);
tetrad_t ICACHE_FLASH_ATTR spawnTetrad(tetradType_t type, coord_t gridCoord, int rotation);
void ICACHE_FLASH_ATTR spawnNextTetrad(void);
void ICACHE_FLASH_ATTR plotSquare(int x0, int y0, int size);
void ICACHE_FLASH_ATTR plotGrid(int x0, int y0, uint8_t unitSize, uint8_t gridWidth, uint8_t gridHeight, uint8_t gridData[][gridWidth]);
void ICACHE_FLASH_ATTR plotShape(int x0, int y0, uint8_t unitSize, uint8_t shapeWidth, uint8_t shapeHeight, uint8_t shape[][shapeWidth]);
int ICACHE_FLASH_ATTR getNextTetradType(void);

bool ICACHE_FLASH_ATTR isLineCleared(int line, uint8_t gridWidth, uint8_t gridHeight, uint8_t gridData[][gridWidth]);
void ICACHE_FLASH_ATTR checkLineClears(uint8_t gridWidth, uint8_t gridHeight, uint8_t gridData[][gridWidth]);

bool ICACHE_FLASH_ATTR checkCollision(coord_t newPos, uint8_t shapeWidth, uint8_t shapeHeight, uint8_t shape[][shapeWidth], uint8_t gridWidth, uint8_t gridHeight, uint8_t gridData[][gridWidth]);

swadgeMode tiltradsMode = 
{
	.modeName = "Tiltrads",
	.fnEnterMode = ttInit,
	.fnExitMode = ttDeInit,
	.fnButtonCallback = ttButtonCallback,
	.fnAudioCallback = NULL,
	.wifiMode = NO_WIFI,
	.fnEspNowRecvCb = NULL,
	.fnEspNowSendCb = NULL,
	.fnAccelerometerCallback = ttAccelerometerCallback
};

accel_t ttAccel = {0};
accel_t ttLastAccel = {0};

uint8_t ttButtonState = 0;
uint8_t ttLastButtonState = 0;

static os_timer_t timerHandleUpdate = {0};

uint32_t modeStartTime = 0; // time mode started in microseconds.
uint32_t stateStartTime = 0; // time the most recent state started in microseconds.
uint32_t deltaTime = 0;	// time elapsed since last update.
uint32_t modeTime = 0;	// total time the mode has been running.
uint32_t stateTime = 0;	// total time the game has been running.

tiltradsState_t currState = TT_TITLE;

//TODO: implement basic ux flow: *->menu screen->game screen->game over / high score screen->*
//TODO: paste mode description from github issue into here, work from that.
//TODO: accelerometer used to move pieces like a steering wheel, not tilting side to side.
//TODO: draw a block, draw a tetrad, have it fall, have it land in place, tilt it.

void ICACHE_FLASH_ATTR ttInit(void)
{
    // Give us responsive input.
	enableDebounce(false);	
	
	// Reset mode time tracking.
	modeStartTime = system_get_time();
	modeTime = 0;

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
	uint32_t newModeTime = system_get_time() - modeStartTime;
	uint32_t newStateTime = system_get_time() - stateStartTime;
	deltaTime = newModeTime - modeTime;
	modeTime = newModeTime;
	stateTime = newStateTime;

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
        default:
            break;
    };
}

void ICACHE_FLASH_ATTR ttTitleInput(void)
{   
    //button a = start game
    if(ttIsButtonPressed(BTN_START_GAME))
    {
        ttChangeState(TT_GAME);
    }
    //button b = go to score screen
    else if(ttIsButtonPressed(BTN_START_SCORES))
    {
        ttChangeState(TT_SCORES);
    }

    //TODO: accel = tilt something on screen like you would a tetrad
}

void ICACHE_FLASH_ATTR ttGameInput(void)
{
	//button a = rotate piece
    if(ttIsButtonPressed(BTN_ROTATE))
    {
        rotateTetrad();
    }
    //button b = instant drop piece
    if(ttIsButtonDown(BTN_DROP))
    {
        dropTetrad();
    }

    // Only move tetrads left and right when the fast drop button isn't being held down.
    if(ttIsButtonUp(BTN_DROP))
    {
        moveTetrad();
    }
}

void ICACHE_FLASH_ATTR ttScoresInput(void)
{
	//button a = start game
    if(ttIsButtonPressed(BTN_START_GAME))
    {
        ttChangeState(TT_GAME);
    }
    //button b = go to title screen
    else if(ttIsButtonPressed(BTN_START_TITLE))
    {
        ttChangeState(TT_TITLE);
    }

	//TODO: accel = tilt to scroll through history of scores?
}

void ICACHE_FLASH_ATTR ttTitleUpdate(void)
{

}

void ICACHE_FLASH_ATTR ttGameUpdate(void)
{
    // clear the grid
    // copy all landed tetrads into it.
    
    //NOTES: absolute left and right for tilting vs. current relative.

    // drop the piece if it's time to drop it
    // if a piece is landed, spawn a new tetrad
    
    dropTimer += deltaTime;

    if (dropTimer >= dropTime)
    {
        dropTimer = 0;

        coord_t dropPos = activeTetrad.topLeft;
        dropPos.r++;

        if (checkCollision(dropPos, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, activeTetrad.shape, GRID_WIDTH, GRID_HEIGHT, tetradsGrid))
        {
            // Land the current tetrad.
            landedTetrads[numLandedTetrads] = activeTetrad;
            numLandedTetrads++;
            
            // Check for any clears now that the new tetrad has landed.
            checkLineClears(GRID_WIDTH, GRID_HEIGHT, tetradsGrid);

            // Spawn the next tetrad.
            spawnNextTetrad();
        }
        else
        {
            activeTetrad.topLeft = dropPos;
        }
    }
}

void ICACHE_FLASH_ATTR ttScoresUpdate(void)
{

}

void ICACHE_FLASH_ATTR ttTitleDisplay(void)
{
	// draw title
	// draw button prompt
	// draw accelerometer controlled thing(s)
	// draw fx

	// Clear the display
    clearDisplay();

    // Draw a title
    plotText(20, 5, "TILTRADS", RADIOSTARS);
    //plotText(35, (FONT_HEIGHT_RADIOSTARS + 1), "TILTRADS", RADIOSTARS);

    // Display the acceleration on the display
    /*char accelStr[32] = {0};

    ets_snprintf(accelStr, sizeof(accelStr), "X:%d", ttAccel.x);
    plotText(0, OLED_HEIGHT - (3 * (FONT_HEIGHT_IBMVGA8 + 1)), accelStr, IBM_VGA_8);

    ets_snprintf(accelStr, sizeof(accelStr), "Y:%d", ttAccel.y);
    plotText(0, OLED_HEIGHT - (2 * (FONT_HEIGHT_IBMVGA8 + 1)), accelStr, IBM_VGA_8);

    ets_snprintf(accelStr, sizeof(accelStr), "Z:%d", ttAccel.z);
    plotText(0, OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)), accelStr, IBM_VGA_8);*/

    // Draw text in the bottom corners to direct which buttons will do what.
    //drawPixel(OLED_WIDTH - 1, OLED_HEIGHT-15, WHITE);
    //drawPixel(0, OLED_HEIGHT-15, WHITE);
    plotText(0, OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)), "SCORES", IBM_VGA_8);
    plotText(OLED_WIDTH - ((8 * 5) - 3), OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)), "START", IBM_VGA_8);
}

void ICACHE_FLASH_ATTR ttGameDisplay(void)
{
    // Clear the display
    clearDisplay();
    
    // Clear the grid data (may not want to do this every frame)
    clearGrid(GRID_WIDTH, GRID_HEIGHT, tetradsGrid);

    // Draw the active tetrad.
    // TODO: this is writing to the grid in such a way that the active tetrad is always overlapping with itself in update checks, move the copy grids into game update code. 
    plotShape(GRID_X + activeTetrad.topLeft.c * (GRID_UNIT_SIZE - 1), GRID_Y + activeTetrad.topLeft.r * (GRID_UNIT_SIZE - 1), GRID_UNIT_SIZE, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, activeTetrad.shape);   
    //copyGrid(activeTetrad.topLeft, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, activeTetrad.shape, GRID_WIDTH, GRID_HEIGHT, tetradsGrid);
    transferGrid(activeTetrad.topLeft, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, activeTetrad.shape, GRID_WIDTH, GRID_HEIGHT, tetradsGrid, numLandedTetrads+1);

    // Draw all the landed tetrads.
    for (int i = 0; i < numLandedTetrads; i++) 
    {
        plotShape(GRID_X + landedTetrads[i].topLeft.c * (GRID_UNIT_SIZE - 1), GRID_Y + landedTetrads[i].topLeft.r * (GRID_UNIT_SIZE - 1), GRID_UNIT_SIZE, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, landedTetrads[i].shape);   
        //copyGrid(landedTetrads[i].topLeft, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, landedTetrads[i].shape, GRID_WIDTH, GRID_HEIGHT, tetradsGrid);
        transferGrid(landedTetrads[i].topLeft, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, landedTetrads[i].shape, GRID_WIDTH, GRID_HEIGHT, tetradsGrid, i+1);
    }

    // Draw the background grid. NOTE: (make sure everything that needs to be in tetradsGrid is in there now).
    plotGrid(GRID_X, GRID_Y, GRID_UNIT_SIZE, GRID_WIDTH, GRID_HEIGHT, tetradsGrid);

    
    // Draw the next tetrad.
    clearGrid(NEXT_GRID_WIDTH, NEXT_GRID_HEIGHT, nextTetradGrid);

    coord_t nextTetradPoint;
    nextTetradPoint.c = 1;
    nextTetradPoint.r = 1;

    tetrad_t nextTetrad = spawnTetrad(nextTetradType, nextTetradPoint, 0);
    plotShape(NEXT_GRID_X + nextTetradPoint.c * (GRID_UNIT_SIZE - 1), NEXT_GRID_Y + nextTetradPoint.r * (GRID_UNIT_SIZE - 1), GRID_UNIT_SIZE, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, nextTetrad.shape);   
    copyGrid(nextTetrad.topLeft, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, nextTetrad.shape, NEXT_GRID_WIDTH, NEXT_GRID_HEIGHT, nextTetradGrid);

    plotGrid(NEXT_GRID_X, NEXT_GRID_Y, GRID_UNIT_SIZE, NEXT_GRID_WIDTH, NEXT_GRID_HEIGHT, nextTetradGrid);

    // Debug text:
    char debugStr[32] = {0};
    ets_snprintf(debugStr, sizeof(debugStr), "Y: %d", (ttAccel.y / ACCEL_SEG_SIZE));
    plotText(0, 0, debugStr, IBM_VGA_8);

    // Display the acceleration on the display
    char accelStr[32] = {0};

    ets_snprintf(accelStr, sizeof(accelStr), "X:%d", ttAccel.x);
    plotText(0, OLED_HEIGHT - (3 * (FONT_HEIGHT_IBMVGA8 + 1)), accelStr, IBM_VGA_8);

    ets_snprintf(accelStr, sizeof(accelStr), "Y:%d", ttAccel.y);
    plotText(0, OLED_HEIGHT - (2 * (FONT_HEIGHT_IBMVGA8 + 1)), accelStr, IBM_VGA_8);

    ets_snprintf(accelStr, sizeof(accelStr), "Z:%d", ttAccel.z);
    plotText(0, OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)), accelStr, IBM_VGA_8);

    //TODO: score
    //TODO: lines
    //TODO: level
    //TODO: the current high score?
}

void ICACHE_FLASH_ATTR ttScoresDisplay(void)
{
    // Clear the display
    clearDisplay();

    plotText(0, 0, "SCORES", IBM_VGA_8);

    //drawPixel(OLED_WIDTH - 1, OLED_HEIGHT-15, WHITE);
    //drawPixel(0, OLED_HEIGHT-15, WHITE);
    plotText(0, OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)), "TITLE", IBM_VGA_8);
    plotText(OLED_WIDTH - ((8 * 5) - 3), OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)), "START", IBM_VGA_8);
}

// helper functions.

void ICACHE_FLASH_ATTR ttChangeState(tiltradsState_t newState)
{
	currState = newState;
	stateStartTime = system_get_time();
	stateTime = 0;

    switch( currState )
    {
        case TT_TITLE:
            break;
        case TT_GAME:
            // TODO: any game restart stuff should go here.
            clearGrid(GRID_WIDTH, GRID_HEIGHT, tetradsGrid);
            clearGrid(NEXT_GRID_WIDTH, NEXT_GRID_HEIGHT, nextTetradGrid);
            numLandedTetrads = 0;
            dropTimer = 0;
            //moveTimer = 0;
            // TODO: reset the piece gen stuff.
            nextTetradType = (tetradType_t)getNextTetradType();
            spawnNextTetrad();
            break;
        case TT_SCORES:
            break;
        default:
            break;
    };
}

bool ICACHE_FLASH_ATTR ttIsButtonPressed(uint8_t button)
{
    //TODO: can press events get lost this way?
    return (ttButtonState & button) && !(ttLastButtonState & button);
}

bool ICACHE_FLASH_ATTR ttIsButtonDown(uint8_t button)
{
    //TODO: can press events get lost this way?
    return ttButtonState & button;
}

bool ICACHE_FLASH_ATTR ttIsButtonUp(uint8_t button)
{
    //TODO: can press events get lost this way?
    return !(ttButtonState & button);
}

void ICACHE_FLASH_ATTR copyGrid(coord_t srcOffset, uint8_t srcWidth, uint8_t srcHeight, uint8_t src[][srcWidth], uint8_t dstWidth, uint8_t dstHeight, uint8_t dst[][dstWidth])
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

//TODO: better name for this, we're transferring new values according to the space laid out here.
void ICACHE_FLASH_ATTR transferGrid(coord_t srcOffset, uint8_t srcWidth, uint8_t srcHeight, uint8_t src[][srcWidth], uint8_t dstWidth, uint8_t dstHeight, uint8_t dst[][dstWidth], uint8_t transferVal)
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

void ICACHE_FLASH_ATTR clearGrid(uint8_t gridWidth, uint8_t gridHeight, uint8_t gridData[][gridWidth])
{
    for (int y = 0; y < gridHeight; y++)
    {
        for (int x = 0; x < gridWidth; x++)
        {
            gridData[y][x] = EMPTY;
        }
    }
}

// This assumes only complete tetrads can be rotated.
void ICACHE_FLASH_ATTR rotateTetrad()
{
    int newRotation = activeTetrad.rotation + 1;
    bool rotationClear = false;

    switch (activeTetrad.type)
    {
        case I_TETRAD:
            for (int i = 0; i < 5; i++)
            {
                if (!rotationClear)
                {
                    coord_t testPoint;
                    testPoint.r = activeTetrad.topLeft.r + iTetradRotationTests[activeTetrad.rotation % 4][i].r;
                    testPoint.c = activeTetrad.topLeft.c + iTetradRotationTests[activeTetrad.rotation % 4][i].c;
                    rotationClear = !checkCollision(testPoint, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, iTetradRotations[newRotation % 4], GRID_WIDTH, GRID_HEIGHT, tetradsGrid);
                    if (rotationClear) activeTetrad.topLeft = testPoint;
                }
            }            
            break;
        case O_TETRAD:
            rotationClear = true;//!checkCollision(activeTetrad.topLeft, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, oTetradRotations[newRotation % 4], GRID_WIDTH, GRID_HEIGHT, tetradsGrid);
	        break;
        case T_TETRAD:
            for (int i = 0; i < 5; i++)
            {
                if (!rotationClear)
                {
                    coord_t testPoint;
                    testPoint.r = activeTetrad.topLeft.r + otjlszTetradRotationTests[activeTetrad.rotation % 4][i].r;
                    testPoint.c = activeTetrad.topLeft.c + otjlszTetradRotationTests[activeTetrad.rotation % 4][i].c;
                    rotationClear = !checkCollision(testPoint, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, tTetradRotations[newRotation % 4], GRID_WIDTH, GRID_HEIGHT, tetradsGrid);
                    if (rotationClear) activeTetrad.topLeft = testPoint;
                }
            }
            break;
        case J_TETRAD:
            for (int i = 0; i < 5; i++)
            {
                if (!rotationClear)
                {
                    coord_t testPoint;
                    testPoint.r = activeTetrad.topLeft.r + otjlszTetradRotationTests[activeTetrad.rotation % 4][i].r;
                    testPoint.c = activeTetrad.topLeft.c + otjlszTetradRotationTests[activeTetrad.rotation % 4][i].c;
                    rotationClear = !checkCollision(testPoint, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, jTetradRotations[newRotation % 4], GRID_WIDTH, GRID_HEIGHT, tetradsGrid);
                    if (rotationClear) activeTetrad.topLeft = testPoint;
                }
            }
            break;
        case L_TETRAD:
            for (int i = 0; i < 5; i++)
            {
                if (!rotationClear)
                {
                    coord_t testPoint;
                    testPoint.r = activeTetrad.topLeft.r + otjlszTetradRotationTests[activeTetrad.rotation % 4][i].r;
                    testPoint.c = activeTetrad.topLeft.c + otjlszTetradRotationTests[activeTetrad.rotation % 4][i].c;
                    rotationClear = !checkCollision(testPoint, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, lTetradRotations[newRotation % 4], GRID_WIDTH, GRID_HEIGHT, tetradsGrid);
                    if (rotationClear) activeTetrad.topLeft = testPoint;
                }
            }
            break;
        case S_TETRAD:
            for (int i = 0; i < 5; i++)
            {
                if (!rotationClear)
                {
                    coord_t testPoint;
                    testPoint.r = activeTetrad.topLeft.r + otjlszTetradRotationTests[activeTetrad.rotation % 4][i].r;
                    testPoint.c = activeTetrad.topLeft.c + otjlszTetradRotationTests[activeTetrad.rotation % 4][i].c;
                    rotationClear = !checkCollision(testPoint, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, sTetradRotations[newRotation % 4], GRID_WIDTH, GRID_HEIGHT, tetradsGrid);
                    if (rotationClear) activeTetrad.topLeft = testPoint;
                }
            }
            break;
        case Z_TETRAD:
            for (int i = 0; i < 5; i++)
            {
                if (!rotationClear)
                {
                    coord_t testPoint;
                    testPoint.r = activeTetrad.topLeft.r + otjlszTetradRotationTests[activeTetrad.rotation % 4][i].r;
                    testPoint.c = activeTetrad.topLeft.c + otjlszTetradRotationTests[activeTetrad.rotation % 4][i].c;
                    rotationClear = !checkCollision(testPoint, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, zTetradRotations[newRotation % 4], GRID_WIDTH, GRID_HEIGHT, tetradsGrid);
                    if (rotationClear) activeTetrad.topLeft = testPoint;
                }
            }
            break;
        default:
            break;
    }

    //TODO: handle wall kicks correctly.

    if (rotationClear)
    {

        // Actually rotate the tetrad.
        activeTetrad.rotation = newRotation;
        coord_t origin;
        origin.c = 0;
        origin.r = 0;
        switch (activeTetrad.type)
        {
            case I_TETRAD:
			    copyGrid(origin, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, iTetradRotations[activeTetrad.rotation % 4], TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, activeTetrad.shape);
                break;
            case O_TETRAD:
		        copyGrid(origin, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, oTetradRotations[activeTetrad.rotation % 4], TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, activeTetrad.shape);
                break;
            case T_TETRAD:
		        copyGrid(origin, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, tTetradRotations[activeTetrad.rotation % 4], TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, activeTetrad.shape);
                break;
            case J_TETRAD:
		        copyGrid(origin, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, jTetradRotations[activeTetrad.rotation % 4], TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, activeTetrad.shape);
                break;
            case L_TETRAD:
		        copyGrid(origin, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, lTetradRotations[activeTetrad.rotation % 4], TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, activeTetrad.shape);
                break;
            case S_TETRAD:
		        copyGrid(origin, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, sTetradRotations[activeTetrad.rotation % 4], TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, activeTetrad.shape);
                break;
            case Z_TETRAD:
		        copyGrid(origin, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, zTetradRotations[activeTetrad.rotation % 4], TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, activeTetrad.shape);
                break;
            default:
                break;
        }
    }
}

void ICACHE_FLASH_ATTR dropTetrad()
{
    //TODO: is this the best way to handle this?
    dropTimer += deltaTime*8;
}

void ICACHE_FLASH_ATTR moveTetrad()
{
    // 0 = min top left
    // 9 = max top left    
    // 3 is the center of top left in normal tetris

    // TODO: this behavior exhibits jittering at the borders of 2 zones, how to prevent this?
    // save the last accel, and if didn't change by a certain threshold, then don't recaculate the value.

    int yMod = ttAccel.y / ACCEL_SEG_SIZE;
    
    coord_t targetPos;
    targetPos.r = activeTetrad.topLeft.r;
    targetPos.c = yMod + TETRAD_SPAWN_X;
    
    // Attempt to prevent jittering for gradual movements.
    if ((targetPos.c == activeTetrad.topLeft.c + 1 || 
        targetPos.c == activeTetrad.topLeft.c - 1) &&
        abs(ttAccel.y - ttLastTestAccel.y) <= ACCEL_JITTER_GUARD)
    {
        targetPos = activeTetrad.topLeft;
    }
    else
    {
        ttLastTestAccel = ttAccel;
    }

    bool moveClear = true;
    while (targetPos.c != activeTetrad.topLeft.c && moveClear) 
    {
        coord_t movePos = activeTetrad.topLeft;

        movePos.c = targetPos.c > movePos.c ? movePos.c + 1 : movePos.c - 1;
        
        if (checkCollision(movePos, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, activeTetrad.shape, GRID_WIDTH, GRID_HEIGHT, tetradsGrid))
        {
            moveClear = false;
        }
        else
        {
            activeTetrad.topLeft = movePos;
        }
    }
}

tetrad_t ICACHE_FLASH_ATTR spawnTetrad(tetradType_t type, coord_t gridCoord, int rotation)
{
    tetrad_t tetrad;
    tetrad.type = type;
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

void ICACHE_FLASH_ATTR spawnNextTetrad()
{
    coord_t spawnPos;
    spawnPos.c = TETRAD_SPAWN_X;
    spawnPos.r = TETRAD_SPAWN_Y;
    activeTetrad = spawnTetrad(nextTetradType, spawnPos, TETRAD_SPAWN_ROT);
    nextTetradType = (tetradType_t)getNextTetradType();

    // Check if this is blocked, if it is, the game is over.
    if (checkCollision(activeTetrad.topLeft, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, activeTetrad.shape, GRID_WIDTH, GRID_HEIGHT, tetradsGrid))
    {
        ttChangeState(TT_SCORES);
    }
    // If the game isn't over, move the initial tetrad to where it should be based on the accelerometer.
    else
    {
        moveTetrad();
    }
}

void ICACHE_FLASH_ATTR plotSquare(int x0, int y0, int size)
{
    plotRect(x0, y0, x0 + (size - 1), y0 + (size - 1));
}

void ICACHE_FLASH_ATTR plotGrid(int x0, int y0, uint8_t unitSize, uint8_t gridWidth, uint8_t gridHeight, uint8_t gridData[][gridWidth])
{
    // Draw the border
    plotRect(x0, y0, x0 + (unitSize - 1) * gridWidth, y0 + (unitSize - 1) * gridHeight);

    // Draw points for grid (maybe disable when not debugging)
    for (int y = 0; y < gridHeight; y++)
    {
        for (int x = 0; x < gridWidth; x++) 
        {
            if (gridData[y][x] == EMPTY) drawPixel(x0 + x * (unitSize - 1) + (unitSize / 2), y0 + y * (unitSize - 1) + (unitSize / 2), WHITE);
        }
    }
}

void ICACHE_FLASH_ATTR plotShape(int x0, int y0, uint8_t unitSize, uint8_t shapeWidth, uint8_t shapeHeight, uint8_t shape[][shapeWidth])
{   
    //TODO: different fill patterns.
    for (int y = 0; y < shapeHeight; y++)
    {
        for (int x = 0; x < shapeWidth; x++)
        {
            if (shape[y][x] != EMPTY) 
            {
                //plotSquare(x0+x*unitSize, y0+y*unitSize, unitSize);
                //top
                if (y == 0 || shape[y-1][x] == EMPTY)
                {
                    plotLine(x0 + x * (unitSize-1),     y0 + y * (unitSize-1), 
                             x0 + (x+1) * (unitSize-1), y0 + y * (unitSize-1));   
                }
                //bot
                if (y == shapeHeight-1 || shape[y+1][x] == EMPTY)
                {
                    plotLine(x0 + x * (unitSize-1),     y0 + (y+1) * (unitSize-1), 
                             x0 + (x+1) * (unitSize-1), y0 + (y+1) * (unitSize-1));   
                }
                
                //left
                if (x == 0 || shape[y][x-1] == EMPTY)
                {
                    plotLine(x0 + x * (unitSize-1), y0 + y * (unitSize-1), 
                             x0 + x * (unitSize-1), y0 + (y+1) * (unitSize-1));   
                }
                //right
                if (x == shapeWidth-1 || shape[y][x+1] == EMPTY)
                {
                    plotLine(x0 + (x+1) * (unitSize-1), y0 + y * (unitSize-1), 
                             x0 + (x+1) * (unitSize-1), y0 + (y+1) * (unitSize-1));   
                }
            }
        }
    }
}

int ICACHE_FLASH_ATTR getNextTetradType()
{
    return ((os_random() % 7) + 1); //TODO: do this well.
}

bool ICACHE_FLASH_ATTR isLineCleared(int line, uint8_t gridWidth, uint8_t gridHeight, uint8_t gridData[][gridWidth])
{
    bool clear = true;
    for (int c = 0; c < gridWidth; c++) 
    {
        if (gridData[line][c] == EMPTY) clear = false;
    }
    return clear;
}

void ICACHE_FLASH_ATTR checkLineClears(uint8_t gridWidth, uint8_t gridHeight, uint8_t gridData[][gridWidth])
{
    int lineClears = 0;
    for (int r = 0; r < gridHeight; r++) 
    {
        if (isLineCleared(r, gridWidth, gridHeight, gridData))
        {
            lineClears++;
            //TODO: actually remove lines.
            //TODO: alter shapes from fallen tetrads.
            //TODO: handle cascade implementation.    
            //for (int c = 0; c < gridWidth; c++) 
            //{
                //gridData[r][c] - 1 = index in the landedTetrads array.
                //landedTetrads[gridData[r][c] - 1].topLeft
                //topLeft.r < clearedLine.r -> topLeft.r--
                //topLeft.r >= clearedLine.r -> ?
                //how to handle a tetrad that exists above and below this line?
                //landedTetrads[gridData[r][c] - 1]
                //landedTetrads[gridData[r][c] - 1].shape[][] = EMPTY;
                for (int t = 0; t < numLandedTetrads; t++)
                {
                    bool hit = false;
                    // Go from bottom to top.
                    for (int tr = TETRAD_GRID_SIZE - 1; tr >= 0; tr--) 
                    {
                        for (int tc = 0; tc < TETRAD_GRID_SIZE; tc++) 
                        {
                            if (landedTetrads[t].shape[tr][tc] != EMPTY) 
                            {
                                coord_t gridPos;
                                gridPos.r = landedTetrads[t].topLeft.r + tr;
                                gridPos.c = landedTetrads[t].topLeft.c + tc;
                                if (gridPos.r == r)
                                {
                                    hit = true;
                                    landedTetrads[t].shape[tr][tc] = EMPTY;
                                }
                                else if (hit && gridPos.r < r)
                                {
                                    //TODO: what if it cannot be moved down anymore? Can this happen?
                                    if (tr < TETRAD_GRID_SIZE - 1)
                                    {
                                        // Copy the current space into the space below it.                                        
                                        landedTetrads[t].shape[tr+1][tc] = landedTetrads[t].shape[tr][tc];
                                        
                                        // Empty the current space.
                                        landedTetrads[t].shape[tr][tc] = EMPTY;
                                    }
                                }           
                            }
                        }
                    }

                    // move tetrads entirely above the line down by 1
                    if (!hit && landedTetrads[t].topLeft.r < r)
                    {
                        landedTetrads[t].topLeft.r++;
                    }
                }
            //}       
        }
    }
}

// what is the best way to handle collisions above the grid space?
bool ICACHE_FLASH_ATTR checkCollision(coord_t newPos, uint8_t shapeWidth, uint8_t shapeHeight, uint8_t shape[][shapeWidth], uint8_t gridWidth, uint8_t gridHeight, uint8_t gridData[][gridWidth])
{
    for (int r = 0; r < TETRAD_GRID_SIZE; r++)
    {
        for (int c = 0; c < TETRAD_GRID_SIZE; c++)
        {
            if (shape[r][c] != EMPTY)
            {
                if (newPos.r + r >= gridHeight || newPos.c + c >= gridWidth || newPos.c + c < 0 || (gridData[newPos.r + r][newPos.c + c] != EMPTY && gridData[newPos.r + r][newPos.c + c] != numLandedTetrads+1))
                {
                    return true;
                }
            }
        }
    }
    return false;
}

