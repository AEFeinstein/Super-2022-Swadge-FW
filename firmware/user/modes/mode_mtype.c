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
#include "cndraw.h"
#include "assets.h"
#include "buttons.h"
#include "menu2d.h"
#include "linked_list.h"
#include "font.h"

#include "embeddednf.h"
#include "embeddedout.h"

#include "mode_mtype.h"
#include "nvm_interface.h"
//NOTES:
/*

This is a Swadge Mode which has states, the mode updates in different ways depending on the current state.
An Update consists of detecting and handling INPUT -> running any game LOGIC that is unrelated to input -> DISPLAY to the user the current mode state.

TODO:
Refine enemy wave generator to ensure better spacing between waves and formations.
Refine enemy behavior so that there's always time for a player to react.

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

// gameplay consts.

#define OWNER_PLAYER 0
#define OWNER_ENEMY 1

#define TYPE_BOLT 0

#define PWRUP_FP 0
//#define PWRUP_REFLECT 1
//#define PWRUP_CHARGE 2
//#define PWRUP_LIFETIME (10 * S_TO_MS_FACTOR * MS_TO_US_FACTOR)
#define PWRUP_DRIFT_DELAY (3 * S_TO_MS_FACTOR * MS_TO_US_FACTOR)

#define ENEMY_SNAKE 0
#define ENEMY_BOMBER 1
#define ENEMY_WALKER 2

#define MAX_PROJECTILES 50
#define MAX_ENEMIES 50
#define MAX_EXPLOSIONS MAX_ENEMIES
#define MAX_POWERUPS 10

#define PLAYER_SPEED 1

#define PLAYER_SHOT_COOLDOWN (0.1875 * S_TO_MS_FACTOR * MS_TO_US_FACTOR)
#define PLAYER_REFLECT_CHARGE_MAX (1.5 * S_TO_MS_FACTOR * MS_TO_US_FACTOR)
#define PLAYER_REFLECT_TIME (1 * S_TO_MS_FACTOR * MS_TO_US_FACTOR)

#define GAMEOVER_START_TIME (3 * S_TO_MS_FACTOR * MS_TO_US_FACTOR)

#define PLAYER_REFLECT_COLLISION 3

#define PLAYER_HALF_WIDTH 3
#define PLAYER_HALF_HEIGHT 3
#define PLAYER_START_X 10
#define PLAYER_START_Y OLED_HALF_HEIGHT

#define PLAYER_PROJECTILE_SPEED 3
#define PLAYER_PROJECTILE_DAMAGE 1
#define PLAYER_START_LIVES 3
#define PLAYER_INVINCIBILITY_TIME (3.0 * S_TO_MS_FACTOR * MS_TO_US_FACTOR)

#define EXPLOSION_FRAMES 6

#define ENEMY_PROJECTILE_SPEED 1
#define ENEMY_PROJECTILE_DAMAGE 1

#define ENEMY_SNAKE_SHOT_COOLDOWN (3.5 * S_TO_MS_FACTOR * MS_TO_US_FACTOR)
#define ENEMY_BOMBER_SHOT_COOLDOWN (0.5 * S_TO_MS_FACTOR * MS_TO_US_FACTOR)
#define ENEMY_WALKER_SHOT_COOLDOWN (2 * S_TO_MS_FACTOR * MS_TO_US_FACTOR)

#define ENEMY_WAVE_EMPTY_TIME (10.0 * S_TO_MS_FACTOR * MS_TO_US_FACTOR)

// score vars.
#define ENEMY_KILL 10
#define WAVE_CLEAR_BONUS 100
#define REFLECT_RAM_BONUS 2
#define REFLECT_KILL_BONUS 10
#define POWERUP_GET_BONUS 1000

#define DIFFICULTY_EASY 1
#define DIFFICULTY_MEDIUM 2
#define DIFFICULTY_HARD 3
#define DIFFICULTY_VERYHARD 4

// floor terrain consts.
#define CHUNK_WIDTH 8
#define NUM_CHUNKS ((OLED_WIDTH/CHUNK_WIDTH)+1)
#define RAND_WALLS_HEIGHT 4

const led_t titleColor =
{
    .r = 0x00,
    .g = 0xFF,
    .b = 0xFF
};

const led_t scoresColor =
{
    .r = 0xFF,
    .g = 0xFF,
    .b = 0x00
};

const led_t gameoverColor =
{
    .r = 0xFF,
    .g = 0x00,
    .b = 0x00
};

const led_t reflectColor =
{
    .r = 0x00,
    .g = 0x00,
    .b = 0xFF
};

const led_t shotColor =
{
    .r = 0xFF,
    .g = 0xFF,
    .b = 0x00
};

// vector struct specifically for use with screen coordinates.
typedef struct
{
    int16_t x;
    int16_t y;
} vec_t;

// precise positions for things like movement of the player.
typedef struct
{
    float x;
    float y;
} vecfloat_t;

typedef struct 
{
    vecfloat_t position; // position in screen coords. (float for precise movement)
    vecfloat_t lastPosition; // position of player last frame.
    vec_t bbHalf;   // half of bounding box width / height.
    uint8_t speed;  // speed of the player.
    uint8_t shotLevel;  // weapon level, controls the amount of bullets fired.
    uint32_t shotCooldown;  // cooldown between firing shots.
    uint32_t abilityChargeCounter;    // counter for ability charge.
    int abilityCountdown;   // counter for how long ability will last once active.
    uint8_t numLives;
    int invincibilityCountdown; // counter for spawn invincibility.
} player_t;

typedef struct 
{
    uint8_t active; // is the projectile in-use on screen.
    uint8_t type; // the type of the projectile.
    uint8_t originalOwner;  // was the projectile originally owned by players or enemies.
    uint8_t owner;  // is the projectile owned by players or enemies.
    vecfloat_t position; // the current position of the projectile.
    vec_t bbHalf; // half of bounding box width / height.
    vecfloat_t direction;    // the direction the projectile will move on update.
    uint8_t speed;  // speed of the projectile.
    uint8_t damage; // the amount of damage the projectile will deal on hit.
} projectile_t;

typedef struct 
{
    uint8_t active; // is the explosion in-use on screen.
    vec_t position; // position in screen coords.
    vec_t bbHalf;
    uint8_t frame; // current explosion frame.
} explosion_t;

typedef struct
{
    uint8_t active;
    uint8_t type;
    vec_t position;
    vec_t bbHalf;
    uint8_t speed;  // drift speed of the powerup.
    uint32_t driftDelay;  // drift speed of the powerup.
} powerup_t;

typedef struct 
{
    uint8_t active; // is enemy in-use on screen.
    uint8_t type; // the type of the enemy.
    vec_t position; // position in screen coords.
    vec_t bbHalf;   // half of bounding box width / height.
    vec_t spawn;    // position that the enemy was spawned at in screen coords.
    //uint8_t speed;  // speed of the enemy.
    //vecfloat_t direction;  // speed of the enemy.
    int32_t frameOffset;  // how much time in frames certain movement is offset.
    uint32_t shotCooldown;  // cooldown between firing shots.
    int8_t health; // the health of the enemy.
} enemy_t;

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

    uint8_t difficulty; // the selected difficulty for the game.

    uint32_t score;
    uint32_t wave; // enemy wave number.
    uint32_t waveEmptyTime; // amount of time there have been no enemies on screen.
    uint32_t enemiesInWave; // number of enemies remaining in this wave.
    uint32_t enemiesOnScreen; // number of enemies visible on screen.
    int32_t gameoverCountdown; // how long after final player death the game over state starts 

    player_t player;

    enemy_t enemies[MAX_ENEMIES];
    projectile_t projectiles[MAX_PROJECTILES];
    explosion_t explosions[MAX_EXPLOSIONS];
    powerup_t powerups[MAX_POWERUPS];

    uint8_t floors[NUM_CHUNKS + 1];
    uint8_t xOffset;
    uint8_t floor;

    uint8_t buttonState;
    uint8_t lastButtonState;

    timer_t updateTimer;

    menu_t* titleMenu;
    menu_t* gameoverMenu;

    mtHighScores_t highScores;

    led_t leds[NUM_LEDS];

    pngHandle playerUpHandle;
    pngHandle playerDownHandle;
    pngHandle playerStraightHandle;

    pngHandle powerupHandle;

    pngSequenceHandle bomberSequenceHandle;
    pngSequenceHandle snakeSequenceHandle;
    pngSequenceHandle walkerSequenceHandle;
    pngSequenceHandle explosionSequenceHandle;
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

// menu callbacks.
static void ICACHE_FLASH_ATTR mtTitleMenuCallback(const char* menuItem);
static void ICACHE_FLASH_ATTR mtGameoverMenuCallback(const char* menuItem);

// input checking.
bool ICACHE_FLASH_ATTR mtIsButtonPressed(uint8_t button);
bool ICACHE_FLASH_ATTR mtIsButtonReleased(uint8_t button);
bool ICACHE_FLASH_ATTR mtIsButtonDown(uint8_t button);
bool ICACHE_FLASH_ATTR mtIsButtonUp(uint8_t button);

// LED FX functions.
void ICACHE_FLASH_ATTR singlePulseLEDs(uint8_t numLEDs, led_t fxColor, float progress);
void ICACHE_FLASH_ATTR blinkLEDs(uint8_t numLEDs, led_t fxColor, uint32_t time);
void ICACHE_FLASH_ATTR alternatingPulseLEDS(uint8_t numLEDs, led_t fxColor, uint32_t time);
void ICACHE_FLASH_ATTR dancingLEDs(uint8_t numLEDs, led_t fxColor, uint32_t time);
//void ICACHE_FLASH_ATTR countdownLEDs(uint8_t numLEDs, led_t fxColor, float progress);
void ICACHE_FLASH_ATTR clearLEDs(uint8_t numLEDs);
void ICACHE_FLASH_ATTR applyLEDBrightness(uint8_t numLEDs, float brightness);

bool ICACHE_FLASH_ATTR submitMTScore(uint8_t difficulty, uint32_t timeSurvived, uint32_t score);
uint8_t ICACHE_FLASH_ATTR getTextWidth(char* text, fonts font);
bool ICACHE_FLASH_ATTR AABBCollision (int ax0, int ay0, int ax1, int ay1, int bx0, int by0, int bx1, int by1);
void ICACHE_FLASH_ATTR normalize (vecfloat_t * vec);
bool ICACHE_FLASH_ATTR fireProjectile (uint8_t owner, uint8_t type, vec_t position, vec_t bbHalf, vecfloat_t direction, uint8_t speed, uint8_t damage);
bool ICACHE_FLASH_ATTR spawnExplosion (vec_t spawn, vec_t bbHalf);
bool ICACHE_FLASH_ATTR spawnEnemy (uint8_t type, vec_t spawn, int8_t health, vec_t bbHalf, int32_t frameOffset);
void ICACHE_FLASH_ATTR spawnEnemyFormation (uint8_t type, vec_t spawn, int8_t health, vec_t bbHalf, int32_t frameOffset, uint8_t numEnemies, int16_t xSpacing, int16_t ySpacing);
void ICACHE_FLASH_ATTR enemyDeath (uint8_t index);
void ICACHE_FLASH_ATTR playerDeath (void);


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
    .menuImg = "mtype-menu.gif" 
};

mType_t* mType;

static const char mt_title[]  = "M-TYPE";
static const char mt_easy[]   = "EASY";
static const char mt_medium[] = "MEDIUM";
static const char mt_hard[]   = "HARD";
static const char mt_veryhard[]   = "DIE FOR ME";
static const char mt_scores[] = "HIGH SCORES";
static const char mt_quit[]   = "QUIT";
static const char mt_gameover[] = "GAME OVER";
static const char mt_restart[]  = "RESTART";
static const char mt_menu[]  = "MENU";

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

    allocPngAsset("mt-pu.png", &mType->playerUpHandle);
    allocPngAsset("mt-pd.png", &mType->playerDownHandle);
    allocPngAsset("mt-ps.png", &mType->playerStraightHandle);
    allocPngAsset("mt-powerup.png", &mType->powerupHandle);

    allocPngSequence(&mType->bomberSequenceHandle, 2,
                     "mt-bomber1.png",
                     "mt-bomber2.png");

    allocPngSequence(&mType->snakeSequenceHandle, 2,
                     "mt-snake1.png",
                     "mt-snake2.png");

    allocPngSequence(&mType->walkerSequenceHandle, 2,
                     "mt-walker1.png",
                     "mt-walker2.png");

    allocPngSequence(&mType->explosionSequenceHandle, 3,
                     "mt-explode1.png",
                     "mt-explode2.png",
                     "mt-explode3.png");

    // Reset mode time tracking.
    mType->modeStartTime = system_get_time();
    mType->modeTime = 0;
    mType->modeFrames = 0;

    // Reset input tracking.
    mType->buttonState = 0;
    mType->lastButtonState = 0;

    // Grab high scores.
    getMTScores(&mType->highScores);

    // Reset mode state.
    mType->state = MT_TITLE;
    mtSetState(MT_TITLE);

    // Init title menu system.
    mType->titleMenu = initMenu(mt_title, mtTitleMenuCallback);
    addRowToMenu(mType->titleMenu);

    addItemToRow(mType->titleMenu, mt_easy);
    addItemToRow(mType->titleMenu, mt_medium);
    addItemToRow(mType->titleMenu, mt_hard);
    addItemToRow(mType->titleMenu, mt_veryhard);
    addRowToMenu(mType->titleMenu);

    addItemToRow(mType->titleMenu, mt_scores);
    addRowToMenu(mType->titleMenu);
    addItemToRow(mType->titleMenu, mt_quit);

    mType->gameoverMenu = initMenu(mt_gameover, mtGameoverMenuCallback);
    addRowToMenu(mType->gameoverMenu);
    addItemToRow(mType->gameoverMenu, mt_restart);
    addRowToMenu(mType->gameoverMenu);
    addItemToRow(mType->gameoverMenu, mt_menu);
    addRowToMenu(mType->gameoverMenu);
    addItemToRow(mType->gameoverMenu, mt_quit);

    // Start the update loop.
    timerDisarm(&(mType->updateTimer));
    timerSetFn(&(mType->updateTimer), mtUpdate, NULL);
    timerArm(&(mType->updateTimer), UPDATE_TIME_MS, true);

    clearLEDs(NUM_LEDS); // initialize preferred swadge LED behavior.
}

/**
 * Called when mType is exited
 */
