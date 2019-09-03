/*
*	mode_tiltrads.c
*
*	Created on: Aug 2, 2019
*		Author: Jonathan Moriarty
*/

#include <osapi.h>

#include "user_main.h"	//swadge mode
#include "buttons.h"
#include "oled.h"		//display functions
#include "font.h"		//draw text
#include "bresenham.h"	//draw shapes

// any defines go here.
// screen location offsets.
#define BTN_CTR_X 96
#define BTN_CTR_Y 40
#define BTN_RAD    8
#define BTN_OFF   12

#define BTN_START_GAME RIGHT
#define BTN_START_SCORES LEFT
#define BTN_START_TITLE LEFT
#define BTN_ROTATE LEFT
#define BTN_DROP RIGHT

// update task info.
#define UPDATE_TIME_MS 16 

// any enums go here.
typedef enum
{
    TT_TITLE,	// title screen
    TT_GAME,	// play the actual game
    TT_SCORES	// high scores / game over screen
	//TODO: does this need a transition state?
} tiltradsState_t;

typedef enum
{
    I_TETRAD,	
    O_TETRAD,	
    T_TETRAD,
    J_TETRAD,
    L_TETRAD,
    S_TETRAD,
    Z_TETRAD
} tetrad_t;

/*
//TODO: this can just be modeled with int and modulus, easier math but is it performant on an ESP?
typedef enum
{
    ROT_0, //spawn
    ROT_1,	
    ROT_2,
    ROT_3
} tetradRotations_t;*/

// function prototypes go here.
void ICACHE_FLASH_ATTR ttInit(void);
void ICACHE_FLASH_ATTR ttDeInit(void);
void ICACHE_FLASH_ATTR ttButtonCallback(uint8_t state, int button, int down);
void ICACHE_FLASH_ATTR ttAccelerometerCallback(accel_t* accel);

// game loop functions.
static void ICACHE_FLASH_ATTR ttUpdate(void* arg);

void ICACHE_FLASH_ATTR ttTitleInput(void);
void ICACHE_FLASH_ATTR ttGameInput(void);
void ICACHE_FLASH_ATTR ttScoresInput(void);

void ICACHE_FLASH_ATTR ttTitleDisplay(void);
void ICACHE_FLASH_ATTR ttGameDisplay(void);
void ICACHE_FLASH_ATTR ttScoresDisplay(void);

// helper functions.
void ICACHE_FLASH_ATTR ttChangeState(tiltradsState_t newState);
bool ICACHE_FLASH_ATTR ttIsButtonPressed(uint8_t button);
void ICACHE_FLASH_ATTR drawTetrad(int x0, int y0, tetrad_t type, int rotation, uint8_t unitSize, bool drawBounds);
void ICACHE_FLASH_ATTR plotITetrad(int x0, int y0, int rotation, uint8_t unitSize);
void ICACHE_FLASH_ATTR plotOTetrad(int x0, int y0, int rotation, uint8_t unitSize);
void ICACHE_FLASH_ATTR plotTTetrad(int x0, int y0, int rotation, uint8_t unitSize);
void ICACHE_FLASH_ATTR plotJTetrad(int x0, int y0, int rotation, uint8_t unitSize);
void ICACHE_FLASH_ATTR plotLTetrad(int x0, int y0, int rotation, uint8_t unitSize);
void ICACHE_FLASH_ATTR plotSTetrad(int x0, int y0, int rotation, uint8_t unitSize);
void ICACHE_FLASH_ATTR plotZTetrad(int x0, int y0, int rotation, uint8_t unitSize);
void ICACHE_FLASH_ATTR rotateTetrad();
void ICACHE_FLASH_ATTR dropTetrad();
void ICACHE_FLASH_ATTR plotSquare(int x0, int y0, int size);
void ICACHE_FLASH_ATTR plotGrid(int x0, int y0, int xUnits, int yUnits, int unitSize, bool drawGridLines);

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

void ICACHE_FLASH_ATTR ttButtonCallback(uint8_t state, int button, int down)
{
	ttButtonState = state;	// Set the state of all buttons
}

void ICACHE_FLASH_ATTR ttAccelerometerCallback(accel_t* accel)
{
	ttAccel.x = accel->x;	// Set the accelerometer values
    ttAccel.y = accel->y;
    ttAccel.z = accel->z;
    //ttUpdateDisplay();		// Update the display
}

static void ICACHE_FLASH_ATTR ttUpdate(void* arg)
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
    };

    // Mark what our inputs were the last time we acted on them.
    ttLastButtonState = ttButtonState;

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

