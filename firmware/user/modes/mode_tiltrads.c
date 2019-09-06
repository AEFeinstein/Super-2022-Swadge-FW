/*
*	mode_tiltrads.c
*
*	Created on: Aug 2, 2019
*		Author: Jonathan Moriarty
*/

#include <osapi.h>
#include <user_interface.h>

#include "user_main.h"	//swadge mode
#include "buttons.h"
#include "oled.h"		//display functions
#include "font.h"		//draw text
#include "bresenham.h"	//draw shapes

// https://gamedevelopment.tutsplus.com/tutorials/implementing-tetris-collision-detection--gamedev-852
// https://simon.lc/the-history-of-tetris-randomizers

// any defines go here.

#define BTN_START_GAME RIGHT
#define BTN_START_SCORES LEFT
#define BTN_START_TITLE LEFT
#define BTN_ROTATE LEFT
#define BTN_DROP RIGHT

// update task info.
#define UPDATE_TIME_MS 16 

// playfield settings.
#define GRID_X 45
#define GRID_Y -1
#define GRID_UNIT_SIZE 4
#define GRID_WIDTH 10
#define GRID_HEIGHT 16

#define NEXT_GRID_X 97
#define NEXT_GRID_Y 3
#define NEXT_GRID_WIDTH 5
#define NEXT_GRID_HEIGHT 5

#define EMPTY 0
#define MAX_TETRADS 160

#define TETRAD_SPAWN_ROT 0
#define TETRAD_SPAWN_X 3
#define TETRAD_SPAWN_Y -1

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

uint8_t iTetradRotations [4][4][4] = 
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

uint8_t oTetradRotations [4][4][4] = 
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

uint8_t tTetradRotations [4][4][4] = 
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

uint8_t jTetradRotations [4][4][4] = 
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

uint8_t lTetradRotations [4][4][4] = 
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

uint8_t sTetradRotations [4][4][4] = 
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

uint8_t zTetradRotations [4][4][4] = 
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
    int x;
    int y;
} coord_t;

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
    uint8_t shape[4][4];
} tetrad_t;

tetrad_t activeTetrad;

tetradType_t nextTetradType;

int numLandedTetrads;
tetrad_t landedTetrads[MAX_TETRADS];

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
void ICACHE_FLASH_ATTR copyGrid(coord_t srcOffset, uint8_t srcWidth, uint8_t srcHeight, uint8_t src[][srcWidth], uint8_t dstWidth, uint8_t dstHeight, uint8_t dst[][dstWidth]);
void ICACHE_FLASH_ATTR clearGrid(uint8_t gridWidth, uint8_t gridHeight, uint8_t gridData[][gridWidth]);
void ICACHE_FLASH_ATTR rotateTetrad(void);
void ICACHE_FLASH_ATTR dropTetrad(void);
tetrad_t ICACHE_FLASH_ATTR spawnTetrad(tetradType_t type, coord_t gridCoord, int rotation);
void ICACHE_FLASH_ATTR spawnNextTetrad(void);
void ICACHE_FLASH_ATTR plotSquare(int x0, int y0, int size);
void ICACHE_FLASH_ATTR plotGrid(int x0, int y0, uint8_t unitSize, uint8_t gridWidth, uint8_t gridHeight, uint8_t gridData[][gridWidth]);
void ICACHE_FLASH_ATTR plotShape(int x0, int y0, uint8_t unitSize, uint8_t shapeWidth, uint8_t shapeHeight, uint8_t shape[][shapeWidth]);
int ICACHE_FLASH_ATTR getNextTetradType(void);

//bool ICACHE_FLASH_ATTR attemptTetradMove(tetrad_t target, coord_t newPos, uint8_t gridWidth, uint8_t gridHeight, uint8_t gridData[][gridWidth]);

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
    if(ttIsButtonPressed(BTN_DROP))
    {
        dropTetrad();
    }
    
	//TODO: accel = tilt the current tetrad

    //TODO: this is a debug input, remove it when done.
    if(ttIsButtonPressed(DOWN))
    {
        ttChangeState(TT_SCORES);
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
    // drop the piece if it's time to drop it
    // if a piece is landed, spawn a new tetrad
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
    plotText(25, 0, "TILTRADS", RADIOSTARS);
    //plotText(35, (FONT_HEIGHT_RADIOSTARS + 1), "TILTRADS", RADIOSTARS);

    // Display the acceleration on the display
    char accelStr[32] = {0};

    ets_snprintf(accelStr, sizeof(accelStr), "X:%d", ttAccel.x);
    plotText(0, OLED_HEIGHT - (3 * (FONT_HEIGHT_IBMVGA8 + 1)), accelStr, IBM_VGA_8);

    ets_snprintf(accelStr, sizeof(accelStr), "Y:%d", ttAccel.y);
    plotText(0, OLED_HEIGHT - (2 * (FONT_HEIGHT_IBMVGA8 + 1)), accelStr, IBM_VGA_8);

    ets_snprintf(accelStr, sizeof(accelStr), "Z:%d", ttAccel.z);
    plotText(0, OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)), accelStr, IBM_VGA_8);
}

