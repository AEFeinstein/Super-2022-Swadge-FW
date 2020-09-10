/*
 * mode_galaga.c
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
Start with the basic mechanics of galaga, add a reflector shield with a CD, and more varied enemy attack patterns, etc.

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

#define BTN_GAME_UP UP
#define BTN_GAME_DOWN DOWN
#define BTN_GAME_LEFT LEFT
#define BTN_GAME_RIGHT RIGHT

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
    GA_TITLE,
    GA_GAME,
    GA_SCORES,
    GA_GAMEOVER
} galagaState_t;

typedef struct
{
    galagaState_t state;

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
} galaga_t;

/*============================================================================
 * Prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR gaEnterMode(void);
void ICACHE_FLASH_ATTR gaExitMode(void);
void ICACHE_FLASH_ATTR gaButtonCallback(uint8_t state __attribute__((unused)), int button, int down);
void ICACHE_FLASH_ATTR gaAccelerometerCallback(accel_t* accel);
void ICACHE_FLASH_ATTR gaAudioCallback(int32_t samp);

static void ICACHE_FLASH_ATTR gaUpdate(void* arg __attribute__((unused)));

// handle inputs.
void ICACHE_FLASH_ATTR gaTitleInput(void);
void ICACHE_FLASH_ATTR gaGameInput(void);
void ICACHE_FLASH_ATTR gaScoresInput(void);
void ICACHE_FLASH_ATTR gaGameoverInput(void);

// update any input-unrelated logic.
void ICACHE_FLASH_ATTR gaTitleLogic(void);
void ICACHE_FLASH_ATTR gaGameLogic(void);
void ICACHE_FLASH_ATTR gaScoresLogic(void);
void ICACHE_FLASH_ATTR gaGameoverLogic(void);

// draw the frame.
void ICACHE_FLASH_ATTR gaTitleDisplay(void);
void ICACHE_FLASH_ATTR gaGameDisplay(void);
void ICACHE_FLASH_ATTR gaScoresDisplay(void);
void ICACHE_FLASH_ATTR gaGameoverDisplay(void);

// mode state management.
void ICACHE_FLASH_ATTR gaSetState(galagaState_t newState);

static void ICACHE_FLASH_ATTR gaMenuCallback(const char* menuItem);

// input checking.
bool ICACHE_FLASH_ATTR gaIsButtonPressed(uint8_t button);
bool ICACHE_FLASH_ATTR gaIsButtonReleased(uint8_t button);
bool ICACHE_FLASH_ATTR gaIsButtonDown(uint8_t button);
bool ICACHE_FLASH_ATTR gaIsButtonUp(uint8_t button);

//TODO: drawing functions.

//TODO: score operations.
/*void ICACHE_FLASH_ATTR loadHighScores(void);
void ICACHE_FLASH_ATTR saveHighScores(void);
bool ICACHE_FLASH_ATTR updateHighScores(uint32_t newScore);*/

//TODO: LED FX functions.


uint8_t ICACHE_FLASH_ATTR getTextWidth(char* text, fonts font);

bool AABBCollision (int ax0, int ay0, int ax1, int ay1, int bx0, int by0, int bx1, int by1);

/*============================================================================
 * Variables
 *==========================================================================*/

swadgeMode galagaMode =
{
    .modeName = "galaga",
    .fnEnterMode = gaEnterMode,
    .fnExitMode = gaExitMode,
    .fnButtonCallback = gaButtonCallback,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = gaAccelerometerCallback,
    .fnAudioCallback = gaAudioCallback,
    .menuImg = "copter-menu.gif" //TODO: need a menu image to link to here.
};

galaga_t* galaga;

static const char ga_title[]  = "M-TYPE";
static const char ga_start[]  = "START";
static const char ga_scores[] = "HIGH SCORES";
static const char ga_quit[]   = "QUIT";

/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * Initializer for galaga
 */