void ICACHE_FLASH_ATTR mtExitMode(void)
{
    timerDisarm(&(mType->updateTimer));
    timerFlush();
    deinitMenu(mType->titleMenu);
    deinitMenu(mType->gameoverMenu);
    
    // free and clear everything.
    freePngAsset(&mType->playerUpHandle);
    freePngAsset(&mType->playerDownHandle);
    freePngAsset(&mType->playerStraightHandle);
    freePngAsset(&mType->powerupHandle);

    freePngSequence(&mType->explosionSequenceHandle);
    freePngSequence(&mType->bomberSequenceHandle);
    freePngSequence(&mType->snakeSequenceHandle);
    freePngSequence(&mType->walkerSequenceHandle);
    os_free(mType);
}

/**
 * 
 *
 * @param state  A bitmask of all button states, unused
 * @param button The button which triggered this event
 * @param down   true if the button was pressed, false if it was released
 */
void ICACHE_FLASH_ATTR mtButtonCallback( uint8_t state, int button, int down )
{
    mType->buttonState = state; // Set the state of all buttons
    if (mType->state == MT_TITLE && down) menuButton(mType->titleMenu, button); // Pass input to menu if appropriate.
    if (mType->state == MT_GAMEOVER && down) menuButton(mType->gameoverMenu, button); // Pass input to menu if appropriate.
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

    // Draw debug FPS counter.
    /*char uiStr[32] = {0};
    float seconds = ((float)mType->stateTime * (float)US_TO_MS_FACTOR * (float)MS_TO_S_FACTOR);
    int32_t fps = (int)((float)mType->stateFrames / seconds);
    //ets_snprintf(uiStr, sizeof(uiStr), "FPS: %d", fps);
    ets_snprintf(uiStr, sizeof(uiStr), "W:%d E:%d", mType->wave, mType->enemiesInWave);
    plotText(OLED_WIDTH - getTextWidth(uiStr, TOM_THUMB) - 1, OLED_HEIGHT - (1 * (FONT_HEIGHT_TOMTHUMB + 1)), uiStr, TOM_THUMB, WHITE);*/
}

// helper functions.

