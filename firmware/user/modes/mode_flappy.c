/*
 * mode_flappy.c
 *
 *  Created on: Mar 27, 2019
 *      Author: adam
 */
#include "user_config.h"

/*============================================================================
 * Includes
 *==========================================================================*/

#include <osapi.h>
#include <mem.h>

#include "user_main.h"
#include "embeddednf.h"
#include "oled.h"
#include "bresenham.h"
#include "assets.h"
#include "buttons.h"
#include "menu2d.h"

/*============================================================================
 * Defines, Structs, Enums
 *==========================================================================*/

#define FLAPPY_UPDATE_MS 10
#define FLAPPY_UPDATE_S (FLAPPY_UPDATE_MS / 1000.0f)
#define FLAPPY_ACCEL     120.0f
#define FLAPPY_JUMP_VEL -50.0f

#define CHUNK_WIDTH 8
#define NUM_CHUNKS ((OLED_WIDTH/CHUNK_WIDTH)+1)
#define RAND_WALLS_HEIGHT 4

#define BIRD_HEIGHT 16

typedef enum
{
    FLAPPY_MENU,
    FLAPPY_GAME
} flappyGameMode;

typedef struct
{
    syncedTimer_t updateTimer;
    int samplesProcessed;
    flappyGameMode mode;
    uint8_t floors[NUM_CHUNKS + 1];
    uint8_t ceils[NUM_CHUNKS + 1];
    uint8_t floor;
    uint8_t height;
    uint8_t xOffset;
    uint32_t frames;
    float birdPos;
    float birdVel;
    uint8_t buttonState;

    menu_t* menu;
} flappy_t;

/*============================================================================
 * Prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR flappyEnterMode(void);
void ICACHE_FLASH_ATTR flappyExitMode(void);
void ICACHE_FLASH_ATTR flappyButtonCallback(uint8_t state __attribute__((unused)),
        int button, int down);
void ICACHE_FLASH_ATTR flappySampleHandler(int32_t samp);

static void ICACHE_FLASH_ATTR flappyUpdate(void* arg __attribute__((unused)));
static void ICACHE_FLASH_ATTR flappyMenuCb(const char* menuItem);
static void ICACHE_FLASH_ATTR flappyStartGame(const char* difficulty);

/*============================================================================
 * Variables
 *==========================================================================*/

swadgeMode flappyMode =
{
    .modeName = "flappy",
    .fnEnterMode = flappyEnterMode,
    .fnExitMode = flappyExitMode,
    .fnButtonCallback = flappyButtonCallback,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = NULL,
    .fnAudioCallback = flappySampleHandler,
};

flappy_t* flappy;

static const char fl_title[]  = "Flappy";
static const char fl_easy[]   = "EASY";
static const char fl_medium[] = "MED";
static const char fl_hard[]   = "HARD";
static const char fl_scores[] = "SCORES";
static const char fl_quit[]   = "QUIT";

/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * Initializer for flappy
 */
void ICACHE_FLASH_ATTR flappyEnterMode(void)
{
    // Alloc and clear everything
    flappy = os_malloc(sizeof(flappy_t));
    ets_memset(flappy, 0, sizeof(flappy_t));

    flappy->mode = FLAPPY_MENU;

    flappy->menu = initMenu(fl_title, flappyMenuCb);
    addRowToMenu(flappy->menu);
    addItemToRow(flappy->menu, fl_easy);
    addItemToRow(flappy->menu, fl_medium);
    addItemToRow(flappy->menu, fl_hard);
    addRowToMenu(flappy->menu);
    addItemToRow(flappy->menu, fl_scores);
    addRowToMenu(flappy->menu);
    addItemToRow(flappy->menu, fl_quit);
    drawMenu(flappy->menu);

    syncedTimerDisarm(&(flappy->updateTimer));
    syncedTimerSetFn(&(flappy->updateTimer), flappyUpdate, NULL);
    syncedTimerArm(&(flappy->updateTimer), FLAPPY_UPDATE_MS, true);
    enableDebounce(false);

    InitColorChord();
}