void ICACHE_FLASH_ATTR ttGameDisplay(void)
{
    // Clear the display
    clearDisplay();
    
    // Clear the grid data (may not want to do this every frame)
    clearGrid(GRID_WIDTH, GRID_HEIGHT, tetradsGrid);

    // Draw the active tetrad.
    plotShape(GRID_X + activeTetrad.topLeft.x * GRID_UNIT_SIZE, GRID_Y + activeTetrad.topLeft.y * GRID_UNIT_SIZE, GRID_UNIT_SIZE, 4, 4, activeTetrad.shape);   
    copyGrid(activeTetrad.topLeft, 4, 4, activeTetrad.shape, GRID_WIDTH, GRID_HEIGHT, tetradsGrid);

    // Draw all the landed tetrads.
    for (int i = 0; i < numLandedTetrads; i++) 
    {
        plotShape(GRID_X + landedTetrads[i].topLeft.x * GRID_UNIT_SIZE, GRID_Y + landedTetrads[i].topLeft.y * GRID_UNIT_SIZE, GRID_UNIT_SIZE, 4, 4, landedTetrads[i].shape);   
        copyGrid(landedTetrads[i].topLeft, 4, 4, landedTetrads[i].shape, GRID_WIDTH, GRID_HEIGHT, tetradsGrid);
    }

    // Draw the background grid. NOTE: (make sure everything that needs to be in tetradsGrid is in there now).
    plotGrid(GRID_X, GRID_Y, GRID_UNIT_SIZE, GRID_WIDTH, GRID_HEIGHT, tetradsGrid);

    
    // Draw the next tetrad.
    clearGrid(NEXT_GRID_WIDTH, NEXT_GRID_HEIGHT, nextTetradGrid);

    coord_t nextTetradPoint;
    nextTetradPoint.x = 1;
    nextTetradPoint.y = 1;

    tetrad_t nextTetrad = spawnTetrad(nextTetradType, nextTetradPoint, 0);
    plotShape(NEXT_GRID_X + nextTetradPoint.x * GRID_UNIT_SIZE, NEXT_GRID_Y + nextTetradPoint.y * GRID_UNIT_SIZE, GRID_UNIT_SIZE, 4, 4, nextTetrad.shape);   
    copyGrid(nextTetrad.topLeft, 4, 4, nextTetrad.shape, NEXT_GRID_WIDTH, NEXT_GRID_HEIGHT, nextTetradGrid);

    plotGrid(NEXT_GRID_X, NEXT_GRID_Y, GRID_UNIT_SIZE, NEXT_GRID_WIDTH, NEXT_GRID_HEIGHT, nextTetradGrid);

    // Debug text:
    /*char debugStr[32] = {0};
    ets_snprintf(debugStr, sizeof(debugStr), "ROT:%d", testRotation);
    plotText(0, 0, debugStr, IBM_VGA_8);*/

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

    plotText(0, 0, "SCORES", RADIOSTARS);
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

void ICACHE_FLASH_ATTR copyGrid(coord_t srcOffset, uint8_t srcWidth, uint8_t srcHeight, uint8_t src[][srcWidth], uint8_t dstWidth, uint8_t dstHeight, uint8_t dst[][dstWidth])
{
    for (int y = 0; y < srcHeight; y++)
    {
        for (int x = 0; x < srcWidth; x++)
        {
            int dstX = x + srcOffset.x;
            int dstY = y + srcOffset.y;
            if (dstX < dstWidth && dstY < dstHeight) dst[dstY][dstX] = src[y][x];
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

void ICACHE_FLASH_ATTR rotateTetrad()
{
    activeTetrad.rotation++;
    coord_t origin;
    origin.x = 0;
    origin.y = 0;
    switch (activeTetrad.type)
    {
        case I_TETRAD:
			copyGrid(origin, 4, 4, iTetradRotations[activeTetrad.rotation % 4], 4, 4, activeTetrad.shape);
            break;
        case O_TETRAD:
		    copyGrid(origin, 4, 4, oTetradRotations[activeTetrad.rotation % 4], 4, 4, activeTetrad.shape);
            break;
        case T_TETRAD:
		    copyGrid(origin, 4, 4, tTetradRotations[activeTetrad.rotation % 4], 4, 4, activeTetrad.shape);
            break;
        case J_TETRAD:
		    copyGrid(origin, 4, 4, jTetradRotations[activeTetrad.rotation % 4], 4, 4, activeTetrad.shape);
            break;
        case L_TETRAD:
		    copyGrid(origin, 4, 4, lTetradRotations[activeTetrad.rotation % 4], 4, 4, activeTetrad.shape);
            break;
        case S_TETRAD:
		    copyGrid(origin, 4, 4, sTetradRotations[activeTetrad.rotation % 4], 4, 4, activeTetrad.shape);
            break;
        case Z_TETRAD:
		    copyGrid(origin, 4, 4, zTetradRotations[activeTetrad.rotation % 4], 4, 4, activeTetrad.shape);
            break;
        default:
            break;
    }
}

void ICACHE_FLASH_ATTR dropTetrad()
{
    //TODO: fill in.
    //testTetradType++;
}

tetrad_t ICACHE_FLASH_ATTR spawnTetrad(tetradType_t type, coord_t gridCoord, int rotation)
{
    tetrad_t tetrad;
    tetrad.type = type;
    tetrad.rotation = rotation;
    tetrad.topLeft = gridCoord;
    coord_t origin;
    origin.x = 0;
    origin.y = 0;
    switch (tetrad.type)
    {
        case I_TETRAD:
			copyGrid(origin, 4, 4, iTetradRotations[rotation], 4, 4, tetrad.shape);
            break;
        case O_TETRAD:
		    copyGrid(origin, 4, 4, oTetradRotations[rotation], 4, 4, tetrad.shape);
            break;
        case T_TETRAD:
		    copyGrid(origin, 4, 4, tTetradRotations[rotation], 4, 4, tetrad.shape);
            break;
        case J_TETRAD:
		    copyGrid(origin, 4, 4, jTetradRotations[rotation], 4, 4, tetrad.shape);
            break;
        case L_TETRAD:
		    copyGrid(origin, 4, 4, lTetradRotations[rotation], 4, 4, tetrad.shape);
            break;
        case S_TETRAD:
		    copyGrid(origin, 4, 4, sTetradRotations[rotation], 4, 4, tetrad.shape);
            break;
        case Z_TETRAD:
		    copyGrid(origin, 4, 4, zTetradRotations[rotation], 4, 4, tetrad.shape);
            break;
        default:
            break;
    }
    return tetrad;
}

void ICACHE_FLASH_ATTR spawnNextTetrad()
{
    coord_t spawnPos;
    spawnPos.x = TETRAD_SPAWN_X;
    spawnPos.y = TETRAD_SPAWN_Y;
    activeTetrad = spawnTetrad(nextTetradType, spawnPos, TETRAD_SPAWN_ROT);
    nextTetradType = (tetradType_t)getNextTetradType();
}

void ICACHE_FLASH_ATTR plotSquare(int x0, int y0, int size)
{
    plotRect(x0, y0, x0 + size, y0 + size);
}

void ICACHE_FLASH_ATTR plotGrid(int x0, int y0, uint8_t unitSize, uint8_t gridWidth, uint8_t gridHeight, uint8_t gridData[][gridWidth])
{
    // Draw the border
    plotRect(x0, y0, x0 + unitSize * gridWidth, y0 + unitSize * gridHeight);

    // Draw points for grid (maybe disable when not debugging)
    for (int y = 0; y < gridHeight; y++)
    {
        for (int x = 0; x < gridWidth; x++) 
        {
            if (gridData[y][x] == EMPTY) plotLine(x0+x*unitSize, y0+y*unitSize, x0+x*unitSize, y0+y*unitSize);
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
                    plotLine(x0+x*unitSize, y0+y*unitSize, x0+x*unitSize+unitSize, y0+y*unitSize);   
                }
                //bot
                if (y == shapeHeight-1 || shape[y+1][x] == EMPTY)
                {
                    plotLine(x0+x*unitSize, y0+y*unitSize+unitSize, x0+x*unitSize+unitSize, y0+y*unitSize+unitSize);   
                }
                
                //left
                if (x == 0 || shape[y][x-1] == EMPTY)
                {
                    plotLine(x0+x*unitSize, y0+y*unitSize, x0+x*unitSize, y0+y*unitSize+unitSize);   
                }
                //right
                if (x == shapeWidth-1 || shape[y][x+1] == EMPTY)
                {
                    plotLine(x0+x*unitSize+unitSize, y0+y*unitSize, x0+x*unitSize+unitSize, y0+y*unitSize+unitSize);   
                }
            }
        }
    }
}

int ICACHE_FLASH_ATTR getNextTetradType()
{
    return ((os_random() % 7) + 1); //TODO: do this well.
}