int testRotation = 0;

void ICACHE_FLASH_ATTR ttGameDisplay(void)
{
    // Clear the display
    clearDisplay();

    // vertical screen orientation.
    //plotGrid(0, 0, 20, 10, 4);

    // horizontal screen orientation.
    plotGrid(45, -1, 10, 16, 4, (testRotation % 2 == 0));

    // next piece area
    plotGrid(45+40+4*3, 3, 6, 4, 4, (testRotation % 2 == 0));
    
    //TODO: the next piece (centered)

    //TODO: score
    //TODO: lines
    //TODO: level
    //TODO: the current high score?

    
    // straight test
    /*int posX = 5;
    int posY = 20;
    int testUnitSize = 4; 
    drawTetrad(posX, posY, I_TETRAD, testRotation, testUnitSize, true);

    // square test
    posX += 25;
    drawTetrad(posX, posY, O_TETRAD, testRotation, testUnitSize, true);

    // J test
    posX += 25;
    drawTetrad(posX, posY, J_TETRAD, testRotation, testUnitSize, true);

    // L test
    posX += 20;
    drawTetrad(posX, posY, L_TETRAD, testRotation, testUnitSize, true);

    // T test
    posX += 20;
    drawTetrad(posX, posY, T_TETRAD, testRotation, testUnitSize, true);
    
    // S test
    posX = 5;
    posY += 25;
    drawTetrad(posX, posY, S_TETRAD, testRotation, testUnitSize, true);

    // Z test
    posX += 20;
    drawTetrad(posX, posY, Z_TETRAD, testRotation, testUnitSize, true);*/
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
}

bool ICACHE_FLASH_ATTR ttIsButtonPressed(uint8_t button)
{
    //TODO: can press events get lost this way?
    return (ttButtonState & button) && !(ttLastButtonState & button);
}

void ICACHE_FLASH_ATTR drawTetrad(int x0, int y0, tetrad_t type, int rotation, uint8_t unitSize, bool drawBounds)
{
    if(drawBounds)
    {
        switch(type)
        {
            case I_TETRAD:
			    plotSquare(x0,y0,unitSize*4);
                break;
            case O_TETRAD:
			    plotRect(x0,y0,x0+unitSize*4,y0+unitSize*3);
                break;
            default: 
                plotSquare(x0,y0,unitSize*3);
        }
    }

    switch(type)
    {
        case I_TETRAD:
		    plotITetrad(x0,y0,rotation,unitSize);
            break;
        case O_TETRAD:
		    plotOTetrad(x0,y0,rotation,unitSize);
            break;
        case T_TETRAD:
		    plotTTetrad(x0,y0,rotation,unitSize);
            break;
        case J_TETRAD:
		    plotJTetrad(x0,y0,rotation,unitSize);
            break;
        case L_TETRAD:
		    plotLTetrad(x0,y0,rotation,unitSize);
            break;
        case S_TETRAD:
		    plotSTetrad(x0,y0,rotation,unitSize);
            break;
        case Z_TETRAD:
		    plotZTetrad(x0,y0,rotation,unitSize);
            break;
    }
}

void ICACHE_FLASH_ATTR plotITetrad(int x0, int y0, int rotation, uint8_t unitSize)
{
    rotation = rotation%4;
    switch(rotation)
    {
        case 0:
            plotSquare(x0,              y0+unitSize,    unitSize);
            plotSquare(x0+unitSize,     y0+unitSize,    unitSize);
            plotSquare(x0+unitSize*2,   y0+unitSize,    unitSize);
            plotSquare(x0+unitSize*3,   y0+unitSize,    unitSize);
            break;
        case 1:
            plotSquare(x0+unitSize*2,   y0,             unitSize);
            plotSquare(x0+unitSize*2,   y0+unitSize,    unitSize);
            plotSquare(x0+unitSize*2,   y0+unitSize*2,  unitSize);
            plotSquare(x0+unitSize*2,   y0+unitSize*3,  unitSize);
            break;
        case 2:
            plotSquare(x0,              y0+unitSize*2,  unitSize);
            plotSquare(x0+unitSize,     y0+unitSize*2,  unitSize);
            plotSquare(x0+unitSize*2,   y0+unitSize*2,  unitSize);
            plotSquare(x0+unitSize*3,   y0+unitSize*2,  unitSize);
            break;
        case 3:
            plotSquare(x0+unitSize*1,   y0,             unitSize);
            plotSquare(x0+unitSize*1,   y0+unitSize,    unitSize);
            plotSquare(x0+unitSize*1,   y0+unitSize*2,  unitSize);
            plotSquare(x0+unitSize*1,   y0+unitSize*3,  unitSize);
            break;
    }
}

