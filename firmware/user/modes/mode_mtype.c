/*
 * mode_mtype.c
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
#include <math.h>

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
Start with the basic mechanics of r type, add a reflector shield with a CD, and more varied enemy attack patterns, etc.

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

#define BTN_GAME_UP UP_MASK
#define BTN_GAME_DOWN DOWN_MASK
#define BTN_GAME_LEFT LEFT_MASK
#define BTN_GAME_RIGHT RIGHT_MASK
#define BTN_GAME_ACTION ACTION_MASK

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

#define OWNER_PLAYER 0
#define OWNER_ENEMY 1
#define MAX_PROJECTILES 100
#define MAX_ENEMIES 100

#define PLAYER_SHOT_COOLDOWN (0.1875 * S_TO_MS_FACTOR * MS_TO_US_FACTOR)
#define PLAYER_REFLECT_CHARGE_MAX (2.5 * S_TO_MS_FACTOR * MS_TO_US_FACTOR)

#define PLAYER_REFLECT_TIME (1 * S_TO_MS_FACTOR * MS_TO_US_FACTOR)

#define CHUNK_WIDTH 8
#define NUM_CHUNKS ((OLED_WIDTH/CHUNK_WIDTH)+1)
#define RAND_WALLS_HEIGHT 4

typedef struct
{
    int16_t x;
    int16_t y;
} vec_t;

typedef struct 
{
    vec_t position;
    uint32_t shotCooldown;
    uint32_t reflectCounter;
    int reflectCountdown;
} player_t;

typedef struct 
{
    uint8_t active;
    vec_t position;
    vec_t spawn;
    uint32_t frameOffsetX;
    uint32_t frameOffsetY;
    uint8_t health;
} enemy_t;

typedef struct 
{
    uint8_t active; // is the projectile active / in-use.
    uint8_t owner; // is the projectile owned by players or enemies.
    vec_t position; // the current position of the projectile.
    vec_t direction; // the direction the projectile will move on update.
    uint8_t speed; // speed of the projectile.
    uint8_t damage; // the amount of damage the projectile will deal on hit.
} projectile_t;

typedef enum
{
    MT_TITLE,
    MT_GAME,
    MT_SCORES,
    MT_GAMEOVER
} mTypeState_t;

typedef struct
{
    mTypeState_t state;

    uint32_t modeStartTime; // time mode started in microseconds.
    uint32_t stateStartTime; // time the most recent state started in microseconds.
    uint32_t deltaTime; // time elapsed since last update in microseconds.
    uint32_t modeTime;  // total time the mode has been running in microseconds.
    uint32_t stateTime; // total time the state has been running in microseconds.
    uint32_t modeFrames; // total number of frames elapsed in this mode.
    uint32_t stateFrames; // total number of frames elapsed in this state.

    uint32_t score;

    player_t player;

    enemy_t enemies[MAX_ENEMIES];
    projectile_t projectiles[MAX_PROJECTILES];

    uint8_t floors[NUM_CHUNKS + 1];
    uint8_t xOffset;
    uint8_t floor;

    uint8_t buttonState;
    uint8_t lastButtonState;

    accel_t accel;
    accel_t lastAccel;

    timer_t updateTimer;

    menu_t* menu;

    //TODO: every game related variable should be contained within this.
} mType_t;

/*============================================================================
 * Prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR mtEnterMode(void);
void ICACHE_FLASH_ATTR mtExitMode(void);
void ICACHE_FLASH_ATTR mtButtonCallback(uint8_t state __attribute__((unused)), int button, int down);

static void ICACHE_FLASH_ATTR mtUpdate(void* arg __attribute__((unused)));

// handle inputs.
void ICACHE_FLASH_ATTR mtTitleInput(void);
void ICACHE_FLASH_ATTR mtGameInput(void);
void ICACHE_FLASH_ATTR mtScoresInput(void);
void ICACHE_FLASH_ATTR mtGameoverInput(void);

// update any input-unrelated logic.
void ICACHE_FLASH_ATTR mtTitleLogic(void);
void ICACHE_FLASH_ATTR mtGameLogic(void);
void ICACHE_FLASH_ATTR mtScoresLogic(void);
void ICACHE_FLASH_ATTR mtGameoverLogic(void);

// draw the frame.
void ICACHE_FLASH_ATTR mtTitleDisplay(void);
void ICACHE_FLASH_ATTR mtGameDisplay(void);
void ICACHE_FLASH_ATTR mtScoresDisplay(void);
void ICACHE_FLASH_ATTR mtGameoverDisplay(void);

// mode state management.
void ICACHE_FLASH_ATTR mtSetState(mTypeState_t newState);

static void ICACHE_FLASH_ATTR mtMenuCallback(const char* menuItem);

// input checking.
bool ICACHE_FLASH_ATTR mtIsButtonPressed(uint8_t button);
bool ICACHE_FLASH_ATTR mtIsButtonReleased(uint8_t button);
bool ICACHE_FLASH_ATTR mtIsButtonDown(uint8_t button);
bool ICACHE_FLASH_ATTR mtIsButtonUp(uint8_t button);

//TODO: drawing functions.

//TODO: score operations.
/*void ICACHE_FLASH_ATTR loadHighScores(void);
void ICACHE_FLASH_ATTR saveHighScores(void);
bool ICACHE_FLASH_ATTR updateHighScores(uint32_t newScore);*/