void ICACHE_FLASH_ATTR mtSetState(mTypeState_t newState)
{
    // mTypeState_t prevState = mType->state;
    mType->state = newState;
    mType->stateStartTime = system_get_time();
    mType->stateTime = 0;
    mType->stateFrames = 0;

    clearLEDs(NUM_LEDS);

    switch( mType->state )
    {
        default:
        case MT_TITLE:
            dancingLEDs(NUM_LEDS, titleColor, mType->stateTime);
            break;
        case MT_GAME:
            // initialize player.
            mType->player.position.x = PLAYER_START_X;
            mType->player.position.y = PLAYER_START_Y;
            mType->player.lastPosition.x = PLAYER_START_X;
            mType->player.lastPosition.y = PLAYER_START_Y;
            mType->player.bbHalf.x = PLAYER_HALF_WIDTH;
            mType->player.bbHalf.y = PLAYER_HALF_HEIGHT;
            mType->player.speed = PLAYER_SPEED;
            mType->player.shotLevel = 0;
            mType->player.shotCooldown = PLAYER_SHOT_COOLDOWN;
            mType->player.abilityChargeCounter = 0;
            mType->player.abilityCountdown = 0;
            mType->player.numLives = PLAYER_START_LIVES;
            mType->player.invincibilityCountdown = 0;
            mType->score = 0;
            mType->wave = 0;
            mType->waveEmptyTime = 0;
            mType->enemiesInWave = 0;
            mType->enemiesOnScreen = 0;
            mType->gameoverCountdown = GAMEOVER_START_TIME;

            // initialize projectiles with default values.
            for (int i = 0; i < MAX_PROJECTILES; i++) {
                mType->projectiles[i].active = 0;
                mType->projectiles[i].type = 0;
                mType->projectiles[i].owner = OWNER_PLAYER;
                mType->projectiles[i].position.x = 0;
                mType->projectiles[i].position.y = 0;
                mType->projectiles[i].bbHalf.x = 1;
                mType->projectiles[i].bbHalf.y = 1;
                mType->projectiles[i].direction.x = 0;
                mType->projectiles[i].direction.y = 0;
                mType->projectiles[i].speed = 1;
                mType->projectiles[i].damage = 1;
            }

            // initialize explosions with default values.
            for (int i = 0; i < MAX_EXPLOSIONS; i++) {
                mType->explosions[i].active = 0;
                mType->explosions[i].position.x = 0;
                mType->explosions[i].position.y = 0;
                mType->explosions[i].bbHalf.x = 0;
                mType->explosions[i].bbHalf.y = 0;
                mType->explosions[i].frame = 0;
            }

            // initialize powerups with default values.
            for (int i = 0; i < MAX_POWERUPS; i++) {
                mType->powerups[i].active = 0;
                mType->powerups[i].type = 0;
                mType->powerups[i].position.x = 0;
                mType->powerups[i].position.y = 0;
                mType->powerups[i].bbHalf.x = 1;
                mType->powerups[i].bbHalf.y = 1;
                mType->powerups[i].speed = 1;
                mType->powerups[i].driftDelay = 0;
            }

            // initialize enemies deactivated with default values.
            for (int i = 0; i < MAX_ENEMIES; i++) {
                mType->enemies[i].active = 0;
                mType->enemies[i].type = 0;
                mType->enemies[i].health = 0;
                mType->enemies[i].position.x = 0;
                mType->enemies[i].position.y = 0;
                mType->enemies[i].bbHalf.x = 0;
                mType->enemies[i].bbHalf.y = 0;
                mType->enemies[i].spawn.x = 0;
                mType->enemies[i].spawn.y = 0;
                //mType->enemies[i].speed = 0;
                //mType->enemies[i].direction.x = 0;
                //mType->enemies[i].direction.y = 0;
                mType->enemies[i].frameOffset = 0;
                mType->enemies[i].shotCooldown = 0;
            }

            // initialize the floor / terrain display.
            mType->floor = OLED_HEIGHT - FONT_HEIGHT_TOMTHUMB - 3;
            ets_memset(mType->floors, mType->floor, (NUM_CHUNKS + 1) * sizeof(uint8_t));
            mType->xOffset = 0;
            break;
        case MT_SCORES:
            // prevent score screen from ending as a result of the press that started it.
            mType->lastButtonState = mType->buttonState;
            alternatingPulseLEDS(NUM_LEDS, scoresColor, mType->modeTime);
            break;
        case MT_GAMEOVER:
            blinkLEDs(NUM_LEDS, gameoverColor, mType->stateTime);
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

// a color is puled all leds according to the type of clear.
void ICACHE_FLASH_ATTR singlePulseLEDs(uint8_t numLEDs, led_t fxColor, float progress)
{
    float lightness = 1.0 - (progress * progress);

    for (int32_t i = 0; i < numLEDs; i++)
    {
        mType->leds[i].r = (uint8_t)((float)fxColor.r * lightness);
        mType->leds[i].g = (uint8_t)((float)fxColor.g * lightness);
        mType->leds[i].b = (uint8_t)((float)fxColor.b * lightness);
    }

    applyLEDBrightness(numLEDs, MODE_LED_BRIGHTNESS);
    setLeds(mType->leds, sizeof(mType->leds));
}

// blink red in sync with OLED gameover FX.
void ICACHE_FLASH_ATTR blinkLEDs(uint8_t numLEDs, led_t fxColor, uint32_t time)
{
    //TODO: there are instances where the red flashes on the opposite of the fill draw, how to ensure this does not happen?
    uint32_t animCycle = ((float)time * US_TO_MS_FACTOR) / DISPLAY_REFRESH_MS;
    bool lightActive = animCycle % 2 == 0;

    for (int32_t i = 0; i < numLEDs; i++)
    {
        mType->leds[i].r = lightActive ? fxColor.r : 0x00;
        mType->leds[i].g = lightActive ? fxColor.g : 0x00;
        mType->leds[i].b = lightActive ? fxColor.b : 0x00;
    }
    
    applyLEDBrightness(numLEDs, MODE_LED_BRIGHTNESS);
    setLeds(mType->leds, sizeof(mType->leds));
}

// alternate lit up like a bulb sign
void ICACHE_FLASH_ATTR alternatingPulseLEDS(uint8_t numLEDs, led_t fxColor, uint32_t time)
{
    float timeS = (float)time * US_TO_MS_FACTOR * MS_TO_S_FACTOR;
    float risingProgress = (sin(timeS * 4.0) + 1.0) / 2.0;
    float fallingProgress = 1.0 - risingProgress;

    float risingR = risingProgress * (float)fxColor.r;
    float risingG = risingProgress * (float)fxColor.g;
    float risingB = risingProgress * (float)fxColor.b;

    float fallingR = fallingProgress * (float)fxColor.r;
    float fallingG = fallingProgress * (float)fxColor.g;
    float fallingB = fallingProgress * (float)fxColor.b;

    bool risingLED;

    for (int32_t i = 0; i < numLEDs; i++)
    {
        risingLED = i % 2 == 0;
        mType->leds[i].r = risingLED ? (uint8_t)risingR : (uint8_t)fallingR;
        mType->leds[i].g = risingLED ? (uint8_t)risingG : (uint8_t)fallingG;
        mType->leds[i].b = risingLED ? (uint8_t)risingB : (uint8_t)fallingB;
    }

    applyLEDBrightness(numLEDs, MODE_LED_BRIGHTNESS);
    setLeds(mType->leds, sizeof(mType->leds));
}

// radial wanderers.
void ICACHE_FLASH_ATTR dancingLEDs(uint8_t numLEDs, led_t fxColor, uint32_t time)
{
    uint32_t animCycle = ((float)time * US_TO_MS_FACTOR * 2.0) / DISPLAY_REFRESH_MS;
    int32_t firstIndex = animCycle % numLEDs;
    int32_t secondIndex = (firstIndex + (numLEDs / 2)) % numLEDs;

    //uint8_t timeMS = ((float)time * US_TO_MS_FACTOR)/400;

    for (int32_t i = 0; i < numLEDs; i++)
    {
        mType->leds[i].r = i == firstIndex || i == secondIndex ? fxColor.r : 0x00;
        mType->leds[i].g = i == firstIndex || i == secondIndex ? fxColor.g : 0x00;
        mType->leds[i].b = i == firstIndex || i == secondIndex ? fxColor.b : 0x00;
    }

    applyLEDBrightness(numLEDs, MODE_LED_BRIGHTNESS);
    setLeds(mType->leds, sizeof(mType->leds));
}

/*void ICACHE_FLASH_ATTR countdownLEDs(uint8_t numLEDs, led_t fxColor, float progress)
{
    // Reverse the direction of progress.
    progress = 1.0 - progress;

    // How many LEDs will be fully lit.
    uint8_t numLitLEDs = progress * numLEDs;

    // Get the length of each segment of progress.
    float segment = 1.0 / numLEDs;
    float segmentProgress = numLitLEDs * segment;
    // Find the amount that the leading LED should be partially lit.
    float modProgress = (progress - segmentProgress) / segment;

    for (int32_t i = 0; i < numLEDs; i++)
    {
        if (i < numLitLEDs)
        {
            mType->leds[i].r = fxColor.r;
            mType->leds[i].g = fxColor.g;
            mType->leds[i].b = fxColor.b;
        }
        else if (i == numLitLEDs)
        {
            mType->leds[i].r = (uint8_t)((float)fxColor.r * modProgress);
            mType->leds[i].g = (uint8_t)((float)fxColor.g * modProgress);
            mType->leds[i].b = (uint8_t)((float)fxColor.b * modProgress);
        }
        else
        {
            mType->leds[i].r = 0x00;
            mType->leds[i].g = 0x00;
            mType->leds[i].b = 0x00;
        }
    }

    applyLEDBrightness(numLEDs, MODE_LED_BRIGHTNESS);
    setLeds(mType->leds, sizeof(mType->leds));
}*/

void ICACHE_FLASH_ATTR clearLEDs(uint8_t numLEDs)
{
    for (int32_t i = 0; i < numLEDs; i++)
    {
        mType->leds[i].r = 0x00;
        mType->leds[i].g = 0x00;
        mType->leds[i].b = 0x00;
    }

    setLeds(mType->leds, sizeof(mType->leds));
}

void ICACHE_FLASH_ATTR applyLEDBrightness(uint8_t numLEDs, float brightness)
{
    // Best way would be to convert to HSV and then set, is this factor method ok?

    for (uint8_t i = 0; i < numLEDs; i++)
    {
        mType->leds[i].r = (uint8_t)((float)mType->leds[i].r * brightness);
        mType->leds[i].g = (uint8_t)((float)mType->leds[i].g * brightness);
        mType->leds[i].b = (uint8_t)((float)mType->leds[i].b * brightness);
    }
}

void ICACHE_FLASH_ATTR mtTitleInput(void)
{
    
}

void ICACHE_FLASH_ATTR mtTitleLogic(void)
{

}

void ICACHE_FLASH_ATTR mtTitleDisplay(void)
{
    drawMenu(mType->titleMenu);
    dancingLEDs(NUM_LEDS, titleColor, mType->stateTime);
}

void ICACHE_FLASH_ATTR mtGameInput(void)
{
    if (mType->player.numLives > 0) {
        // account for good movement if multiple axis of movement are in play.
        vecfloat_t moveDir;
        moveDir.x = 0;
        moveDir.y = 0;

        if (mtIsButtonDown(BTN_GAME_UP)) {
            moveDir.y--;
        }
        if (mtIsButtonDown(BTN_GAME_DOWN)) {
            moveDir.y++;
        }
        if (mtIsButtonDown(BTN_GAME_LEFT)) {
            moveDir.x--;
        }
        if (mtIsButtonDown(BTN_GAME_RIGHT)) {
            moveDir.x++;
        }

        normalize(&moveDir);
        moveDir.x *= mType->player.speed;
        moveDir.y *= mType->player.speed;


        mType->player.lastPosition.x = mType->player.position.x;
        mType->player.lastPosition.y = mType->player.position.y;
        mType->player.position.x += moveDir.x;
        mType->player.position.y += moveDir.y;

        // clamp position of player to within the bounds of the screen and terrain.
        if (mType->player.position.x - mType->player.bbHalf.x < 0) {
            mType->player.position.x = mType->player.bbHalf.x;
        }
        else if (mType->player.position.x + mType->player.bbHalf.x >= OLED_WIDTH) {
            mType->player.position.x = OLED_WIDTH - 1 - mType->player.bbHalf.x;
        }

        if (mType->player.position.y - mType->player.bbHalf.y < 0) {
            mType->player.position.y = mType->player.bbHalf.y;
        }
        // this bouncing effect at the lower y bound is intentional, feels like bouncing off the ground.
        else if (mType->player.position.y >= (mType->floor - RAND_WALLS_HEIGHT)) {
            mType->player.position.y = (mType->floor - RAND_WALLS_HEIGHT) - mType->player.bbHalf.y;
        }

        // activate the reflect shield if the ability is charged and the fire button is pressed.
        if (mtIsButtonReleased(BTN_GAME_ACTION) && mType->player.abilityChargeCounter >= PLAYER_REFLECT_CHARGE_MAX) {
            mType->player.abilityChargeCounter = 0;
            mType->player.abilityCountdown = PLAYER_REFLECT_TIME;
        }   

        // update the shot cd.
        mType->player.shotCooldown += mType->deltaTime;

        if (mType->player.shotCooldown > PLAYER_SHOT_COOLDOWN) {
            mType->player.shotCooldown = PLAYER_SHOT_COOLDOWN;
        }

        // fire a shot if the fire button is being held down and shot is off cd.
        if (mtIsButtonDown(BTN_GAME_ACTION) && mType->player.shotCooldown >= PLAYER_SHOT_COOLDOWN && mType->player.abilityCountdown <= 0) {
            //mType->player.abilityChargeCounter = 0; // uncomment if firing should reset ability cd.
            vec_t firePos;
            firePos.x = mType->player.position.x;
            firePos.y = mType->player.position.y;

            vec_t bbHalf;
            bbHalf.x = 2;
            bbHalf.y = 0;

            vecfloat_t dir;
            dir.x = 1;
            dir.y = 0;

            if (mType->player.shotLevel != 1) {
                fireProjectile(OWNER_PLAYER, TYPE_BOLT, firePos, bbHalf, dir, PLAYER_PROJECTILE_SPEED, PLAYER_PROJECTILE_DAMAGE);
            }
            if (mType->player.shotLevel > 0) {
                firePos.y = mType->player.position.y - mType->player.bbHalf.y;
                fireProjectile(OWNER_PLAYER, TYPE_BOLT, firePos, bbHalf, dir, PLAYER_PROJECTILE_SPEED, PLAYER_PROJECTILE_DAMAGE);
                firePos.y = mType->player.position.y + mType->player.bbHalf.y;
                fireProjectile(OWNER_PLAYER, TYPE_BOLT, firePos, bbHalf, dir, PLAYER_PROJECTILE_SPEED, PLAYER_PROJECTILE_DAMAGE);
            }
            mType->player.shotCooldown = 0;
        }

        if (mtIsButtonUp(BTN_GAME_ACTION) && mType->player.abilityCountdown <= 0) {
            mType->player.abilityChargeCounter += mType->deltaTime;
            if (mType->player.abilityChargeCounter > PLAYER_REFLECT_CHARGE_MAX) {
                mType->player.abilityChargeCounter = PLAYER_REFLECT_CHARGE_MAX;
            }
        }
    }
}

void ICACHE_FLASH_ATTR mtGameLogic(void)
{
    // enemy movement and projectile spawning
    mType->enemiesInWave = 0;
    mType->enemiesOnScreen = 0;

    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (mType->enemies[i].active) {
            mType->enemiesInWave++;
            if (mType->enemies[i].position.x >= 0 && mType->enemies[i].position.x <= OLED_WIDTH) {
                mType->enemiesOnScreen++;
            }

            if (mType->enemies[i].type == ENEMY_SNAKE) {
                // bob up and down as they advance across the screen.
                mType->enemies[i].position.x += mType->stateFrames % 2 == 0 ? -1 : 0;
                if (mType->enemies[i].position.x < -mType->enemies[i].bbHalf.x) {
                    mType->enemies[i].frameOffset = mType->stateFrames;
                    mType->enemies[i].position.x = OLED_WIDTH + mType->enemies[i].bbHalf.x;
                }
                mType->enemies[i].position.y = mType->enemies[i].spawn.y + (7 * sin(((mType->stateFrames + mType->enemies[i].frameOffset) / 25.0)));

                // update enemy shot cooldown.
                mType->enemies[i].shotCooldown += mType->deltaTime;
                if (mType->enemies[i].shotCooldown >= ENEMY_SNAKE_SHOT_COOLDOWN) {
                    mType->enemies[i].shotCooldown = 0;

                    vec_t bbHalf;
                    bbHalf.x = 0;
                    bbHalf.y = 0;

                    vecfloat_t dir;
                    dir.x = -1;
                    dir.y = 0;

                    fireProjectile(OWNER_ENEMY, TYPE_BOLT, mType->enemies[i].position, bbHalf, dir, ENEMY_PROJECTILE_SPEED, ENEMY_PROJECTILE_DAMAGE);
                }
            }
            else if (mType->enemies[i].type == ENEMY_BOMBER) {
                // advance slowly across the screen, stopping to drop bombs when above the player.
                int16_t prevX = mType->enemies[i].position.x;

                mType->enemies[i].position.x += -1;//mType->stateFrames % 2 == 0 ? -1 : 0;
                if (mType->enemies[i].position.x < -mType->enemies[i].bbHalf.x) {
                    mType->enemies[i].frameOffset = mType->stateFrames;
                    mType->enemies[i].position.x = OLED_WIDTH + mType->enemies[i].bbHalf.x;
                }

                // update enemy shot cooldown.
                mType->enemies[i].shotCooldown += mType->deltaTime;

                bool inRange = (prevX > mType->player.lastPosition.x && mType->enemies[i].position.x <= mType->player.position.x) ||
                                (prevX < mType->player.lastPosition.x && mType->enemies[i].position.x >= mType->player.position.x);

                if (mType->enemies[i].shotCooldown >= ENEMY_BOMBER_SHOT_COOLDOWN && inRange) {
                    mType->enemies[i].shotCooldown = 0;

                    vec_t bbHalf;
                    bbHalf.x = 0;
                    bbHalf.y = 0;

                    vecfloat_t dir;
                    dir.x = 0;
                    dir.y = mType->player.position.y >= mType->enemies[i].position.y ? 1 : -1;

                    fireProjectile(OWNER_ENEMY, TYPE_BOLT, mType->enemies[i].position, bbHalf, dir, ENEMY_PROJECTILE_SPEED, ENEMY_PROJECTILE_DAMAGE);
                }
            }
            else if (mType->enemies[i].type == ENEMY_WALKER) {
                // advance slowly across the screen, stopping to drop bombs when above the player.
                // int16_t prevX = mType->enemies[i].position.x;

                mType->enemies[i].position.x += mType->stateFrames % 4 == 0 ? -1 : 0;
                if (mType->enemies[i].position.x < -mType->enemies[i].bbHalf.x) {
                    mType->enemies[i].frameOffset = mType->stateFrames;
                    mType->enemies[i].position.x = OLED_WIDTH + mType->enemies[i].bbHalf.x;
                }

                // update enemy shot cooldown.
                mType->enemies[i].shotCooldown += mType->deltaTime;

                if (mType->enemies[i].shotCooldown >= ENEMY_WALKER_SHOT_COOLDOWN) {
                    mType->enemies[i].shotCooldown = 0;

                    vec_t bbHalf;
                    bbHalf.x = 0;
                    bbHalf.y = 0;

                    vecfloat_t dir;
                    dir.x = mType->player.position.x - mType->enemies[i].position.x;
                    dir.y = mType->player.position.y - mType->enemies[i].position.y;
                    normalize(&dir);

                    fireProjectile(OWNER_ENEMY, TYPE_BOLT, mType->enemies[i].position, bbHalf, dir, ENEMY_PROJECTILE_SPEED, ENEMY_PROJECTILE_DAMAGE);
                }
            }

            int plx0, plx1, ply0, ply1;
            plx0 = mType->player.position.x - mType->player.bbHalf.x;
            plx1 = mType->player.position.x + mType->player.bbHalf.x;

            ply0 = mType->player.position.y - mType->player.bbHalf.y;
            ply1 = mType->player.position.y + mType->player.bbHalf.y;

            if (mType->player.abilityCountdown > 0) {
                plx0 -= PLAYER_REFLECT_COLLISION;
                plx1 += PLAYER_REFLECT_COLLISION;
                ply0 -= PLAYER_REFLECT_COLLISION;
                ply1 += PLAYER_REFLECT_COLLISION;
            }

            // check collision with enemies to damage player.
            if (AABBCollision(plx0, ply0, plx1, ply1,
                mType->enemies[i].position.x - mType->enemies[i].bbHalf.x, 
                mType->enemies[i].position.y - mType->enemies[i].bbHalf.y, 
                mType->enemies[i].position.x + mType->enemies[i].bbHalf.x, 
                mType->enemies[i].position.y + mType->enemies[i].bbHalf.y)) {
                    if (mType->player.abilityCountdown <= 0 && mType->player.invincibilityCountdown <= 0) {
                        playerDeath();
                    }
                    else {
                        // kill the enemy if reflect shield is up.
                        mType->score += ENEMY_KILL * REFLECT_RAM_BONUS;
                        enemyDeath(i);
                    }
            }
        }
    }

    if (mType->enemiesOnScreen == 0) {
        mType->waveEmptyTime += mType->deltaTime;
    }

    if (mType->enemiesInWave == 0 || mType->waveEmptyTime >= ENEMY_WAVE_EMPTY_TIME) {

        vec_t bbHalf;
        vec_t initialSpawn;
        // uint8_t speed;
        // vecfloat_t direction;
        int numFormations;
        int type;
        int i;
        int8_t health;
        int32_t unitFrameOffset;
        uint8_t numEnemies;
        int16_t xSpacing;
        int16_t ySpacing;

        // skip tutorial waves on higher difficulties.
        if (mType->wave < 3 && (mType->difficulty == DIFFICULTY_HARD || mType->difficulty ==  DIFFICULTY_VERYHARD)) {
            mType->wave += 3;
        }

        switch (mType->wave) {
            case 0:
                /* first wave */
                type = ENEMY_SNAKE;
                health = 1;
                bbHalf.x = 3;
                bbHalf.y = 3;
                initialSpawn.x = OLED_WIDTH + bbHalf.x * 2;
                initialSpawn.y = OLED_HEIGHT - 35;
                // speed = 1;
                // direction.x = -1;
                // direction.y = 0;
                unitFrameOffset = 10;
                numEnemies = 3;
                xSpacing = 10;
                ySpacing = 0;
                
                spawnEnemyFormation(type, initialSpawn, health, bbHalf, unitFrameOffset, numEnemies, xSpacing, ySpacing);

                initialSpawn.x = OLED_WIDTH + 75;
                initialSpawn.y = OLED_HEIGHT - 55;
                spawnEnemyFormation(type, initialSpawn, health, bbHalf, unitFrameOffset, numEnemies, xSpacing, ySpacing);

                initialSpawn.x = OLED_WIDTH + 145;
                initialSpawn.y = OLED_HEIGHT - 25;
                spawnEnemyFormation(type, initialSpawn, health, bbHalf, unitFrameOffset, numEnemies, xSpacing, ySpacing);

                break;
            case 1:
                /* second wave */
                type = ENEMY_SNAKE;
                health = 1;
                bbHalf.x = 3;
                bbHalf.y = 3;
                // speed = 1;
                // direction.x = -1;
                // direction.y = 0;
                unitFrameOffset = 10;
                numEnemies = 3;
                xSpacing = -10;
                ySpacing = 0;

                initialSpawn.x = OLED_WIDTH + 20;
                initialSpawn.y = OLED_HEIGHT - 35;
                spawnEnemyFormation(type, initialSpawn, health, bbHalf, unitFrameOffset, numEnemies, xSpacing, ySpacing);

                initialSpawn.x = OLED_WIDTH + 85;
                spawnEnemyFormation(type, initialSpawn, health, bbHalf, unitFrameOffset, numEnemies, xSpacing, ySpacing);

                type = ENEMY_WALKER;
                initialSpawn.x = OLED_WIDTH + 35;
                initialSpawn.y = OLED_HEIGHT - 15;
                xSpacing = 80;
                spawnEnemyFormation(type, initialSpawn, health, bbHalf, unitFrameOffset, numEnemies, xSpacing, ySpacing);
                break;
            case 2:
                /* third wave */
                type = ENEMY_SNAKE;
                health = 1;
                bbHalf.x = 3;
                bbHalf.y = 3;
                // speed = 1;
                // direction.x = -1;
                // direction.y = 0;
                unitFrameOffset = 10;
                numEnemies = 3;
                xSpacing = -10;
                ySpacing = 0;

                /*initialSpawn.x = OLED_WIDTH + 25;
                initialSpawn.y = OLED_HEIGHT - 35;
                spawnEnemyFormation(type, initialSpawn, health, bbHalf, unitFrameOffset, numEnemies, xSpacing, ySpacing);

                initialSpawn.x = OLED_WIDTH + 125;
                spawnEnemyFormation(type, initialSpawn, health, bbHalf, unitFrameOffset, numEnemies, xSpacing, ySpacing);*/

                type = ENEMY_BOMBER;
                initialSpawn.x = OLED_WIDTH + 35;
                initialSpawn.y = 10;
                xSpacing = 25;
                spawnEnemyFormation(type, initialSpawn, health, bbHalf, unitFrameOffset, numEnemies, xSpacing, ySpacing);

                type = ENEMY_WALKER;
                initialSpawn.x = OLED_WIDTH + 35;
                initialSpawn.y = OLED_HEIGHT - 15;
                xSpacing = 40;
                spawnEnemyFormation(type, initialSpawn, health, bbHalf, unitFrameOffset, numEnemies, xSpacing, ySpacing);
                break;
            default:
                /* randomly generated wave */
                // playable area is like 5 to OLED_HEIGHT - 15
                initialSpawn.x = OLED_WIDTH + bbHalf.x;
                numFormations = (3 + mType->wave / 2 + mType->wave % 2) * (mType->difficulty != DIFFICULTY_VERYHARD ? mType->difficulty : 10);
                for (i = 0; i < numFormations; i++) {
                    type = os_random() % 3;
                    bbHalf.x = 3;
                    bbHalf.y = 3;
                    // speed = 1;
                    // direction.x = -1;
                    // direction.y = 0;
                    health = 1;//(mType->wave / 10);
                    unitFrameOffset = 20;
                    numEnemies = mType->difficulty + (os_random() % mType->wave);
                    xSpacing = bbHalf.x * 2 + (os_random() % 20);
                    ySpacing = 0;

                    initialSpawn.x += (os_random() % 50);
                    initialSpawn.y = (os_random() % (OLED_HEIGHT - 25)) + 5;
                    if (type == ENEMY_WALKER) {
                        initialSpawn.y = OLED_HEIGHT - 15;
                        xSpacing *= 2;
                    }
                    spawnEnemyFormation(type, initialSpawn, health, bbHalf, unitFrameOffset, numEnemies, xSpacing, ySpacing);
                }
                break;
        }

        mType->score += mType->wave * WAVE_CLEAR_BONUS;
        mType->wave++;
        mType->waveEmptyTime = 0;
    }
    
    // projectile movement and collision
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (mType->projectiles[i].active) {

            // TODO: better accounting for projectiles on non-orthagonal trajectories that are larger than 1 pixel.

            // check projectile collisions as a bounding box that is defined by the projectiles current position and its projected position.
            int px0, px1, py0, py1;

            px0 = mType->projectiles[i].position.x - mType->projectiles[i].bbHalf.x;
            px1 = mType->projectiles[i].position.x + mType->projectiles[i].bbHalf.x;

            if (mType->projectiles[i].direction.x < 0) {
                px0 -= mType->projectiles[i].direction.x * mType->projectiles[i].speed;
            }
            else if (mType->projectiles[i].direction.x > 0) {
                px1 += mType->projectiles[i].direction.x * mType->projectiles[i].speed;
            }

            py0 = mType->projectiles[i].position.y - mType->projectiles[i].bbHalf.y;
            py1 = mType->projectiles[i].position.y + mType->projectiles[i].bbHalf.y;

            if (mType->projectiles[i].direction.y < 0) {
                py0 -= mType->projectiles[i].direction.y * mType->projectiles[i].speed;
            }
            else if (mType->projectiles[i].direction.y > 0) {
                py1 += mType->projectiles[i].direction.y * mType->projectiles[i].speed;
            }

            if (mType->projectiles[i].owner == OWNER_PLAYER) {
                for (int j = 0; j < MAX_ENEMIES; j++) {
                    if (mType->enemies[j].active && mType->enemies[j].position.x - mType->enemies[j].bbHalf.x < OLED_WIDTH) {
                        if (AABBCollision(px0, py0, px1, py1, 
                            mType->enemies[j].position.x - mType->enemies[j].bbHalf.x, 
                            mType->enemies[j].position.y - mType->enemies[j].bbHalf.y, 
                            mType->enemies[j].position.x + mType->enemies[j].bbHalf.x, 
                            mType->enemies[j].position.y + mType->enemies[j].bbHalf.y)) {
                            mType->projectiles[i].active = 0;
                            mType->enemies[j].health -= mType->projectiles[i].damage;
                            if (mType->enemies[j].health <= 0) {
                                //TODO: increase by amount of enemy health.
                                mType->score += mType->projectiles[i].originalOwner == OWNER_ENEMY ? ENEMY_KILL * REFLECT_KILL_BONUS : ENEMY_KILL;
                                enemyDeath(j);
                            }
                        }
                    }
                }
            }

            if (mType->projectiles[i].owner == OWNER_ENEMY) {
                // if player reflect shield is up then the range to check for collisions is larger.
                int plx0, plx1, ply0, ply1;
                plx0 = mType->player.position.x - mType->player.bbHalf.x;
                plx1 = mType->player.position.x + mType->player.bbHalf.x;

                ply0 = mType->player.position.y - mType->player.bbHalf.y;
                ply1 = mType->player.position.y + mType->player.bbHalf.y;

                if (mType->player.abilityCountdown > 0) {
                    plx0 -= PLAYER_REFLECT_COLLISION;
                    plx1 += PLAYER_REFLECT_COLLISION;
                    ply0 -= PLAYER_REFLECT_COLLISION;
                    ply1 += PLAYER_REFLECT_COLLISION;
                }

                if (AABBCollision(px0, py0, px1, py1, 
                    plx0, ply0, plx1, ply1)) {
                    
                    // reflect projectiles if reflect is up.
                    if (mType->player.abilityCountdown > 0) {
                        mType->projectiles[i].owner = OWNER_PLAYER;
                        mType->projectiles[i].direction.x *= -1;
                        mType->projectiles[i].direction.y *= -1;
                        mType->projectiles[i].speed *= 2;
                    }
                    else {
                        if (mType->player.invincibilityCountdown <= 0) {
                            playerDeath();
                        }
                        else {
                            mType->projectiles[i].active = 0;
                        }
                    }
                }
            }

            // deactivate projectile if it is entirely out of bounds.
            if (mType->projectiles[i].position.x - mType->projectiles[i].bbHalf.x >= OLED_WIDTH ||
                mType->projectiles[i].position.x + mType->projectiles[i].bbHalf.x < 0 ||
                mType->projectiles[i].position.y - mType->projectiles[i].bbHalf.y >= OLED_HEIGHT ||
                mType->projectiles[i].position.y + mType->projectiles[i].bbHalf.y < 0) {
                mType->projectiles[i].active = 0;
            }

            // if we didn't hit anything or go out of bounds then move.
            if (mType->projectiles[i].active) {
                mType->projectiles[i].position.x += (mType->projectiles[i].direction.x * mType->projectiles[i].speed);
                mType->projectiles[i].position.y += (mType->projectiles[i].direction.y * mType->projectiles[i].speed);
            }
        }   
    }

    // powerup movement and collision
    for (int i = 0; i < MAX_POWERUPS; i++) {
        if (mType->powerups[i].active) {
            // powerups drift left after a short delay.
            mType->powerups[i].driftDelay += mType->deltaTime;
            if (mType->powerups[i].driftDelay >= PWRUP_DRIFT_DELAY) {
                mType->powerups[i].position.x -= mType->powerups[i].speed;
            }

            // players collide with a powerup to recieve its effects.
            if (AABBCollision(mType->player.position.x - mType->player.bbHalf.x, 
                    mType->player.position.y - mType->player.bbHalf.y, 
                    mType->player.position.x + mType->player.bbHalf.x, 
                    mType->player.position.y + mType->player.bbHalf.y, 
                    mType->powerups[i].position.x - mType->powerups[i].bbHalf.x, 
                    mType->powerups[i].position.y - mType->powerups[i].bbHalf.y, 
                    mType->powerups[i].position.x + mType->powerups[i].bbHalf.x, 
                    mType->powerups[i].position.y + mType->powerups[i].bbHalf.y)) {
                        mType->powerups[i].active = 0;
                        mType->score += POWERUP_GET_BONUS;
                        if (mType->powerups[i].type == PWRUP_FP) {
                            mType->player.shotLevel++;
                            // TODO: this really should be something cooler, like a screen clearing beam or bomb
                            if (mType->player.shotLevel > 2) {
                                // if reflect is in use, refresh reflect duration, makes chains possible.
                                if (mType->player.abilityCountdown > 0) {
                                    mType->player.abilityCountdown = PLAYER_REFLECT_TIME;
                                }
                                // max out player's reflect guage if not in use and not fully charged.
                                else {
                                    mType->player.abilityChargeCounter = PLAYER_REFLECT_CHARGE_MAX;
                                }
                            }
                        }
            }

            // despawn powerups if they drift off of the screen.
            if (mType->powerups[i].position.x + mType->powerups[i].bbHalf.x < 0) {
                mType->powerups[i].active = 0;
            }
        }
    }

    if (mType->player.abilityCountdown > 0) {
        mType->player.abilityCountdown -= mType->deltaTime;
        if (mType->player.abilityCountdown < 0) {
            mType->player.abilityCountdown = 0;
        }
    }

    if (mType->player.invincibilityCountdown > 0) {
        mType->player.invincibilityCountdown -= mType->deltaTime;
        if (mType->player.invincibilityCountdown < 0) {
            mType->player.invincibilityCountdown = 0;
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

    // gameover transition
    if (mType->player.numLives == 0) {
        mType->gameoverCountdown -= mType->deltaTime;
        if (mType->gameoverCountdown <= 0) {
            submitMTScore(mType->difficulty, mType->stateTime, mType->score);
            mtSetState(MT_GAMEOVER);
        }
    }
}

void ICACHE_FLASH_ATTR mtGameDisplay(void)
{
    // clear the frame.
    fillDisplayArea(0, 0, OLED_WIDTH - 1, OLED_HEIGHT - 1, BLACK);

    // score text.
    char uiStr[32] = {0};
    ets_snprintf(uiStr, sizeof(uiStr), "%06u", mType->score);
    int scoreTextX = 52;
    int scoreTextY = 1;
    fillDisplayArea(scoreTextX, 0, scoreTextX + 23, FONT_HEIGHT_TOMTHUMB + 1, BLACK);
    plotText(scoreTextX, scoreTextY, uiStr, TOM_THUMB, WHITE);

    // draw powerups.
    for (int i = 0; i < MAX_POWERUPS; i++) {
        if (mType->powerups[i].active) {
            drawPngInv(&mType->powerupHandle, (int16_t)mType->powerups[i].position.x - mType->powerups[i].bbHalf.x, (int16_t)mType->powerups[i].position.y - mType->powerups[i].bbHalf.y, 
                        false, false, 0, mType->stateFrames % 2);
        }
    }

    // draw projectiles.
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (mType->projectiles[i].active) {
            plotLine(mType->projectiles[i].position.x - mType->projectiles[i].bbHalf.x, 
                    mType->projectiles[i].position.y - mType->projectiles[i].bbHalf.y, 
                    mType->projectiles[i].position.x + mType->projectiles[i].bbHalf.x, 
                    mType->projectiles[i].position.y + mType->projectiles[i].bbHalf.y, WHITE);
        }   
    }

    // draw enemies.
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (mType->enemies[i].active) {
            if (mType->enemies[i].type == ENEMY_SNAKE) {
                drawPngSequence(&mType->snakeSequenceHandle, 
                                (int16_t)mType->enemies[i].position.x - mType->enemies[i].bbHalf.x, (int16_t)mType->enemies[i].position.y - mType->enemies[i].bbHalf.y,
                                false, false, 0, mType->stateFrames % 2);
            }
            else if  (mType->enemies[i].type == ENEMY_BOMBER) {
                drawPngSequence(&mType->bomberSequenceHandle, 
                                (int16_t)mType->enemies[i].position.x - mType->enemies[i].bbHalf.x, (int16_t)mType->enemies[i].position.y - mType->enemies[i].bbHalf.y,
                                false, false, 0, mType->stateFrames % 2);
            }
            else if  (mType->enemies[i].type == ENEMY_WALKER) {
                drawPngSequence(&mType->walkerSequenceHandle, 
                                (int16_t)mType->enemies[i].position.x - mType->enemies[i].bbHalf.x, (int16_t)mType->enemies[i].position.y - mType->enemies[i].bbHalf.y,
                                false, false, 0, mType->stateFrames % 2);
            }
        }
    }

    // draw player
    pngHandle * playerSprite = &mType->playerStraightHandle;
    if (mType->player.numLives > 0) {
        if (mType->player.abilityCountdown > 0) {
            plotCircle(mType->player.position.x, mType->player.position.y, ((mType->stateFrames / 2) % 3) + 4, WHITE);
        }
        if (mType->player.position.y < mType->player.lastPosition.y) {
            playerSprite = &mType->playerUpHandle;
        }
        else if (mType->player.position.y > mType->player.lastPosition.y) {
            playerSprite = &mType->playerDownHandle;
        }
        bool inv = mType->player.invincibilityCountdown > 0 && mType->stateFrames % 2 == 0;
        drawPngInv(playerSprite, (int16_t)mType->player.position.x - mType->player.bbHalf.x, (int16_t)mType->player.position.y - mType->player.bbHalf.y, 
                    true, false, 0, inv);
    }

    // draw ui
    /*
    reflect text
    reflect bar rect container
    reflect line bar fill
    wave #
    */

    int reflectTextX = 30;
    int reflectTextY = OLED_HEIGHT - (1 * (FONT_HEIGHT_TOMTHUMB + 1));

    int boundaryLineY = reflectTextY - 2;

    // fill ui area
    fillDisplayArea(0, boundaryLineY, OLED_WIDTH - 1, OLED_HEIGHT - 1, BLACK);

    // upper ui border
    //plotLine(0, boundaryLineY, OLED_WIDTH - 1, boundaryLineY, WHITE);

    int livesTextX = 12;
    int livesTextY = OLED_HEIGHT - (1 * (FONT_HEIGHT_TOMTHUMB + 1));

    int livesSymbolX = 7;
    int livesSymbolY = livesTextY + 2;

    // lives display
    drawPngInv(playerSprite, (int16_t)livesSymbolX - mType->player.bbHalf.x, (int16_t)livesSymbolY - mType->player.bbHalf.y, 
                false, false, 270, false);

    // lives text
    ets_snprintf(uiStr, sizeof(uiStr), "x%d", mType->player.numLives);
    plotText(livesTextX, livesTextY, uiStr, TOM_THUMB, WHITE);

    // reflect text
    ets_snprintf(uiStr, sizeof(uiStr), "REFLECT");
    plotText(reflectTextX, reflectTextY, uiStr, TOM_THUMB, WHITE);

    // reflect bar container
    int reflectBarX0 = reflectTextX + getTextWidth(uiStr, TOM_THUMB) + 1;
    int reflectBarY0 = reflectTextY + 1;
    int reflectBarX1 = reflectBarX0 + 35;
    int reflectBarY1 = reflectBarY0 + 2;
    plotRect(reflectBarX0, reflectBarY0, reflectBarX1, reflectBarY1, WHITE);

    // reflect bar fill
    // this will deplete from full while the reflect is winding down.
    float charge = mType->player.abilityCountdown > 0 ? (float)mType->player.abilityCountdown / PLAYER_REFLECT_TIME : (float)mType->player.abilityChargeCounter / PLAYER_REFLECT_CHARGE_MAX;

    int reflectBarFillX1 = reflectBarX0 + (charge * ((reflectBarX1 - 1) - reflectBarX0));
    plotLine(reflectBarX0, reflectBarY0 + 1, reflectBarFillX1, reflectBarY0 + 1, WHITE);

    // wave text
    int waveTextX = reflectBarX1 + 10;
    int waveTextY = reflectTextY;
    uint32_t waveDisplay = mType->wave;
    // adjust so that on higher difficulties first displays as wave 1 despite skipped tutorial waves.
    if (mType->difficulty == DIFFICULTY_HARD || mType->difficulty ==  DIFFICULTY_VERYHARD) {
        waveDisplay -= 3;
    }
    // cap at 99.
    if (waveDisplay > 99) {
        waveDisplay = 99;
    }
    ets_snprintf(uiStr, sizeof(uiStr), "WAVE:%02u", waveDisplay);
    plotText(waveTextX, waveTextY, uiStr, TOM_THUMB, WHITE);

    // For each chunk coordinate
    for(uint8_t w = 0; w < NUM_CHUNKS; w++)
    {
        // Plot a floor segment line between chunk coordinates
        plotLine(
            (w * CHUNK_WIDTH) - mType->xOffset,
            mType->floors[w],
            ((w + 1) * CHUNK_WIDTH) - mType->xOffset,
            mType->floors[w + 1],
            WHITE);
    }

    // Since explosions are FX, they get updated after the screen has drawn.
    for (int i = 0; i < MAX_EXPLOSIONS; i++) {
        if (mType->explosions[i].active) {
            drawPngSequence(&mType->explosionSequenceHandle, 
                            (int16_t)mType->explosions[i].position.x - mType->explosions[i].bbHalf.x, (int16_t)mType->explosions[i].position.y - mType->explosions[i].bbHalf.y,
                            false, false, 0, mType->explosions[i].frame / 2);

            mType->explosions[i].frame++;
            if (mType->explosions[i].frame >= EXPLOSION_FRAMES) {
                mType->explosions[i].active = 0;
            }
        }
    }

    // Update LED FX.
    if (mType->player.abilityCountdown > 0) {
        dancingLEDs(NUM_LEDS, reflectColor, mType->stateTime);
    }
    else {
        float shotProgress = ((float)mType->player.shotCooldown / (float)PLAYER_SHOT_COOLDOWN);
        singlePulseLEDs(NUM_LEDS, shotColor, shotProgress);
    }

    //char uiStr[32] = {0};
    //ets_snprintf(uiStr, sizeof(uiStr), "BTN: %d u:%d, d:%d, l:%d, r:%d, a:%d", button, upDown, downDown, leftDown, rightDown, actionDown);
    /*ets_snprintf(uiStr, sizeof(uiStr), "sp:%f", shotProgress);
    fillDisplayArea(OLED_WIDTH - getTextWidth(uiStr, TOM_THUMB) - 1, OLED_HEIGHT - (1 * (FONT_HEIGHT_TOMTHUMB + 1)), OLED_WIDTH, OLED_HEIGHT, BLACK);
    plotText(OLED_WIDTH - getTextWidth(uiStr, TOM_THUMB) - 1, OLED_HEIGHT - (1 * (FONT_HEIGHT_TOMTHUMB + 1)), uiStr, TOM_THUMB, WHITE);*/
}