/**
 * Called when flappy is exited
 */
void ICACHE_FLASH_ATTR flappyExitMode(void)
{
    syncedTimerDisarm(&(flappy->updateTimer));
    syncedTimerFlush();
    deinitMenu(flappy->menu);
    os_free(flappy);
}

/**
 * TODO
 *
 * @param menuItem
 */
static void ICACHE_FLASH_ATTR flappyMenuCb(const char* menuItem)
{
    if(fl_easy == menuItem)
    {
        flappyStartGame(fl_easy);
    }
    else if (fl_medium == menuItem)
    {
        flappyStartGame(fl_medium);
    }
    else if (fl_hard == menuItem)
    {
        flappyStartGame(fl_hard);
    }
    else if (fl_scores == menuItem)
    {

    }
    else if (fl_quit == menuItem)
    {

    }
}

/**
 * TODO
 *
 */
static void ICACHE_FLASH_ATTR flappyStartGame(const char* difficulty)
{
    flappy->samplesProcessed = 0;
    flappy->mode = FLAPPY_GAME;

    ets_memset(flappy->floors, OLED_HEIGHT - 1, (NUM_CHUNKS + 1) * sizeof(uint8_t));
    ets_memset(flappy->ceils, 0,                (NUM_CHUNKS + 1) * sizeof(uint8_t));
    flappy->floor = OLED_HEIGHT - 1;
    flappy->height = OLED_HEIGHT - 1;
    flappy->xOffset = 0;

    flappy->frames = 0;
    flappy->birdPos = BIRD_HEIGHT;
    flappy->birdVel = 0;
}

/**
 * @brief called on a timer, updates the game state
 *
 * @param arg
 */