//TODO: LED FX functions.


uint8_t ICACHE_FLASH_ATTR getTextWidth(char* text, fonts font);

bool ICACHE_FLASH_ATTR AABBCollision (int ax0, int ay0, int ax1, int ay1, int bx0, int by0, int bx1, int by1);

/*============================================================================
 * Variables
 *==========================================================================*/

swadgeMode mTypeMode =
{
    .modeName = "mtype",
    .fnEnterMode = mtEnterMode,
    .fnExitMode = mtExitMode,
    .fnButtonCallback = mtButtonCallback,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = NULL,
    .fnAudioCallback = NULL,
    .menuImg = "copter-menu.gif" //TODO: need a menu image to link to here.
};

mType_t* mType;

static const char mt_title[]  = "M-TYPE";
static const char mt_start[]  = "START";
static const char mt_scores[] = "HIGH SCORES";
static const char mt_quit[]   = "QUIT";

/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * Initializer for mType
 */
void ICACHE_FLASH_ATTR mtEnterMode(void)
{
    // Give us responsive input.
    enableDebounce(false);

    // Alloc and clear everything.
    mType = os_malloc(sizeof(mType_t));
    ets_memset(mType, 0, sizeof(mType_t));

    // Reset mode time tracking.
    mType->modeStartTime = system_get_time();
    mType->modeTime = 0;
    mType->modeFrames = 0;

    // Reset input tracking.
    mType->buttonState = 0;
    mType->lastButtonState = 0;

    // TODO: is this the correct way to initialize this?
    ets_memset(&(mType->accel), 0, sizeof(accel_t));
    ets_memset(&(mType->lastAccel), 0, sizeof(accel_t));

    // Reset mode state.
    mType->state = MT_TITLE;
    mtSetState(MT_TITLE);

    // Init menu system.
    mType->menu = initMenu(mt_title, mtMenuCallback); //TODO: input handler for menu?
    addRowToMenu(mType->menu);
    addItemToRow(mType->menu, mt_start);
    addRowToMenu(mType->menu);
    addItemToRow(mType->menu, mt_scores);
    addRowToMenu(mType->menu);
    addItemToRow(mType->menu, mt_quit);
    drawMenu(mType->menu);

    // Start the update loop.
    timerDisarm(&(mType->updateTimer));
    timerSetFn(&(mType->updateTimer), mtUpdate, NULL);
    timerArm(&(mType->updateTimer), UPDATE_TIME_MS, true);

    //InitColorChord(); //TODO: Initialize preferred swadge LED behavior.
}

/**
 * Called when mType is exited
 */