void ICACHE_FLASH_ATTR mtScoresInput(void)
{
    if (mtIsButtonPressed(BTN_GAME_LEFT)) {
        //1-3
        mType->difficulty--;
        if (mType->difficulty < DIFFICULTY_EASY) {
            mType->difficulty = DIFFICULTY_VERYHARD;
        }
    }
    if (mtIsButtonPressed(BTN_GAME_RIGHT)) {
        mType->difficulty++;
        if (mType->difficulty > DIFFICULTY_VERYHARD) {
            mType->difficulty = DIFFICULTY_EASY;
        }
    }
    if (mtIsButtonPressed(BTN_GAME_ACTION)) {
        mtSetState(MT_TITLE);
    }
}

void ICACHE_FLASH_ATTR mtScoresLogic(void)
{

}

void ICACHE_FLASH_ATTR mtScoresDisplay(void)
{
    clearDisplay();

    mtScore_t* currentDiffScores;
    char scoresTitle[16];

    switch (mType->difficulty) {
        default:
        case DIFFICULTY_VERYHARD:
            currentDiffScores = mType->highScores.veryhardScores;
            ets_snprintf(scoresTitle, sizeof(scoresTitle), mt_veryhard);
            break;
        case DIFFICULTY_HARD:
            currentDiffScores = mType->highScores.hardScores;
            ets_snprintf(scoresTitle, sizeof(scoresTitle), mt_hard);
            break;
        case DIFFICULTY_MEDIUM:
            currentDiffScores = mType->highScores.mediumScores;
            ets_snprintf(scoresTitle, sizeof(scoresTitle), mt_medium);
            break;
        case DIFFICULTY_EASY:
            currentDiffScores = mType->highScores.easyScores;
            ets_snprintf(scoresTitle, sizeof(scoresTitle), mt_easy);
            break;
    }

    int titleXPos = (OLED_WIDTH - textWidth(scoresTitle, IBM_VGA_8)) / 2;
    int titleYPos = 5;
    plotText(titleXPos, titleYPos, scoresTitle, IBM_VGA_8, WHITE);

    for (int i = 0; i < MT_NUM_HIGHSCORES; i++) {
        mtScore_t currScore = currentDiffScores[i];
        if (currScore.score > 0 || currScore.timeSurvived > 0) {
            int yPos = 26 + (i % 4) * 10;
            int xPos = 5 + (i / 4) * 60;
            char scoreText[24];
            uint32_t secondsSurvived = currScore.timeSurvived * US_TO_MS_FACTOR * MS_TO_S_FACTOR;
            ets_snprintf(scoreText, sizeof(scoreText), "%d. %06u (%u:%02u)", (i + 1), currScore.score, secondsSurvived / 60, secondsSurvived % 60);
            plotText(xPos, yPos, scoreText, TOM_THUMB, WHITE);
        }
    }

    plotText(0, 56, "<", TOM_THUMB, WHITE);
    plotText(125, 56, ">", TOM_THUMB, WHITE);

    alternatingPulseLEDS(NUM_LEDS, scoresColor, mType->modeTime);
}

