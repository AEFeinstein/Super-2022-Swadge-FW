/*
*	mode_mazerf.c
*
*	Created on: 21 Sept 2019
*               Author: bbkiw
*		Refactor of maze using Jonathan Moriarty basic set up
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


#define ACCEL_SEG_SIZE 25 // higher value more or less means less sensetive.
#define ACCEL_JITTER_GUARD 14//7 // higher = less sensetive.

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


// coordinates on the playfield grid, not the screen.
typedef struct
{
    int c;
    int r;
} coord_t;


// Title screen info.

// Score screen info.
uint32_t clearScoreTimer;
bool holdingClearScore;

// Game state info.
uint32_t currentLevel; // The current difficulty level, increments every 10 line clears.
uint32_t score; // The current score this game.
uint32_t highScores[NUM_TT_HIGH_SCORES];
bool newHighScore;

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

// drawing functions.
void ICACHE_FLASH_ATTR plotSquare(int x0, int y0, int size, color col);
void ICACHE_FLASH_ATTR plotGrid(int x0, int y0, uint8_t unitSize, uint8_t gridWidth, uint8_t gridHeight, uint32_t gridData[][gridWidth], color col);
void ICACHE_FLASH_ATTR plotShape(int x0, int y0, uint8_t unitSize, uint8_t shapeWidth, uint8_t shapeHeight, uint32_t shape[][shapeWidth], color col);
void ICACHE_FLASH_ATTR plotCenteredText(uint8_t x0, uint8_t y, uint8_t x1, char* text, fonts font, color col);
uint8_t getTextWidth(char* text, fonts font);

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

bool ICACHE_FLASH_ATTR isLineCleared(int line, uint8_t gridWidth, uint8_t gridHeight, uint32_t gridData[][gridWidth]);
int ICACHE_FLASH_ATTR checkLineClears(uint8_t gridWidth, uint8_t gridHeight, uint32_t gridData[][gridWidth]);

bool ICACHE_FLASH_ATTR checkCollision(coord_t newPos, uint8_t shapeWidth, uint8_t shapeHeight, uint32_t shape[][shapeWidth], uint8_t gridWidth, uint8_t gridHeight, uint32_t gridData[][gridWidth], uint32_t selfGridValue);

swadgeMode mazerfMode = 
{
	.modeName = "Mazerf",
	.fnEnterMode = ttInit,
	.fnExitMode = ttDeInit,
	.fnButtonCallback = ttButtonCallback,
	.wifiMode = NO_WIFI,
	.fnEspNowRecvCb = NULL,
	.fnEspNowSendCb = NULL,
	.fnAccelerometerCallback = ttAccelerometerCallback
};

accel_t ttAccel = {0};
accel_t ttLastAccel = {0};

accel_t ttLastTestAccel = {0};

uint8_t ttButtonState = 0;
uint8_t ttLastButtonState = 0;

static os_timer_t timerHandleUpdate = {0};

uint32_t modeStartTime = 0; // time mode started in microseconds.
uint32_t stateStartTime = 0; // time the most recent state started in microseconds.
uint32_t deltaTime = 0;	// time elapsed since last update.
uint32_t modeTime = 0;	// total time the mode has been running.
uint32_t stateTime = 0;	// total time the game has been running.

tiltradsState_t currState = TT_TITLE;

void ICACHE_FLASH_ATTR ttInit(void)
{
    // Give us responsive input.
	enableDebounce(false);	
	
	// Reset mode time tracking.
	modeStartTime = system_get_time();
	modeTime = 0;

	// Reset state stuff.
	ttChangeState(TT_TITLE);

    // Grab any memory we need.
    initLandedTetrads();
    initTypeOrder();

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
    //moveTetrad(&tutorialTetrad, 1);
}

void ICACHE_FLASH_ATTR ttGameInput(void)
{
    //Refresh the tetrads grid.
    refreshTetradsGrid(false);

	//button a = rotate piece
    if(ttIsButtonPressed(BTN_GAME_ROTATE))
    {
        rotateTetrad(&activeTetrad);
    }

    //button b = soft drop piece
    if(ttIsButtonDown(BTN_GAME_DROP))
    {
        softDropTetrad();
    }

    // Only move tetrads left and right when the fast drop button isn't being held down.
    if(ttIsButtonUp(BTN_GAME_DROP))
    {
        moveTetrad(&activeTetrad);
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

}

void ICACHE_FLASH_ATTR ttGameUpdate(void)
{
    //Refresh the tetrads grid.
    refreshTetradsGrid(false);

    if (dropTimer >= dropTime)
    {
        dropTimer = 0;

        if (ttIsButtonDown(BTN_GAME_DROP))
        {
            score += SCORE_SOFT_DROP;
        }

        // If we couldn't drop, then we've landed.
        if (!dropTetrad(&(activeTetrad)))
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
            uint32_t linesClearedThisDrop = checkLineClears(GRID_WIDTH, GRID_HEIGHT, tetradsGrid);

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

            // Spawn the next tetrad.
            spawnNextTetrad(&activeTetrad, randomizer, tetradCounter+1);

            // Reset the drop info to whatever is appropriate for the current level.
            dropTime = getDropTime(currentLevel);
            dropTimer = 0;
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

        refreshTetradsGrid(false);

    }
}

void ICACHE_FLASH_ATTR ttScoresUpdate(void)
{
    // Do nothing.
}

void ICACHE_FLASH_ATTR ttGameoverUpdate(void)
{

}

void ICACHE_FLASH_ATTR ttTitleDisplay(void)
{
	// Clear the display.
    clearDisplay();

    // TILTRADS
    plotText(20, 5, "TILTRADS", RADIOSTARS, WHITE);
    
    // SCORES   START
    plotText(0, OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)), "SCORES", IBM_VGA_8, WHITE);
    plotText(OLED_WIDTH - getTextWidth("START", IBM_VGA_8), OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)), "START", IBM_VGA_8, WHITE);


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
    refreshTetradsGrid(true);

    // Draw the background grid. NOTE: (make sure everything that needs to be in tetradsGrid is in there now).
    plotGrid(GRID_X, GRID_Y, GRID_UNIT_SIZE, GRID_WIDTH, GRID_HEIGHT, tetradsGrid, WHITE);

    
    // NEXT
    plotCenteredText(GRID_X + (GRID_UNIT_SIZE * GRID_WIDTH), 0, OLED_WIDTH, "NEXT", TOM_THUMB, WHITE);

    // Draw the next tetrad.
    coord_t nextTetradPoint;
    nextTetradPoint.c = 1;
    nextTetradPoint.r = 1;
    tetrad_t nextTetrad = spawnTetrad(nextTetradType, tetradCounter+1, nextTetradPoint, 0);
    plotShape(NEXT_GRID_X + nextTetradPoint.c * (GRID_UNIT_SIZE - 1), NEXT_GRID_Y + nextTetradPoint.r * (GRID_UNIT_SIZE - 1), GRID_UNIT_SIZE, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, nextTetrad.shape, WHITE);

    // Draw the grid holding the next tetrad.
    clearGrid(NEXT_GRID_WIDTH, NEXT_GRID_HEIGHT, nextTetradGrid);
    copyGrid(nextTetrad.topLeft, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, nextTetrad.shape, NEXT_GRID_WIDTH, NEXT_GRID_HEIGHT, nextTetradGrid);
    plotGrid(NEXT_GRID_X, NEXT_GRID_Y, GRID_UNIT_SIZE, NEXT_GRID_WIDTH, NEXT_GRID_HEIGHT, nextTetradGrid, WHITE);

    //HIGH
    //99999
    newHighScore = score > highScores[0];
    plotCenteredText(0, 0, GRID_X, newHighScore ? "HIGH (NEW!)" : "HIGH", TOM_THUMB, WHITE);
    char uiStr[32] = {0};
    ets_snprintf(uiStr, sizeof(uiStr), "%d", newHighScore ? score : highScores[0]);
    plotCenteredText(0, (FONT_HEIGHT_TOMTHUMB + 1), GRID_X, uiStr, TOM_THUMB, WHITE);

    //SCORE
    //99999
    plotCenteredText(0, (3*FONT_HEIGHT_TOMTHUMB), GRID_X, "SCORE", TOM_THUMB, WHITE);
    ets_snprintf(uiStr, sizeof(uiStr), "%d", score);
    plotCenteredText(0, (4*FONT_HEIGHT_TOMTHUMB)+1, GRID_X, uiStr, TOM_THUMB, WHITE);

    //LINES
    // 999
    plotCenteredText(0, (6*FONT_HEIGHT_TOMTHUMB), GRID_X, "LINES", TOM_THUMB, WHITE);
    ets_snprintf(uiStr, sizeof(uiStr), "%d", linesClearedTotal);
    plotCenteredText(0, (7*FONT_HEIGHT_TOMTHUMB)+1, GRID_X, uiStr, TOM_THUMB, WHITE);

    //LEVEL
    // 99
    plotCenteredText(0, (9*FONT_HEIGHT_TOMTHUMB), GRID_X, "LEVEL", TOM_THUMB, WHITE);
    ets_snprintf(uiStr, sizeof(uiStr), "%d", (currentLevel+1)); // Levels are displayed with 1 as the base level.
    plotCenteredText(0, (10*FONT_HEIGHT_TOMTHUMB)+1, GRID_X, uiStr, TOM_THUMB, WHITE);
}

void ICACHE_FLASH_ATTR ttScoresDisplay(void)
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
    ets_snprintf(uiStr, sizeof(uiStr), "YOUR LAST SCORE: %d", ttGetLastScore());
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

void ICACHE_FLASH_ATTR ttGameoverDisplay(void)
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
            spawnNextTetrad(&activeTetrad, randomizer, tetradCounter+1);
            // Reset the drop info to whatever is appropriate for the current level.
            dropTime = getDropTime(currentLevel);
            dropTimer = 0;

            break;
        case TT_SCORES:
            loadHighScores();
            clearScoreTimer = 0;
            holdingClearScore = false;
            break;
        case TT_GAMEOVER:
            // Update high score if needed.
            newHighScore = updateHighScores(score);
            if (newHighScore) saveHighScores();
            
            // Save out the last score.
            ttSetLastScore(score);
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

void ICACHE_FLASH_ATTR refreshTetradsGrid(bool includeActive)
{
    clearGrid(GRID_WIDTH, GRID_HEIGHT, tetradsGrid);

    node_t * current = landedTetrads->first;
    for (int t = 0; t < landedTetrads->length; t++) 
    {
        tetrad_t * currentTetrad = (tetrad_t *)current->val;
        transferGrid(currentTetrad->topLeft, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, currentTetrad->shape, GRID_WIDTH, GRID_HEIGHT, tetradsGrid, t+1);
        current = current->next;
    }

    if (includeActive)
    {
        transferGrid(activeTetrad.topLeft, TETRAD_GRID_SIZE, TETRAD_GRID_SIZE, activeTetrad.shape, GRID_WIDTH, GRID_HEIGHT, tetradsGrid, activeTetrad.gridValue);
    }
}

// This assumes only complete tetrads can be rotated.
void ICACHE_FLASH_ATTR rotateTetrad(tetrad_t * tetrad)
{
}

void ICACHE_FLASH_ATTR softDropTetrad()
{
}

void ICACHE_FLASH_ATTR moveTetrad(tetrad_t * tetrad)
{
}

bool ICACHE_FLASH_ATTR dropTetrad(tetrad_t * tetrad)
{
}

tetrad_t ICACHE_FLASH_ATTR spawnTetrad(tetradType_t type, uint32_t gridValue, coord_t gridCoord, int rotation)
{
}

void ICACHE_FLASH_ATTR spawnNextTetrad(tetrad_t * newTetrad, tetradRandomizer_t randomType, uint32_t gridValue)
{
}

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
}

void ICACHE_FLASH_ATTR clearLandedTetrads()
{
}

void ICACHE_FLASH_ATTR deInitLandedTetrads()
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