void ICACHE_FLASH_ATTR mtExitMode(void)
{
    timerDisarm(&(mType->updateTimer));
    timerFlush();
    deinitMenu(mType->menu);
    //clear(&mType->obstacles);
    // TODO: free and clear everything.
    os_free(mType);
}

/**
 * TODO
 *
 * @param state  A bitmask of all button states, unused
 * @param button The button which triggered this event
 * @param down   true if the button was pressed, false if it was released
 */
void ICACHE_FLASH_ATTR mtButtonCallback( uint8_t state, int button, int down )
{
    mType->buttonState = state; // Set the state of all buttons
    if (mType->state == MT_TITLE && down) menuButton(mType->menu, button); // Pass input to menu if appropriate.
}

/**
 * @brief called on a timer, updates the game state
 *
 * @param arg
 */
static void ICACHE_FLASH_ATTR mtUpdate(void* arg __attribute__((unused)))
{
    // Update time tracking.
    // NOTE: delta time is in microseconds.
    // UPDATE time is in milliseconds.

    uint32_t newModeTime = system_get_time() - mType->modeStartTime;
    uint32_t newStateTime = system_get_time() - mType->stateStartTime;
    mType->deltaTime = newModeTime - mType->modeTime;
    mType->modeTime = newModeTime;
    mType->stateTime = newStateTime;
    mType->modeFrames++;
    mType->stateFrames++;

    // Handle Input
    switch( mType->state )
    {
        default:
        case MT_TITLE:
        {
            mtTitleInput();
            break;
        }
        case MT_GAME:
        {
            mtGameInput();
            break;
        }
        case MT_SCORES:
        {
            mtScoresInput();
            break;
        }
        case MT_GAMEOVER:
        {
            mtGameoverInput();
            break;
        }
    };

    // Mark what our inputs were the last time we acted on them.
    mType->lastButtonState = mType->buttonState;
    mType->lastAccel = mType->accel;

    // Handle State Logic
    switch( mType->state )
    {
        default:
        case MT_TITLE:
        {
            mtTitleLogic();
            break;
        }
        case MT_GAME:
        {
            mtGameLogic();
            break;
        }
        case MT_SCORES:
        {
            mtScoresLogic();
            break;
        }
        case MT_GAMEOVER:
        {
            mtGameoverLogic();
            break;
        }
    };

    // Handle Drawing Frame (based on the state)
    switch( mType->state )
    {
        default:
        case MT_TITLE:
        {
            mtTitleDisplay();
            break;
        }
        case MT_GAME:
        {
            mtGameDisplay();
            break;
        }
        case MT_SCORES:
        {
            mtScoresDisplay();
            break;
        }
        case MT_GAMEOVER:
        {
            mtGameoverDisplay();
            break;
        }
    };

    int projectiles = 0;

    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (mType->projectiles[i].active) {
            projectiles++;
        }
    }

    // Draw debug FPS counter.
    /*char uiStr[32] = {0};
    double seconds = ((double)mType->stateTime * (double)US_TO_MS_FACTOR * (double)MS_TO_S_FACTOR);
    int32_t fps = (int)((double)mType->stateFrames / seconds);
    ets_snprintf(uiStr, sizeof(uiStr), "FPS: %d", fps);
    plotText(OLED_WIDTH - getTextWidth(uiStr, TOM_THUMB) - 1, OLED_HEIGHT - (1 * (FONT_HEIGHT_TOMTHUMB + 1)), uiStr, TOM_THUMB, WHITE);*/
}

// helper functions.