void ICACHE_FLASH_ATTR mtGameoverInput(void)
{

}

void ICACHE_FLASH_ATTR mtGameoverLogic(void)
{

}

void ICACHE_FLASH_ATTR mtGameoverDisplay(void)
{
    drawMenu(mType->gameoverMenu);
    blinkLEDs(NUM_LEDS, gameoverColor, mType->stateTime);
}

/**
 * 
 *
 * @param menuItem
 */
static void ICACHE_FLASH_ATTR mtTitleMenuCallback(const char* menuItem)
{
    if (mt_easy == menuItem)
    {
        // Change state to start game.
        mType->difficulty = DIFFICULTY_EASY;
        mtSetState(MT_GAME);
    }
    else if (mt_medium == menuItem)
    {
        // Change state to start game.
        mType->difficulty = DIFFICULTY_MEDIUM;
        mtSetState(MT_GAME);
    }
    else if (mt_hard == menuItem)
    {
        // Change state to start game.
        mType->difficulty = DIFFICULTY_HARD;
        mtSetState(MT_GAME);
    }
    else if (mt_veryhard == menuItem)
    {
        // Change state to start game.
        mType->difficulty = DIFFICULTY_VERYHARD;
        mtSetState(MT_GAME);
    }
    else if (mt_scores == menuItem)
    {
        // Change state to score screen.
        // Start on medium scores.
        mType->difficulty = DIFFICULTY_MEDIUM;
        mtSetState(MT_SCORES);
    }
    else if (mt_quit == menuItem)
    {
        // Exit this swadge mode.
        switchToSwadgeMode(0);
    }
}