static void ICACHE_FLASH_ATTR flappyUpdate(void* arg __attribute__((unused)))
{
    switch(flappy->mode)
    {
        default:
        case FLAPPY_MENU:
        {
            drawMenu(flappy->menu);
            break;
        }
        case FLAPPY_GAME:
        {
            // Increment the frame count
            flappy->frames++;

            // Do something every CHUNK_WIDTH frames, alternating
            switch(flappy->frames % (CHUNK_WIDTH * 2))
            {
                case 0:
                {
                    // On one cycle, either raise or lower the floor randomly
                    if(0 == os_random() % 2)
                    {
                        // Raise the floor
                        flappy->floor += RAND_WALLS_HEIGHT;
                        if(flappy->floor > OLED_HEIGHT - 1)
                        {
                            flappy->floor = OLED_HEIGHT - 1;
                        }
                        break;
                    }
                    else
                    {
                        // Lower the floor
                        flappy->floor -= RAND_WALLS_HEIGHT;
                        uint8_t minFloor = flappy->height;
                        if(flappy->floor < minFloor)
                        {
                            flappy->floor = minFloor;
                        }
                        break;
                    }
                    break;
                }
                case CHUNK_WIDTH:
                {
                    // On the other cycle, decrease the height, narrowing the tunnel
                    if(flappy->height > BIRD_HEIGHT)
                    {
                        flappy->height--;
                    }
                    break;
                }
                default:
                {
                    // Every other frame, do nothing
                    break;
                }
            }

            // Increment the X offset for othe walls
            flappy->xOffset++;

            // If we've moved CHUNK_WIDTH pixels
            if(flappy->xOffset == CHUNK_WIDTH)
            {
                // Reset the X offset
                flappy->xOffset = 0;

                // Shift all the chunk indices over one
                ets_memmove(&(flappy->floors[0]), &(flappy->floors[1]), NUM_CHUNKS);
                ets_memmove(&(flappy->ceils[0]),  &(flappy->ceils[1]),  NUM_CHUNKS);

                // Randomly generate new coordinates for a chunk
                flappy->floors[NUM_CHUNKS] = flappy->floor - (os_random() % RAND_WALLS_HEIGHT);
                flappy->ceils[NUM_CHUNKS] = (flappy->floor - flappy->height) + (os_random() % RAND_WALLS_HEIGHT);
            }

            // Update the bird's position (velocity dependent)
            flappy->birdPos = flappy->birdPos +
                              (flappy->birdVel * FLAPPY_UPDATE_S) +
                              (FLAPPY_ACCEL * FLAPPY_UPDATE_S * FLAPPY_UPDATE_S) / 2;

            // Then Update the bird's velocity (position independent)
            flappy->birdVel = flappy->birdVel + (FLAPPY_ACCEL * FLAPPY_UPDATE_S);

            // Stop the bird at either the floor or the ceiling
            if(flappy->birdPos < 0)
            {
                flappy->birdPos = 0;
            }
            else if(flappy->birdPos > OLED_HEIGHT - BIRD_HEIGHT)
            {
                flappy->birdPos = OLED_HEIGHT - BIRD_HEIGHT;
                flappy->birdVel = 0;
            }

            // First clear the OLED
            clearDisplay();

            // For each chunk coordinate
            uint8_t w;
            for(w = 0; w < NUM_CHUNKS + 1; w++)
            {
                // Plot a floor segment line between chunk coordinates
                plotLine(
                    (w * CHUNK_WIDTH) - flappy->xOffset,
                    flappy->floors[w],
                    ((w + 1) * CHUNK_WIDTH) - flappy->xOffset,
                    flappy->floors[w + 1],
                    WHITE);

                // Plot a ceiling segment line between chunk coordinates
                plotLine(
                    (w * CHUNK_WIDTH) - flappy->xOffset,
                    flappy->ceils[w],
                    ((w + 1) * CHUNK_WIDTH) - flappy->xOffset,
                    flappy->ceils[w + 1],
                    WHITE);
            }

            // Find the bird's integer position
            int16_t birdPos = (int16_t)(flappy->birdPos + 0.5f);

            // Iterate over the bird's sprite to see if it would be drawn over a wall
            bool collision = false;
            for(uint8_t x = 0; x < BIRD_HEIGHT; x++)
            {
                for(uint8_t y = 0; y < BIRD_HEIGHT; y++)
                {
                    // The pixel is already white, so there's a collision!
                    if(WHITE == getPixel(x, birdPos + y))
                    {
                        collision = true;
                        break;
                    }
                }
            }

            // If there was a collision
            if(true == collision)
            {
                // Immediately jump back to the menu
                flappy->mode = FLAPPY_MENU;
            }
            else
            {
                // Otherwise draw the bird and continue
                drawBitmapFromAsset("dino.png", 0, (int16_t)(flappy->birdPos + 0.5f), true, false, 0);
            }

            break;
        }
    }
}

/**
 * TODO
 *
 * @param state  A bitmask of all button states, unused
 * @param button The button which triggered this event
 * @param down   true if the button was pressed, false if it was released
 */
void ICACHE_FLASH_ATTR flappyButtonCallback( uint8_t state,
        int button __attribute__((unused)), int down __attribute__((unused)))
{
    switch (flappy->mode)
    {
        default:
        case FLAPPY_MENU:
        {
            if(down)
            {
                menuButton(flappy->menu, button);
            }
            break;
        }
        case FLAPPY_GAME:
        {
            if(down)
            {
                flappy->birdVel = FLAPPY_JUMP_VEL;
            }
            flappy->buttonState = state;
            break;
        }
    }
}

/**
 * This is called every time an audio sample is read from the ADC
 * This processes the sample and will display update the LEDs every
 * 128 samples
 *
 * @param samp A 32 bit audio sample read from the ADC (microphone)
 */
void ICACHE_FLASH_ATTR flappySampleHandler(int32_t samp)
{
    switch(flappy->mode)
    {
        default:
        case FLAPPY_MENU:
        {
            break;
        }
        case FLAPPY_GAME:
        {
            PushSample32( samp );
            flappy->samplesProcessed++;

            // If at least 128 samples have been processed
            if( flappy->samplesProcessed >= 128 )
            {
                // Colorchord magic
                HandleFrameInfo();

                // Reset the sample count
                flappy->samplesProcessed = 0;
            }
        }
    }
}