void ICACHE_FLASH_ATTR mtSetState(mTypeState_t newState)
{
    mTypeState_t prevState = mType->state;
    mType->state = newState;
    mType->stateStartTime = system_get_time();
    mType->stateTime = 0;
    mType->stateFrames = 0;

    switch( mType->state )
    {
        default:
        case MT_TITLE:
            break;
        case MT_GAME:
            mType->player.position.x = 10;
            mType->player.position.y = OLED_HALF_HEIGHT;
            mType->player.shotCooldown = 0;
            mType->player.reflectCounter = 0;
            mType->player.reflectCountdown = 0;
            mType->score = 0;
            for (int i = 0; i < MAX_PROJECTILES; i++) {
                mType->projectiles[i].active = 0;
                mType->projectiles[i].owner = OWNER_PLAYER;
                mType->projectiles[i].position.x = 0;
                mType->projectiles[i].position.y = 0;
                mType->projectiles[i].direction.x = 0;
                mType->projectiles[i].direction.y = 0;
                mType->projectiles[i].speed = 1;
                mType->projectiles[i].damage = 1;
            }

            int enemyCounter = 0;
            for (int i = 0; i < 4; i++) {
                mType->enemies[enemyCounter].active = 1;
                mType->enemies[enemyCounter].health = 2;
                mType->enemies[enemyCounter].position.x = OLED_WIDTH - (10 * i);
                mType->enemies[enemyCounter].position.y = OLED_HEIGHT - 20;
                mType->enemies[enemyCounter].spawn.x = mType->enemies[enemyCounter].position.x;
                mType->enemies[enemyCounter].spawn.y = mType->enemies[enemyCounter].position.y;
                mType->enemies[enemyCounter].frameOffsetX = 0;
                mType->enemies[enemyCounter].frameOffsetY = i * 10;
                enemyCounter++;
            }

            for (int i = 0; i < 4; i++) {
                mType->enemies[enemyCounter].active = 1;
                mType->enemies[enemyCounter].health = 2;
                mType->enemies[enemyCounter].position.x = OLED_WIDTH - (10 * i);
                mType->enemies[enemyCounter].position.y = OLED_HEIGHT - 50;
                mType->enemies[enemyCounter].spawn.x = mType->enemies[enemyCounter].position.x;
                mType->enemies[enemyCounter].spawn.y = mType->enemies[enemyCounter].position.y;
                mType->enemies[enemyCounter].frameOffsetX = 0;
                mType->enemies[enemyCounter].frameOffsetY = i * 10;
                enemyCounter++;
            }

            mType->floor = OLED_HEIGHT - FONT_HEIGHT_TOMTHUMB - 3;//OLED_HEIGHT - 1;
            ets_memset(mType->floors, mType->floor, (NUM_CHUNKS + 1) * sizeof(uint8_t));
            mType->xOffset = 0;
            
            break;
        case MT_SCORES:
            break;
        case MT_GAMEOVER:
            break;
    };
}

bool ICACHE_FLASH_ATTR mtIsButtonPressed(uint8_t button)
{
    return (mType->buttonState & button) && !(mType->lastButtonState & button);
}

bool ICACHE_FLASH_ATTR mtIsButtonReleased(uint8_t button)
{
    return !(mType->buttonState & button) && (mType->lastButtonState & button);
}

bool ICACHE_FLASH_ATTR mtIsButtonDown(uint8_t button)
{
    return mType->buttonState & button;
}

bool ICACHE_FLASH_ATTR mtIsButtonUp(uint8_t button)
{
    return !(mType->buttonState & button);
}

void ICACHE_FLASH_ATTR mtTitleInput(void)
{
    
}

void ICACHE_FLASH_ATTR mtTitleLogic(void)
{

}

void ICACHE_FLASH_ATTR mtTitleDisplay(void)
{
    drawMenu(mType->menu);
}