static void ICACHE_FLASH_ATTR mtGameoverMenuCallback(const char* menuItem)
{
    if (mt_restart == menuItem)
    {
        // Change state to start game.
        mtSetState(MT_GAME);
    }
    if (mt_menu == menuItem)
    {
        // Change state to main menu.
        mtSetState(MT_TITLE);
    }
    else if (mt_quit == menuItem)
    {
        // Exit this swadge mode.
        switchToSwadgeMode(0);
    }
}

bool ICACHE_FLASH_ATTR submitMTScore(uint8_t difficulty, uint32_t timeSurvived, uint32_t score) {
    bool newHighScore = false;
    if (difficulty == DIFFICULTY_EASY) {
        for (int i = 0; i < MT_NUM_HIGHSCORES; i++) {
            if (score >= mType->highScores.easyScores[i].score) {
                newHighScore = true;
                uint32_t tempScore = mType->highScores.easyScores[i].score;
                uint32_t tempTimeSurvived = mType->highScores.easyScores[i].timeSurvived;
                mType->highScores.easyScores[i].score = score;
                mType->highScores.easyScores[i].timeSurvived = timeSurvived;
                score = tempScore;
                timeSurvived = tempTimeSurvived;
            }
        }
    }
    else if (difficulty == DIFFICULTY_MEDIUM) {
        for (int i = 0; i < MT_NUM_HIGHSCORES; i++) {
            if (score >= mType->highScores.mediumScores[i].score) {
                newHighScore = true;
                uint32_t tempScore = mType->highScores.mediumScores[i].score;
                uint32_t tempTimeSurvived = mType->highScores.mediumScores[i].timeSurvived;
                mType->highScores.mediumScores[i].score = score;
                mType->highScores.mediumScores[i].timeSurvived = timeSurvived;
                score = tempScore;
                timeSurvived = tempTimeSurvived;
            }
        }
    }
    else if (difficulty == DIFFICULTY_HARD) {
        for (int i = 0; i < MT_NUM_HIGHSCORES; i++) {
            if (score >= mType->highScores.hardScores[i].score) {
                newHighScore = true;
                uint32_t tempScore = mType->highScores.hardScores[i].score;
                uint32_t tempTimeSurvived = mType->highScores.hardScores[i].timeSurvived;
                mType->highScores.hardScores[i].score = score;
                mType->highScores.hardScores[i].timeSurvived = timeSurvived;
                score = tempScore;
                timeSurvived = tempTimeSurvived;
            }
        }
    }
    else if (difficulty == DIFFICULTY_VERYHARD) {
        for (int i = 0; i < MT_NUM_HIGHSCORES; i++) {
            if (score >= mType->highScores.veryhardScores[i].score) {
                newHighScore = true;
                uint32_t tempScore = mType->highScores.veryhardScores[i].score;
                uint32_t tempTimeSurvived = mType->highScores.veryhardScores[i].timeSurvived;
                mType->highScores.veryhardScores[i].score = score;
                mType->highScores.veryhardScores[i].timeSurvived = timeSurvived;
                score = tempScore;
                timeSurvived = tempTimeSurvived;
            }
        }
    }
    else {
        // Unknown difficulty, do nothing.
    }
    
    if (newHighScore) {
        setMTScores(&mType->highScores);
    }
    return newHighScore;
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

bool ICACHE_FLASH_ATTR AABBCollision (int ax0, int ay0, int ax1, int ay1, int bx0, int by0, int bx1, int by1) {
    // int awidth = ax1 - ax0;
    // int aheight = ay1 - ay0;

    // int bwidth = bx1 - bx0;
    // int bheight = by1 - by0;

    return (ax0 < bx1 &&
            ax1 > bx0 &&
            ay0 < by1 &&
            ay1 > by0);
}

void ICACHE_FLASH_ATTR normalize (vecfloat_t * vec)
{
    if (vec->x != 0 || vec->y != 0) {
        float mag = sqrt(pow(vec->x, 2) + pow(vec->y, 2));
        vec->x /= mag;
        vec->y /= mag;
    }
}

bool ICACHE_FLASH_ATTR fireProjectile (uint8_t owner, uint8_t type, vec_t position, vec_t bbHalf, vecfloat_t direction, uint8_t speed, uint8_t damage)
{
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!mType->projectiles[i].active) {
            mType->projectiles[i].active = 1;
            mType->projectiles[i].type = type;
            mType->projectiles[i].originalOwner = owner;
            mType->projectiles[i].owner = owner;
            mType->projectiles[i].position.x = position.x;
            mType->projectiles[i].position.y = position.y;
            mType->projectiles[i].bbHalf.x = bbHalf.x;
            mType->projectiles[i].bbHalf.y = bbHalf.y;
            mType->projectiles[i].direction.x = direction.x;
            mType->projectiles[i].direction.y = direction.y;
            mType->projectiles[i].speed = speed;
            mType->projectiles[i].damage = damage;
            return true;
        }
    }
    return false;
}

