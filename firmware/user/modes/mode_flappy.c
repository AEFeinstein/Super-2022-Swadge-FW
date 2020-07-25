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
#include <stdint.h>

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

/*============================================================================
 * Defines, Structs, Enums
 *==========================================================================*/

#define FLAPPY_UPDATE_MS 20
#define FLAPPY_UPDATE_S (FLAPPY_UPDATE_MS / 1000.0f)
#define FLAPPY_ACCEL     120.0f
// #define FLAPPY_JUMP_VEL -50.0f

#define CHUNK_WIDTH 8
#define NUM_CHUNKS ((OLED_WIDTH/CHUNK_WIDTH)+1)
#define RAND_WALLS_HEIGHT 4

#define CHOPPER_HEIGHT 4

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
    float chopperPos;
    float chopperVel;
    uint8_t buttonState;
    list_t obstacles;
    uint8_t obsHeight;

    uint8_t oldPeakFreq;
    uint8_t peakFreq;

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
// static uint8_t ICACHE_FLASH_ATTR findPeakFreq(void);

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
    .menuImg = "copter-menu.gif"
};

flappy_t* flappy;

static const char fl_title[]  = "Flappy";
static const char fl_mic_easy[]   = "MIC EASY";
static const char fl_mic_medium[] = "MIC MED";
static const char fl_mic_hard[]   = "MIC HARD";
static const char fl_btn_easy[]   = "BTN EASY";
static const char fl_btn_medium[] = "BTN MED";
static const char fl_btn_hard[]   = "BTN HARD";
static const char fl_scores[] = "HIGH SCORES";
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
    addItemToRow(flappy->menu, fl_mic_easy);
    addItemToRow(flappy->menu, fl_mic_medium);
    addItemToRow(flappy->menu, fl_mic_hard);
    addRowToMenu(flappy->menu);
    addItemToRow(flappy->menu, fl_btn_easy);
    addItemToRow(flappy->menu, fl_btn_medium);
    addItemToRow(flappy->menu, fl_btn_hard);
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
    clear(&flappy->obstacles);
    os_free(flappy);
}

/**
 * TODO
 *
 * @param menuItem
 */