void ICACHE_FLASH_ATTR mtGameInput(void)
{
    //TODO: account for good movement if multiple axis of movement are in play.
    if (mtIsButtonDown(BTN_GAME_UP)) {
        mType->player.position.y--;
    }
    if (mtIsButtonDown(BTN_GAME_DOWN)) {
        mType->player.position.y++;
    }
    if (mtIsButtonDown(BTN_GAME_LEFT)) {
        mType->player.position.x--;
    }
    if (mtIsButtonDown(BTN_GAME_RIGHT)) {
        mType->player.position.x++;
    }

    // clamp position of player to within the bounds of the screen.
    if (mType->player.position.x < 0) {
        mType->player.position.x = 0;
    }
    else if (mType->player.position.x >= OLED_WIDTH) {
        mType->player.position.x = OLED_WIDTH - 1;
    }
    if (mType->player.position.y < 0) {
        mType->player.position.y = 0;
    }
    else if (mType->player.position.y >= OLED_HEIGHT) {
        mType->player.position.y = OLED_HEIGHT - 1;
    }

    mType->player.shotCooldown += mType->deltaTime;

    if (mtIsButtonPressed(BTN_GAME_ACTION) && mType->player.reflectCounter >= PLAYER_REFLECT_CHARGE_MAX) {
        //TODO: dodge roll or charge beam or reflect shield?
        mType->player.reflectCounter = 0;
        mType->player.reflectCountdown = PLAYER_REFLECT_TIME;
    }   

    if (mtIsButtonDown(BTN_GAME_ACTION) && mType->player.shotCooldown >= PLAYER_SHOT_COOLDOWN) {

        //mType->player.reflectCounter = 0;
        for (int i = 0; i < MAX_PROJECTILES; i++) {
            if (!mType->projectiles[i].active) {
                mType->player.shotCooldown = 0;
                mType->projectiles[i].active = 1;
                mType->projectiles[i].owner = OWNER_PLAYER;
                mType->projectiles[i].position.x = mType->player.position.x;
                mType->projectiles[i].position.y = mType->player.position.y;
                mType->projectiles[i].direction.x = 1;
                mType->projectiles[i].direction.y = 0;
                mType->projectiles[i].speed = 4;
                mType->projectiles[i].damage = 1;
                break;
            }
        }
    }

    if (mtIsButtonUp(BTN_GAME_ACTION)) {
        mType->player.reflectCounter += mType->deltaTime;
        if (mType->player.reflectCounter > PLAYER_REFLECT_CHARGE_MAX) {
            mType->player.reflectCounter = PLAYER_REFLECT_CHARGE_MAX;
        }
    }

    mType->score++;
    // determine player move direction
    // determine if a projectile should be spawned
    // determine if the reflect shield should come up
}

void ICACHE_FLASH_ATTR mtGameLogic(void)
{
    // enemy movement and projectile spawning
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (mType->enemies[i].active) {
            mType->enemies[i].position.x = mType->enemies[i].spawn.x - ((mType->stateFrames - mType->enemies[i].frameOffsetX) / 3);
            if (mType->enemies[i].position.x < -10) {
                mType->enemies[i].frameOffsetX = mType->stateFrames;
                mType->enemies[i].spawn.x = OLED_WIDTH + 10;
            }
            mType->enemies[i].position.y = mType->enemies[i].spawn.y + (7 * sin(((mType->stateFrames + mType->enemies[i].frameOffsetY) / 25.0)));
        }
    }
    
    // projectile movement and collision
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (mType->projectiles[i].active) {
            mType->projectiles[i].position.x += (mType->projectiles[i].direction.x * mType->projectiles[i].speed);
            mType->projectiles[i].position.y += (mType->projectiles[i].direction.y * mType->projectiles[i].speed);

            //TODO: check collision with players / enemies
            if (mType->projectiles[i].owner == OWNER_PLAYER) {
                for (int j = 0; j < MAX_ENEMIES; j++) {
                    if (mType->enemies[j].active) {
                        if (AABBCollision(mType->projectiles[i].position.x, 
                            mType->projectiles[i].position.y, 
                            mType->projectiles[i].position.x, 
                            mType->projectiles[i].position.y, 
                            mType->enemies[j].position.x - 3, 
                            mType->enemies[j].position.y - 3, 
                            mType->enemies[j].position.x + 3, 
                            mType->enemies[j].position.y + 3)) {
                            mType->projectiles[i].active = 0;
                            mType->enemies[j].health -= mType->projectiles[i].damage;
                            if (mType->enemies[j].health <= 0) {
                                mType->enemies[j].active = 0;
                            }
                        }
                    }
                }
            }

            if (mType->projectiles[i].owner == OWNER_PLAYER) {

            }

            if (mType->projectiles[i].position.x >= OLED_WIDTH ||
                mType->projectiles[i].position.x < 0 ||
                mType->projectiles[i].position.y >= OLED_HEIGHT ||
                mType->projectiles[i].position.y < 0) {
                mType->projectiles[i].active = 0;
            }
        }   
    }

    if (mType->player.reflectCountdown > 0) {
        mType->player.reflectCountdown -= mType->deltaTime;
        if (mType->player.reflectCountdown < 0) {
            mType->player.reflectCountdown = 0;
        }
    }

    // Increment the X offset for other walls
    mType->xOffset++;

    // If we've moved CHUNK_WIDTH pixels
    if(mType->xOffset == CHUNK_WIDTH)
    {
        // Reset the X offset
        mType->xOffset = 0;

        // Shift all the chunk indices over one
        ets_memmove(&(mType->floors[0]), &(mType->floors[1]), NUM_CHUNKS);

        // Randomly generate new coordinates for a chunk
        mType->floors[NUM_CHUNKS] = mType->floor - (os_random() % RAND_WALLS_HEIGHT);
    }
}