bool ICACHE_FLASH_ATTR spawnExplosion (vec_t spawn, vec_t bbHalf) 
{
    for (int i = 0; i < MAX_EXPLOSIONS; i++) {
        if (!mType->explosions[i].active) {
            mType->explosions[i].active = 1;
            mType->explosions[i].position.x = spawn.x;
            mType->explosions[i].position.y = spawn.y;
            mType->explosions[i].bbHalf.x = bbHalf.x;
            mType->explosions[i].bbHalf.y = bbHalf.y;
            mType->explosions[i].frame = 0;
            return true;
        }
    }
    return false;
}

bool ICACHE_FLASH_ATTR spawnEnemy (uint8_t type, vec_t spawn, int8_t health, vec_t bbHalf, int32_t frameOffset) {
    if (mType->enemiesInWave < MAX_ENEMIES) {
        for (int i = 0; i < MAX_ENEMIES; i++) {
            if (!mType->enemies[i].active) {
                mType->enemies[i].active = 1;
                mType->enemies[i].type = type;
                mType->enemies[i].health = health;
                mType->enemies[i].position.x = spawn.x;
                mType->enemies[i].position.y = spawn.y;
                mType->enemies[i].bbHalf.x = bbHalf.x;
                mType->enemies[i].bbHalf.y = bbHalf.y;
                mType->enemies[i].spawn.x = spawn.x;
                mType->enemies[i].spawn.y = spawn.y;
                mType->enemies[i].frameOffset = frameOffset;
                mType->enemies[i].shotCooldown = 0;
                mType->enemiesInWave++;
                return true;
            }
        }
    }
    return false;
}