static void ICACHE_FLASH_ATTR flappyMenuCb(const char* menuItem)
{
    if(fl_mic_easy == menuItem)
    {
        flappyStartGame(fl_mic_easy);
    }
    else if (fl_mic_medium == menuItem)
    {
        flappyStartGame(fl_mic_medium);
    }
    else if (fl_mic_hard == menuItem)
    {
        flappyStartGame(fl_mic_hard);
    }
    if(fl_btn_easy == menuItem)
    {
        flappyStartGame(fl_btn_easy);
    }
    else if (fl_btn_medium == menuItem)
    {
        flappyStartGame(fl_btn_medium);
    }
    else if (fl_btn_hard == menuItem)
    {
        flappyStartGame(fl_btn_hard);
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
    os_printf("Start %s mode", difficulty);
    flappy->samplesProcessed = 0;
    flappy->mode = FLAPPY_GAME;

    ets_memset(flappy->floors, OLED_HEIGHT - 1, (NUM_CHUNKS + 1) * sizeof(uint8_t));
    ets_memset(flappy->ceils, 0,                (NUM_CHUNKS + 1) * sizeof(uint8_t));
    flappy->floor = OLED_HEIGHT - 1;
    flappy->height = OLED_HEIGHT - 1;
    flappy->xOffset = 0;

    flappy->frames = 0;
    flappy->chopperPos = (OLED_HEIGHT - CHOPPER_HEIGHT) / 2;
    flappy->chopperVel = 0;
    flappy->obsHeight = 16;

    flappy->peakFreq = 0;
    flappy->oldPeakFreq = 0;

    clear(&flappy->obstacles);
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

            // Every four full screens
            if(flappy->frames % (OLED_WIDTH * 4) == 0)
            {
                if(flappy->floor > OLED_HEIGHT - 1 - 3 * RAND_WALLS_HEIGHT)
                {
                    // Raise the floor
                    flappy->floor -= RAND_WALLS_HEIGHT;
                    if(flappy->floor > OLED_HEIGHT - 1)
                    {
                        flappy->floor = OLED_HEIGHT - 1;
                    }

                    // Decrease the height, narrowing the tunnel
                    if(flappy->height > CHOPPER_HEIGHT)
                    {
                        flappy->height -= (2 * RAND_WALLS_HEIGHT);
                    }
                }
            }

            // Increment the X offset for other walls
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

            // Every half a screen, spawn a new obstacle
            if(flappy->frames % (OLED_WIDTH / 2) == 0)
            {
                // Find the bounds where this obstacle can be drawn
                uint8_t minObsY = flappy->ceils[NUM_CHUNKS];
                uint8_t maxObsY = flappy->floors[NUM_CHUNKS] - flappy->obsHeight;

                // Pick a random position within the bounds
                uint8_t obsY = minObsY + (os_random() % (maxObsY - minObsY));
                // Push it on the linked list, storing the X and Y coords as the pointer
                push(&(flappy->obstacles), (void*)((uintptr_t)((OLED_WIDTH << 8) | obsY)));
            }

            // If the button is not pressed
            if(!(flappy->buttonState & 0x10))
            {
                // Gravity is positive
                // Update the chopper's position (velocity dependent)
                flappy->chopperPos = flappy->chopperPos +
                                     (flappy->chopperVel * FLAPPY_UPDATE_S) +
                                     (FLAPPY_ACCEL * FLAPPY_UPDATE_S * FLAPPY_UPDATE_S) / 2;
                // Then Update the chopper's velocity (position independent)
                flappy->chopperVel = flappy->chopperVel + (FLAPPY_ACCEL * FLAPPY_UPDATE_S);
            }
            else
            {
                // Gravity is negative
                // Update the chopper's position (velocity dependent)
                flappy->chopperPos = flappy->chopperPos +
                                     (flappy->chopperVel * FLAPPY_UPDATE_S) -
                                     (FLAPPY_ACCEL * FLAPPY_UPDATE_S * FLAPPY_UPDATE_S) / 2;
                // Then Update the chopper's velocity (position independent)
                flappy->chopperVel = flappy->chopperVel - (FLAPPY_ACCEL * FLAPPY_UPDATE_S);
            }

            // Stop the chopper at either the floor or the ceiling
            if(flappy->chopperPos < 0)
            {
                flappy->chopperPos = 0;
                flappy->chopperVel = 0;
            }
            else if(flappy->chopperPos > OLED_HEIGHT - CHOPPER_HEIGHT)
            {
                flappy->chopperPos = OLED_HEIGHT - CHOPPER_HEIGHT;
                flappy->chopperVel = 0;
            }

            // First clear the OLED
            clearDisplay();

            // For each chunk coordinate
            for(uint8_t w = 0; w < NUM_CHUNKS + 1; w++)
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

            // For each obstacle
            node_t* obs = flappy->obstacles.first;
            while(obs != NULL)
            {
                // Shift the obstacle
                obs->val = (void*)(((uintptr_t)obs->val) - 0x100);

                // Extract X and Y coordinates
                int8_t x = (((uintptr_t)obs->val) >> 8) & 0xFF;
                int8_t y = (((uintptr_t)obs->val)     ) & 0xFF;

                // If the obstacle is off the screen
                if(x + 2 <= 0)
                {
                    // Move to the next
                    obs = obs->next;
                    // Remove it from the linked list
                    removeEntry(&flappy->obstacles, obs->prev);
                }
                else
                {
                    // Otherwise draw it
                    plotRect(x, y, x + 2, y + flappy->obsHeight, WHITE);
                    // Move to the next
                    obs = obs->next;
                }
            }

            // Find the chopper's integer position
            int16_t chopperPos = (int16_t)(flappy->chopperPos + 0.5f);

            // Iterate over the chopper's sprite to see if it would be drawn over a wall
            bool collision = false;
            for(uint8_t x = 0; x < CHOPPER_HEIGHT; x++)
            {
                for(uint8_t y = 0; y < CHOPPER_HEIGHT; y++)
                {
                    // The pixel is already white, so there's a collision!
                    if(WHITE == getPixel(x, chopperPos + y))
                    {
                        collision = true;
                        break;
                    }
                    else
                    {
                        drawPixel(x, chopperPos + y, WHITE);
                    }
                }
            }

            // If there was a collision
            if(true == collision)
            {
                // Immediately jump back to the menu
                flappy->mode = FLAPPY_MENU;
                os_printf("Score: %d\n", flappy->frames / 8);
            }

            // Render the score as text
            char framesStr[8] = {0};
            ets_snprintf(framesStr, sizeof(framesStr), "%d", flappy->frames / 8);
            int16_t framesStrWidth = textWidth(framesStr, IBM_VGA_8);
            // Make sure the width is a multiple of 8 to keep it drawn consistently
            while(framesStrWidth % 8 != 0)
            {
                framesStrWidth++;
            }
            // Draw the score in the upper right hand corner
            fillDisplayArea(OLED_WIDTH - 1 - framesStrWidth, 0, OLED_WIDTH, FONT_HEIGHT_IBMVGA8 + 1, BLACK);
            plotText(OLED_WIDTH - framesStrWidth, 0, framesStr, IBM_VGA_8, WHITE);

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

                static uint8_t shift = 0;
                static uint8_t mode = 0;
                led_t leds[NUM_LIN_LEDS] = {{0}};
                switch(mode)
                {
                    default:
                    case 0:
                    {
                        leds[0].r = 0xFF >> shift;
                        leds[1].g = 0xFF >> shift;
                        leds[2].b = 0xFF >> shift;
                        leds[3].b = 0xFF >> shift;
                        leds[4].g = 0xFF >> shift;
                        leds[5].r = 0xFF >> shift;
                        break;
                    }
                    case 1:
                    {
                        leds[0].r = 0xFF >> shift;
                        leds[0].g = 0xFF >> shift;
                        leds[1].g = 0xFF >> shift;
                        leds[1].b = 0xFF >> shift;
                        leds[2].b = 0xFF >> shift;
                        leds[2].r = 0xFF >> shift;
                        leds[3].b = 0xFF >> shift;
                        leds[3].r = 0xFF >> shift;
                        leds[4].g = 0xFF >> shift;
                        leds[4].b = 0xFF >> shift;
                        leds[5].r = 0xFF >> shift;
                        leds[5].g = 0xFF >> shift;
                        break;
                    }
                    case 2:
                    {
                        ets_memset(leds, 0xFF >> shift, sizeof(leds));
                        break;
                    }
                }
                shift = (shift + 1) % 8;
                if(0 == shift)
                {
                    mode = (mode + 1) % 3;
                }
                setLeds(leds, sizeof(leds));

            }
            break;
        }
        case FLAPPY_GAME:
        {
            // if(down)
            // {
            //     flappy->chopperVel = FLAPPY_JUMP_VEL;
            // }
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

                // flappy->oldPeakFreq = flappy->peakFreq;
                // flappy->peakFreq = findPeakFreq();
                // // os_printf("%d\n", flappy->peakFreq);

                // static int maxF = 0;
                // if(flappy->peakFreq > maxF)
                // {
                //     maxF = flappy->peakFreq;
                //     os_printf("MF %d\n", maxF);
                // }

                // int16_t delta = flappy->peakFreq - flappy->oldPeakFreq;
                // if(delta > 80)
                // {
                //     delta = -(delta - 191);
                // }
                // else if (delta < -80)
                // {
                //     delta = -(delta + 191);
                // }

                // if((1 < delta && delta < 7) || (-7 < delta && delta < -1) )
                // {
                //     os_printf("%d\n", delta);

                //     flappy->chopperPos -= (delta / 2);
                //     if(flappy->chopperPos < 0)
                //     {
                //         flappy->chopperPos = 0;
                //     }
                //     else if (flappy->chopperPos > OLED_HEIGHT - 16)
                //     {
                //         flappy->chopperPos = OLED_HEIGHT - 16;
                //     }
                // }

                // if(flappy->peakFreq > flappy->oldPeakFreq)
                // {
                //     // TODO go up!
                // }
                // else if(flappy->peakFreq > flappy->oldPeakFreq)
                // {
                //     // TODO go down!
                // }

                // Reset the sample count
                flappy->samplesProcessed = 0;
            }
        }
    }
}

/**
 * TODO Test this
 *
 * @return uint32_t
 */
// static uint8_t ICACHE_FLASH_ATTR findPeakFreq(void)
// {
//     uint8_t maxFreq = 0;
//     uint16_t maxAmp = 0;

//     for(uint8_t i = 0; i < MAXNOTES; i++ )
//     {
//         if( note_peak_amps2[i] > maxAmp && note_peak_freqs[i] != 255 )
//         {
//             maxFreq = note_peak_freqs[i];
//             maxAmp = note_peak_amps2[i];
//         }
//     }
//     return maxFreq;
// }