void ICACHE_FLASH_ATTR mtGameDisplay(void)
{
    /*DEMO TODO:
    background / terrain fx
    ui for beam / reflect shield
    reflect shield animation
    enemy movement
    enemy firing projectiles
    enemy damage fx / explosions
    action button is restarting game bug
    */

    //clear the frame.
    fillDisplayArea(0, 0, OLED_WIDTH - 1, OLED_HEIGHT - 1, BLACK);

    // draw background
    // draw projectiles
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (mType->projectiles[i].active) {
            if (mType->projectiles[i].owner == OWNER_PLAYER) {
                plotLine(mType->projectiles[i].position.x, mType->projectiles[i].position.y, mType->projectiles[i].position.x - 5, mType->projectiles[i].position.y, WHITE);
            }
            else {
                plotLine(mType->projectiles[i].position.x, mType->projectiles[i].position.y, mType->projectiles[i].position.x, mType->projectiles[i].position.y, WHITE);
            }
        }   
    }
    // draw enemies
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (mType->enemies[i].active) {
            plotCircle(mType->enemies[i].position.x, mType->enemies[i].position.y, 4, WHITE);
            plotCircle(mType->enemies[i].position.x, mType->enemies[i].position.y, (mType->stateFrames / 15) % 2 ? 2 : 1, WHITE);
        }
    }
    // draw player
    #define PLAYER_HALF_WIDTH 3
    fillDisplayArea(mType->player.position.x - PLAYER_HALF_WIDTH, mType->player.position.y - PLAYER_HALF_WIDTH, mType->player.position.x + PLAYER_HALF_WIDTH, mType->player.position.y + PLAYER_HALF_WIDTH, BLACK);
    plotRect(mType->player.position.x - PLAYER_HALF_WIDTH, mType->player.position.y - PLAYER_HALF_WIDTH, mType->player.position.x + PLAYER_HALF_WIDTH, mType->player.position.y + PLAYER_HALF_WIDTH, WHITE);
    if (mType->player.reflectCountdown > 0) {
        plotCircle(mType->player.position.x, mType->player.position.y, ((mType->stateFrames / 2) % 3) + 4, WHITE);//mType->stateFrames % 3 ? WHITE : BLACK);
    }
    //plotCircle(mType->player.position.x, mType->player.position.y, 5, WHITE);
    // draw ui
    /*
    reflect text
    reflect bar rect container
    reflect line bar fill
    score #
    */

    char uiStr[32] = {0};

    // score text
    ets_snprintf(uiStr, sizeof(uiStr), "%06d", mType->score);
    int scoreTextX = 52;
    int scoreTextY = 1;

    fillDisplayArea(scoreTextX, 0, scoreTextX + 23, FONT_HEIGHT_TOMTHUMB + 1, BLACK);
    plotText(scoreTextX, scoreTextY, uiStr, TOM_THUMB, WHITE);

    int reflectTextX = 30;
    int reflectTextY = OLED_HEIGHT - (1 * (FONT_HEIGHT_TOMTHUMB + 1));

    int boundaryLineY = reflectTextY - 2;

    // fill ui area
    fillDisplayArea(0, boundaryLineY, OLED_WIDTH - 1, OLED_HEIGHT - 1, BLACK);

    // upper ui border
    //plotLine(0, boundaryLineY, OLED_WIDTH - 1, boundaryLineY, WHITE);

    // reflect text
    ets_snprintf(uiStr, sizeof(uiStr), "REFLECT");
    plotText(reflectTextX, reflectTextY, uiStr, TOM_THUMB, WHITE);

    // reflect bar container
    int reflectBarX0 = reflectTextX + getTextWidth(uiStr, TOM_THUMB) + 1;
    int reflectBarY0 = reflectTextY + 1;
    int reflectBarX1 = reflectBarX0 + 35;
    int reflectBarY1 = reflectBarY0 + 2;
    plotRect(reflectBarX0, reflectBarY0, reflectBarX1, reflectBarY1, WHITE);

    //reflect bar fill
    double charge = (double)mType->player.reflectCounter / PLAYER_REFLECT_CHARGE_MAX;
    int reflectBarFillX1 = reflectBarX0 + (charge * ((reflectBarX1 - 1) - reflectBarX0));
    plotLine(reflectBarX0, reflectBarY0 + 1, reflectBarFillX1, reflectBarY0 + 1, WHITE);

    // For each chunk coordinate
    for(uint8_t w = 0; w < NUM_CHUNKS + 1; w++)
    {
        // Plot a floor segment line between chunk coordinates
        plotLine(
            (w * CHUNK_WIDTH) - mType->xOffset,
            mType->floors[w],
            ((w + 1) * CHUNK_WIDTH) - mType->xOffset,
            mType->floors[w + 1],
            WHITE);
    }

    /*char uiStr[32] = {0};
    ets_snprintf(uiStr, sizeof(uiStr), "BTN: %d u:%d, d:%d, l:%d, r:%d, a:%d", button, upDown, downDown, leftDown, rightDown, actionDown);
    fillDisplayArea(OLED_WIDTH - getTextWidth(uiStr, TOM_THUMB) - 1, OLED_HEIGHT - (1 * (FONT_HEIGHT_TOMTHUMB + 1)), OLED_WIDTH, OLED_HEIGHT, BLACK);
    plotText(OLED_WIDTH - getTextWidth(uiStr, TOM_THUMB) - 1, OLED_HEIGHT - (1 * (FONT_HEIGHT_TOMTHUMB + 1)), uiStr, TOM_THUMB, WHITE);*/
    
}