void ICACHE_FLASH_ATTR spawnEnemyFormation (uint8_t type, vec_t spawn, int8_t health, vec_t bbHalf, int32_t frameOffset, uint8_t numEnemies, int16_t xSpacing, int16_t ySpacing) {
    // bool spawnSuccess = true;
    for (int i = 0; i < numEnemies; i++) {
        vec_t currentSpawn;
        currentSpawn.x = spawn.x + i * xSpacing;
        currentSpawn.y = spawn.y + i * ySpacing;
        int32_t currentFrameOffset = frameOffset * i;
        spawnEnemy(type, currentSpawn, health, bbHalf, currentFrameOffset);
    }
}

void ICACHE_FLASH_ATTR enemyDeath (uint8_t index) {
    // Update the enemy counter, useful for optimizing enemy spawning for later waves.
    if (mType->enemiesInWave > 0) {
        mType->enemiesInWave--;
    }

    spawnExplosion(mType->enemies[index].position, mType->enemies[index].bbHalf);
    mType->enemies[index].active = 0;

    // powerup spawn determined by chance augmented by difficulty.
    uint8_t powerupChance = 25 - (5 * mType->difficulty);
    if (os_random() % 100 <= powerupChance) {
        for (int k = 0; k < MAX_POWERUPS; k++) {
            if (!mType->powerups[k].active) {
                mType->powerups[k].active = 1;
                mType->powerups[k].type = PWRUP_FP;
                mType->powerups[k].position.x = mType->enemies[index].position.x;
                mType->powerups[k].position.y = mType->enemies[index].position.y;
                mType->powerups[k].bbHalf.x = 2;
                mType->powerups[k].bbHalf.y = 2;
                mType->powerups[k].driftDelay = 0;
                mType->powerups[k].speed = 1;
                break;
            }
        }
    }
}

void ICACHE_FLASH_ATTR playerDeath (void) {
    if (mType->player.numLives > 0) {
        // decrease remaining lives.
        mType->player.numLives--;

        // spawn explosion at dead player position.
        vec_t playerPos;
        playerPos.x = (int16_t)mType->player.position.x;
        playerPos.y = (int16_t)mType->player.position.y;
        spawnExplosion(playerPos, mType->player.bbHalf);

        // deactivate projectiles.
        for (int i  = 0; i < MAX_PROJECTILES; i++) {
            mType->projectiles[i].active = 0;
        }
        
        // reset player and move back to start.
        mType->player.position.x = PLAYER_START_X;
        mType->player.position.y = PLAYER_START_Y;
        mType->player.lastPosition.x = PLAYER_START_X;
        mType->player.lastPosition.y = PLAYER_START_Y;
        mType->player.shotLevel = 0;
        mType->player.shotCooldown = PLAYER_SHOT_COOLDOWN;
        mType->player.abilityChargeCounter = 0;
        mType->player.abilityCountdown = 0;

        // start spawn invincibility.
        mType->player.invincibilityCountdown = PLAYER_INVINCIBILITY_TIME;
    }
}