void ICACHE_FLASH_ATTR gaEnterMode(void)
{
    // Give us responsive input.
    enableDebounce(false);

    // Alloc and clear everything.
    galaga = os_malloc(sizeof(galaga_t));
    ets_memset(galaga, 0, sizeof(galaga_t));

    // Reset mode time tracking.
    galaga->modeStartTime = system_get_time();
    galaga->modeTime = 0;
    galaga->modeFrames = 0;

    // Reset input tracking.
    galaga->buttonState = 0;
    galaga->lastButtonState = 0;

    // TODO: is this the correct way to initialize this?
    ets_memset(&(galaga->accel), 0, sizeof(accel_t));
    ets_memset(&(galaga->lastAccel), 0, sizeof(accel_t));

    // Reset mode state.
    galaga->state = GA_TITLE;
    gaSetState(GA_TITLE);

    // Init menu system.
    galaga->menu = initMenu(ga_title, gaMenuCallback); //TODO: input handler for menu?
    addRowToMenu(galaga->menu);
    addItemToRow(galaga->menu, ga_start);
    addRowToMenu(galaga->menu);
    addItemToRow(galaga->menu, ga_scores);
    addRowToMenu(galaga->menu);
    addItemToRow(galaga->menu, ga_quit);
    drawMenu(galaga->menu);

    // Start the update loop.
    timerDisarm(&(galaga->updateTimer));
    timerSetFn(&(galaga->updateTimer), gaUpdate, NULL);
    timerArm(&(galaga->updateTimer), UPDATE_TIME_MS, true);

    //InitColorChord(); //TODO: Initialize preferred swadge LED behavior.
}

/**
 * Called when galaga is exited
 */
void ICACHE_FLASH_ATTR gaExitMode(void)
{
    timerDisarm(&(galaga->updateTimer));
    timerFlush();
    deinitMenu(galaga->menu);
    //clear(&galaga->obstacles);
    // TODO: free and clear everything.
    os_free(galaga);
}

/**
 * TODO
 *
 * @param state  A bitmask of all button states, unused
 * @param button The button which triggered this event
 * @param down   true if the button was pressed, false if it was released
 */
void ICACHE_FLASH_ATTR gaButtonCallback( uint8_t state, int button, int down )
{
    galaga->buttonState = state; // Set the state of all buttons
    if (galaga->state == GA_TITLE && down) menuButton(galaga->menu, button); // Pass input to menu if appropriate.
}

void ICACHE_FLASH_ATTR gaAccelerometerCallback(accel_t* accel)
{
    galaga->accel.x = accel->x;   // Set the accelerometer values
    galaga->accel.y = accel->y;
    galaga->accel.z = accel->z;
}

/**
 * This is called every time an audio sample is read from the ADC
 * This processes the sample and will display update the LEDs every
 * 128 samples
 *
 * @param samp A 32 bit audio sample read from the ADC (microphone)
 */
void ICACHE_FLASH_ATTR gaAudioCallback(int32_t samp)
{
    //TODO: should I even do anything with this?
}

/**
 * @brief called on a timer, updates the game state
 *
 * @param arg
 */
static void ICACHE_FLASH_ATTR gaUpdate(void* arg __attribute__((unused)))
{
    // Update time tracking.
    // NOTE: delta time is in microseconds.
    // UPDATE time is in milliseconds.

    uint32_t newModeTime = system_get_time() - galaga->modeStartTime;
    uint32_t newStateTime = system_get_time() - galaga->stateStartTime;
    galaga->deltaTime = newModeTime - galaga->modeTime;
    galaga->modeTime = newModeTime;
    galaga->stateTime = newStateTime;
    galaga->modeFrames++;
    galaga->stateFrames++;

    // Handle Input
    switch( galaga->state )
    {
        default:
        case GA_TITLE:
        {
            gaTitleInput();
            break;
        }
        case GA_GAME:
        {
            gaGameInput();
            break;
        }
        case GA_SCORES:
        {
            gaScoresInput();
            break;
        }
        case GA_GAMEOVER:
        {
            gaGameoverInput();
            break;
        }
    };

    // Mark what our inputs were the last time we acted on them.
    galaga->lastButtonState = galaga->buttonState;
    galaga->lastAccel = galaga->accel;

    // Handle State Logic
    switch( galaga->state )
    {
        default:
        case GA_TITLE:
        {
            gaTitleLogic();
            break;
        }
        case GA_GAME:
        {
            gaGameLogic();
            break;
        }
        case GA_SCORES:
        {
            gaScoresLogic();
            break;
        }
        case GA_GAMEOVER:
        {
            gaGameoverLogic();
            break;
        }
    };

    // Handle Drawing Frame (based on the state)
    switch( galaga->state )
    {
        default:
        case GA_TITLE:
        {
            gaTitleDisplay();
            break;
        }
        case GA_GAME:
        {
            gaGameDisplay();
            break;
        }
        case GA_SCORES:
        {
            gaScoresDisplay();
            break;
        }
        case GA_GAMEOVER:
        {
            gaGameoverDisplay();
            break;
        }
    };

    int projectiles = 0;

    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (galaga->projectiles[i].active) {
            projectiles++;
        }
    }

    // Draw debug FPS counter.
    /*char uiStr[32] = {0};
    double seconds = ((double)galaga->stateTime * (double)US_TO_MS_FACTOR * (double)MS_TO_S_FACTOR);
    int32_t fps = (int)((double)galaga->stateFrames / seconds);
    ets_snprintf(uiStr, sizeof(uiStr), "FPS: %d", fps);
    plotText(OLED_WIDTH - getTextWidth(uiStr, TOM_THUMB) - 1, OLED_HEIGHT - (1 * (FONT_HEIGHT_TOMTHUMB + 1)), uiStr, TOM_THUMB, WHITE);*/
}