void ICACHE_FLASH_ATTR mtScoresInput(void)
{

}

void ICACHE_FLASH_ATTR mtScoresLogic(void)
{

}

void ICACHE_FLASH_ATTR mtScoresDisplay(void)
{

}

void ICACHE_FLASH_ATTR mtGameoverInput(void)
{

}

void ICACHE_FLASH_ATTR mtGameoverLogic(void)
{

}

void ICACHE_FLASH_ATTR mtGameoverDisplay(void)
{

}

/**
 * TODO
 *
 * @param menuItem
 */
static void ICACHE_FLASH_ATTR mtMenuCallback(const char* menuItem)
{
    if (mt_start == menuItem)
    {
        // Change state to start game.
        mtSetState(MT_GAME);
    }
    else if (mt_scores == menuItem)
    {
        // Change state to score screen.
        mtSetState(MT_SCORES);
    }
    else if (mt_quit == menuItem)
    {
        // Exit this swadge mode.
        switchToSwadgeMode(0);
    }
}

uint8_t ICACHE_FLASH_ATTR getTextWidth(char* text, fonts font)
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

bool AABBCollision (int ax0, int ay0, int ax1, int ay1, int bx0, int by0, int bx1, int by1) {
    int awidth = ax1 - ax0;
    int aheight = ay1 - ay0;

    int bwidth = bx1 - bx0;
    int bheight = by1 - by0;

    return (ax0 < bx1 &&
            ax1 > bx0 &&
            ay0 < by1 &&
            ay1 > by0);
}