void ICACHE_FLASH_ATTR plotOTetrad(int x0, int y0, int rotation, uint8_t unitSize)
{
    plotSquare(x0+unitSize,     y0,             unitSize);
    plotSquare(x0+unitSize*2,   y0,             unitSize);
    plotSquare(x0+unitSize,     y0+unitSize,    unitSize);
    plotSquare(x0+unitSize*2,   y0+unitSize,    unitSize);
}

void ICACHE_FLASH_ATTR plotTTetrad(int x0, int y0, int rotation, uint8_t unitSize)
{
    rotation = rotation%4;
    switch(rotation)
    {
        case 0:
            plotSquare(x0+unitSize,     y0,             unitSize);
            plotSquare(x0,              y0+unitSize,    unitSize);
            plotSquare(x0+unitSize,     y0+unitSize,    unitSize);
            plotSquare(x0+unitSize*2,   y0+unitSize,    unitSize);
            break;
        case 1:
            plotSquare(x0+unitSize,   y0,             unitSize);
            plotSquare(x0+unitSize,   y0+unitSize,    unitSize);
            plotSquare(x0+unitSize,   y0+unitSize*2,  unitSize);
            plotSquare(x0+unitSize*2, y0+unitSize,    unitSize);
            break;
        case 2:
            plotSquare(x0+unitSize,     y0+unitSize*2,  unitSize);
            plotSquare(x0,              y0+unitSize,    unitSize);
            plotSquare(x0+unitSize,     y0+unitSize,    unitSize);
            plotSquare(x0+unitSize*2,   y0+unitSize,    unitSize);
            break;
        case 3:
            plotSquare(x0+unitSize,   y0,             unitSize);
            plotSquare(x0+unitSize,   y0+unitSize,    unitSize);
            plotSquare(x0+unitSize,   y0+unitSize*2,  unitSize);
            plotSquare(x0,            y0+unitSize,    unitSize);
            break;
    }
}

void ICACHE_FLASH_ATTR plotJTetrad(int x0, int y0, int rotation, uint8_t unitSize)
{
    rotation = rotation%4;
    switch(rotation)
    {
        case 0:
            plotSquare(x0,              y0,             unitSize);
            plotSquare(x0,              y0+unitSize,    unitSize);
            plotSquare(x0+unitSize,     y0+unitSize,    unitSize);
            plotSquare(x0+unitSize*2,   y0+unitSize,    unitSize);
            break;
        case 1:
            plotSquare(x0+unitSize,   y0,             unitSize);
            plotSquare(x0+unitSize*2, y0,             unitSize);
            plotSquare(x0+unitSize,   y0+unitSize,    unitSize);
            plotSquare(x0+unitSize,   y0+unitSize*2,  unitSize);
            break;
        case 2:
            plotSquare(x0,              y0+unitSize,    unitSize);
            plotSquare(x0+unitSize,     y0+unitSize,    unitSize);
            plotSquare(x0+unitSize*2,   y0+unitSize,    unitSize);
            plotSquare(x0+unitSize*2,   y0+unitSize*2,  unitSize);
            break;
        case 3:
            plotSquare(x0+unitSize,   y0,             unitSize);
            plotSquare(x0+unitSize,   y0+unitSize,    unitSize);
            plotSquare(x0+unitSize,   y0+unitSize*2,  unitSize);
            plotSquare(x0,            y0+unitSize*2,  unitSize);
            break;
    }
}

void ICACHE_FLASH_ATTR plotLTetrad(int x0, int y0, int rotation, uint8_t unitSize)
{
    rotation = rotation%4;
    switch(rotation)
    {
        case 0:
            plotSquare(x0+unitSize*2,   y0,             unitSize);
            plotSquare(x0,              y0+unitSize,    unitSize);
            plotSquare(x0+unitSize,     y0+unitSize,    unitSize);
            plotSquare(x0+unitSize*2,   y0+unitSize,    unitSize);
            break;
        case 1:
            plotSquare(x0+unitSize,     y0,             unitSize);
            plotSquare(x0+unitSize,     y0+unitSize,    unitSize);
            plotSquare(x0+unitSize,     y0+unitSize*2,  unitSize);
            plotSquare(x0+unitSize*2,   y0+unitSize*2,  unitSize);
            break;
        case 2:
            plotSquare(x0,              y0+unitSize*2,  unitSize);
            plotSquare(x0,              y0+unitSize,    unitSize);
            plotSquare(x0+unitSize,     y0+unitSize,    unitSize);
            plotSquare(x0+unitSize*2,   y0+unitSize,    unitSize);
            break;
        case 3:
            plotSquare(x0,            y0,             unitSize);
            plotSquare(x0+unitSize,   y0,             unitSize);
            plotSquare(x0+unitSize,   y0+unitSize,    unitSize);
            plotSquare(x0+unitSize,   y0+unitSize*2,  unitSize);
            break;
    }
}