// helper functions.

void ICACHE_FLASH_ATTR gaSetState(galagaState_t newState)
{
    galagaState_t prevState = galaga->state;
    galaga->state = newState;
    galaga->stateStartTime = system_get_time();
    galaga->stateTime = 0;
    galaga->stateFrames = 0;

    switch( galaga->state )
    {
        default:
        case GA_TITLE:
            break;
        case GA_GAME:
            galaga->player.position.x = 10;
            galaga->player.position.y = OLED_HALF_HEIGHT;
            galaga->player.shotCooldown = 0;
            galaga->player.reflectCounter = 0;
            galaga->player.reflectCountdown = 0;
            galaga->score = 0;
            for (int i = 0; i < MAX_PROJECTILES; i++) {
                galaga->projectiles[i].active = 0;
                galaga->projectiles[i].owner = OWNER_PLAYER;
                galaga->projectiles[i].position.x = 0;
                galaga->projectiles[i].position.y = 0;
                galaga->projectiles[i].direction.x = 0;
                galaga->projectiles[i].direction.y = 0;
                galaga->projectiles[i].speed = 1;
                galaga->projectiles[i].damage = 1;
            }

            for (int i = 0; i < 8; i++) {
                galaga->enemies[i].active = 1;
                galaga->enemies[i].health = 2;
                galaga->enemies[i].position.x = i < 4 ? OLED_WIDTH - 10 : OLED_WIDTH - 20;
                galaga->enemies[i].position.y = OLED_HEIGHT - 20 - (10 * (i % 4));
            }

            galaga->floor = OLED_HEIGHT - FONT_HEIGHT_TOMTHUMB - 3;//OLED_HEIGHT - 1;
            ets_memset(galaga->floors, galaga->floor, (NUM_CHUNKS + 1) * sizeof(uint8_t));
            galaga->xOffset = 0;
            
            break;
        case GA_SCORES:
            break;
        case GA_GAMEOVER:
            break;
    };
}

bool ICACHE_FLASH_ATTR gaIsButtonPressed(uint8_t button)
{
    return (galaga->buttonState & button) && !(galaga->lastButtonState & button);
}

bool ICACHE_FLASH_ATTR gaIsButtonReleased(uint8_t button)
{
    return !(galaga->buttonState & button) && (galaga->lastButtonState & button);
}

bool ICACHE_FLASH_ATTR gaIsButtonDown(uint8_t button)
{
    return galaga->buttonState & button;
}

bool ICACHE_FLASH_ATTR gaIsButtonUp(uint8_t button)
{
    return !(galaga->buttonState & button);
}

void ICACHE_FLASH_ATTR gaTitleInput(void)
{
    
}

void ICACHE_FLASH_ATTR gaTitleLogic(void)
{

}

void ICACHE_FLASH_ATTR gaTitleDisplay(void)
{
    drawMenu(galaga->menu);
}

void ICACHE_FLASH_ATTR gaGameInput(void)
{
    //TODO: account for good movement if multiple axis of movement are in play.
    if (gaIsButtonDown(UP)) {
        galaga->player.position.y--;
    }
    if (gaIsButtonDown(DOWN)) {
        galaga->player.position.y++;
    }
    if (gaIsButtonDown(LEFT)) {
        galaga->player.position.x--;
    }
    if (gaIsButtonDown(RIGHT)) {
        galaga->player.position.x++;
    }

    // clamp position of player to within the bounds of the screen.
    if (galaga->player.position.x < 0) {
        galaga->player.position.x = 0;
    }
    else if (galaga->player.position.x >= OLED_WIDTH) {
        galaga->player.position.x = OLED_WIDTH - 1;
    }
    if (galaga->player.position.y < 0) {
        galaga->player.position.y = 0;
    }
    else if (galaga->player.position.y >= OLED_HEIGHT) {
        galaga->player.position.y = OLED_HEIGHT - 1;
    }

    galaga->player.shotCooldown += galaga->deltaTime;

    if (gaIsButtonPressed(ACTION) && galaga->player.reflectCounter >= PLAYER_REFLECT_CHARGE_MAX) {
        //TODO: dodge roll or charge beam or reflect shield?
        galaga->player.reflectCounter = 0;
        galaga->player.reflectCountdown = PLAYER_REFLECT_TIME;
    }   

    if (gaIsButtonDown(ACTION) && galaga->player.shotCooldown >= PLAYER_SHOT_COOLDOWN) {

        //galaga->player.reflectCounter = 0;
        for (int i = 0; i < MAX_PROJECTILES; i++) {
            if (!galaga->projectiles[i].active) {
                galaga->player.shotCooldown = 0;
                galaga->projectiles[i].active = 1;
                galaga->projectiles[i].owner = OWNER_PLAYER;
                galaga->projectiles[i].position.x = galaga->player.position.x;
                galaga->projectiles[i].position.y = galaga->player.position.y;
                galaga->projectiles[i].direction.x = 1;
                galaga->projectiles[i].direction.y = 0;
                galaga->projectiles[i].speed = 4;
                galaga->projectiles[i].damage = 1;
                break;
            }
        }
    }

    if (gaIsButtonUp(ACTION)) {
        galaga->player.reflectCounter += galaga->deltaTime;
        if (galaga->player.reflectCounter > PLAYER_REFLECT_CHARGE_MAX) {
            galaga->player.reflectCounter = PLAYER_REFLECT_CHARGE_MAX;
        }
    }

    galaga->score++;
    // determine player move direction
    // determine if a projectile should be spawned
    // determine if the reflect shield should come up
}

void ICACHE_FLASH_ATTR gaGameLogic(void)
{
    // enemy movement and projectile spawning
    
    // projectile movement and collision
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (galaga->projectiles[i].active) {
            galaga->projectiles[i].position.x += (galaga->projectiles[i].direction.x * galaga->projectiles[i].speed);
            galaga->projectiles[i].position.y += (galaga->projectiles[i].direction.y * galaga->projectiles[i].speed);

            //TODO: check collision with players / enemies
            if (galaga->projectiles[i].owner == OWNER_PLAYER) {
                for (int j = 0; j < MAX_ENEMIES; j++) {
                    if (galaga->enemies[j].active) {
                        if (AABBCollision(galaga->projectiles[i].position.x, 
                            galaga->projectiles[i].position.y, 
                            galaga->projectiles[i].position.x, 
                            galaga->projectiles[i].position.y, 
                            galaga->enemies[j].position.x - 3, 
                            galaga->enemies[j].position.y - 3, 
                            galaga->enemies[j].position.x + 3, 
                            galaga->enemies[j].position.y + 3)) {
                            galaga->projectiles[i].active = 0;
                            galaga->enemies[j].health -= galaga->projectiles[i].damage;
                            if (galaga->enemies[j].health <= 0) {
                                galaga->enemies[j].active = 0;
                            }
                        }
                    }
                }
            }

            if (galaga->projectiles[i].owner == OWNER_PLAYER) {

            }

            if (galaga->projectiles[i].position.x >= OLED_WIDTH ||
                galaga->projectiles[i].position.x < 0 ||
                galaga->projectiles[i].position.y >= OLED_HEIGHT ||
                galaga->projectiles[i].position.y < 0) {
                galaga->projectiles[i].active = 0;
            }
        }   
    }

    if (galaga->player.reflectCountdown > 0) {
        galaga->player.reflectCountdown -= galaga->deltaTime;
        if (galaga->player.reflectCountdown < 0) {
            galaga->player.reflectCountdown = 0;
        }
    }

    // Increment the X offset for other walls
    galaga->xOffset++;

    // If we've moved CHUNK_WIDTH pixels
    if(galaga->xOffset == CHUNK_WIDTH)
    {
        // Reset the X offset
        galaga->xOffset = 0;

        // Shift all the chunk indices over one
        ets_memmove(&(galaga->floors[0]), &(galaga->floors[1]), NUM_CHUNKS);

        // Randomly generate new coordinates for a chunk
        galaga->floors[NUM_CHUNKS] = galaga->floor - (os_random() % RAND_WALLS_HEIGHT);
    }
}