void ICACHE_FLASH_ATTR plotSTetrad(int x0, int y0, int rotation, uint8_t unitSize)
{
    rotation = rotation%4;
    switch(rotation)
    {
        case 0:
            plotSquare(x0,              y0+unitSize,    unitSize);
            plotSquare(x0+unitSize,     y0+unitSize,    unitSize);
            plotSquare(x0+unitSize,     y0,             unitSize);
            plotSquare(x0+unitSize*2,   y0,             unitSize);
            break;
        case 1:
            plotSquare(x0+unitSize,     y0,             unitSize);
            plotSquare(x0+unitSize,     y0+unitSize,    unitSize);
            plotSquare(x0+unitSize*2,   y0+unitSize,    unitSize);
            plotSquare(x0+unitSize*2,   y0+unitSize*2,  unitSize);
            break;
        case 2:
            plotSquare(x0,              y0+unitSize*2,  unitSize);
            plotSquare(x0+unitSize,     y0+unitSize*2,  unitSize);
            plotSquare(x0+unitSize,     y0+unitSize,    unitSize);
            plotSquare(x0+unitSize*2,   y0+unitSize,    unitSize);
            break;
        case 3:
            plotSquare(x0,              y0,             unitSize);
            plotSquare(x0,              y0+unitSize,    unitSize);
            plotSquare(x0+unitSize,     y0+unitSize,    unitSize);
            plotSquare(x0+unitSize,     y0+unitSize*2,  unitSize);
            break;
    }
}

void ICACHE_FLASH_ATTR plotZTetrad(int x0, int y0, int rotation, uint8_t unitSize)
{
    rotation = rotation%4;
    switch(rotation)
    {
        case 0:
            plotSquare(x0,              y0,             unitSize);
            plotSquare(x0+unitSize,     y0,             unitSize);
            plotSquare(x0+unitSize,     y0+unitSize,    unitSize);
            plotSquare(x0+unitSize*2,   y0+unitSize,    unitSize);
            break;
        case 1:
            plotSquare(x0+unitSize,     y0+unitSize*2,  unitSize);
            plotSquare(x0+unitSize,     y0+unitSize,    unitSize);
            plotSquare(x0+unitSize*2,   y0+unitSize,    unitSize);
            plotSquare(x0+unitSize*2,   y0,             unitSize);
            break;
        case 2:
            plotSquare(x0,              y0+unitSize,    unitSize);
            plotSquare(x0+unitSize,     y0+unitSize,    unitSize);
            plotSquare(x0+unitSize,     y0+unitSize*2,  unitSize);
            plotSquare(x0+unitSize*2,   y0+unitSize*2,  unitSize);
            break;
        case 3:
            plotSquare(x0,            y0+unitSize*2,  unitSize);
            plotSquare(x0,            y0+unitSize,    unitSize);
            plotSquare(x0+unitSize,   y0+unitSize,    unitSize);
            plotSquare(x0+unitSize,   y0,             unitSize);
            break;
    }
}

void ICACHE_FLASH_ATTR rotateTetrad()
{
    //TODO: fill in. does this need a rot direction as parameter?
    testRotation++;
}

void ICACHE_FLASH_ATTR dropTetrad()
{
    //TODO: fill in.
}

void ICACHE_FLASH_ATTR plotSquare(int x0, int y0, int size)
{
    plotRect(x0, y0, x0 + size, y0 + size);
}

void ICACHE_FLASH_ATTR plotGrid(int x0, int y0, int xUnits, int yUnits, int unitSize, bool drawGridLines)
{
    if (drawGridLines) 
    {
        for (int x = 0; x < xUnits; x++)
        {
            for (int y = 0; y < yUnits; y++) 
            {
                plotSquare(x0+x*unitSize, y0+y*unitSize, unitSize);
            }
        }
    }
    else
    {
        plotRect(x0, y0, x0 + unitSize * xUnits, y0 + unitSize * yUnits);
    }
}