void ICACHE_FLASH_ATTR gaGameDisplay(void)
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
        if (galaga->projectiles[i].active) {
            if (galaga->projectiles[i].owner == OWNER_PLAYER) {
                plotLine(galaga->projectiles[i].position.x, galaga->projectiles[i].position.y, galaga->projectiles[i].position.x - 5, galaga->projectiles[i].position.y, WHITE);
            }
            else {
                plotLine(galaga->projectiles[i].position.x, galaga->projectiles[i].position.y, galaga->projectiles[i].position.x, galaga->projectiles[i].position.y, WHITE);
            }
        }   
    }
    // draw enemies
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (galaga->enemies[i].active) {
            plotCircle(galaga->enemies[i].position.x, galaga->enemies[i].position.y, 3, WHITE);
        }
    }
    // draw player
    #define PLAYER_HALF_WIDTH 3
    fillDisplayArea(galaga->player.position.x - PLAYER_HALF_WIDTH, galaga->player.position.y - PLAYER_HALF_WIDTH, galaga->player.position.x + PLAYER_HALF_WIDTH, galaga->player.position.y + PLAYER_HALF_WIDTH, BLACK);
    plotRect(galaga->player.position.x - PLAYER_HALF_WIDTH, galaga->player.position.y - PLAYER_HALF_WIDTH, galaga->player.position.x + PLAYER_HALF_WIDTH, galaga->player.position.y + PLAYER_HALF_WIDTH, WHITE);
    if (galaga->player.reflectCountdown > 0) {
        plotCircle(galaga->player.position.x, galaga->player.position.y, (galaga->stateFrames % 3) + 4, WHITE);//galaga->stateFrames % 3 ? WHITE : BLACK);
    }
    //plotCircle(galaga->player.position.x, galaga->player.position.y, 5, WHITE);
    // draw ui
    /*
    reflect text
    reflect bar rect container
    reflect line bar fill
    score #
    */

    char uiStr[32] = {0};

    // score text
    ets_snprintf(uiStr, sizeof(uiStr), "%06d", galaga->score);
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
    double charge = (double)galaga->player.reflectCounter / PLAYER_REFLECT_CHARGE_MAX;
    int reflectBarFillX1 = reflectBarX0 + (charge * ((reflectBarX1 - 1) - reflectBarX0));
    plotLine(reflectBarX0, reflectBarY0 + 1, reflectBarFillX1, reflectBarY0 + 1, WHITE);

    // For each chunk coordinate
    for(uint8_t w = 0; w < NUM_CHUNKS + 1; w++)
    {
        // Plot a floor segment line between chunk coordinates
        plotLine(
            (w * CHUNK_WIDTH) - galaga->xOffset,
            galaga->floors[w],
            ((w + 1) * CHUNK_WIDTH) - galaga->xOffset,
            galaga->floors[w + 1],
            WHITE);
    }

    /*char uiStr[32] = {0};
    ets_snprintf(uiStr, sizeof(uiStr), "BTN: %d u:%d, d:%d, l:%d, r:%d, a:%d", button, upDown, downDown, leftDown, rightDown, actionDown);
    fillDisplayArea(OLED_WIDTH - getTextWidth(uiStr, TOM_THUMB) - 1, OLED_HEIGHT - (1 * (FONT_HEIGHT_TOMTHUMB + 1)), OLED_WIDTH, OLED_HEIGHT, BLACK);
    plotText(OLED_WIDTH - getTextWidth(uiStr, TOM_THUMB) - 1, OLED_HEIGHT - (1 * (FONT_HEIGHT_TOMTHUMB + 1)), uiStr, TOM_THUMB, WHITE);*/
    
}

void ICACHE_FLASH_ATTR gaScoresInput(void)
{

}

void ICACHE_FLASH_ATTR gaScoresLogic(void)
{

}

void ICACHE_FLASH_ATTR gaScoresDisplay(void)
{

}

void ICACHE_FLASH_ATTR gaGameoverInput(void)
{

}

void ICACHE_FLASH_ATTR gaGameoverLogic(void)
{

}

void ICACHE_FLASH_ATTR gaGameoverDisplay(void)
{

}

/**
 * TODO
 *
 * @param menuItem
 */
static void ICACHE_FLASH_ATTR gaMenuCallback(const char* menuItem)
{
    if (ga_start == menuItem)
    {
        // Change state to start game.
        gaSetState(GA_GAME);
    }
    else if (ga_scores == menuItem)
    {
        // Change state to score screen.
        gaSetState(GA_SCORES);
    }
    else if (ga_quit == menuItem)
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
