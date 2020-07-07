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

/*============================================================================
 * Defines, Structs, Enums
 *==========================================================================*/

#define CHUNK_WIDTH 8
#define NUM_CHUNKS ((OLED_WIDTH/CHUNK_WIDTH)+1)
#define RAND_WALLS_HEIGHT 4

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
    uint8_t offset;
    uint32_t frames;
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

/*============================================================================
 * Const data
 *==========================================================================*/


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

    flappy->mode = FLAPPY_GAME;
    flappy->floor = OLED_HEIGHT - 1;
    flappy->height = OLED_HEIGHT - 1;
    ets_memset(flappy->floors, OLED_HEIGHT - 1, (NUM_CHUNKS + 1) * sizeof(uint8_t));
    ets_memset(flappy->ceils, 0,                (NUM_CHUNKS + 1) * sizeof(uint8_t));

    syncedTimerDisarm(&(flappy->updateTimer));
    syncedTimerSetFn(&(flappy->updateTimer), flappyUpdate, NULL);

    InitColorChord();
    clearDisplay();
}

/**
 * Called when flappy is exited
 */
void ICACHE_FLASH_ATTR flappyExitMode(void)
{
    syncedTimerDisarm(&(flappy->updateTimer));
    os_free(flappy);
}

/**
 * @brief called on a timer, updates the game state
 *
 * @param arg
 */
static void ICACHE_FLASH_ATTR flappyUpdate(void* arg __attribute__((unused)))
{
    clearDisplay();

    flappy->frames++;
    switch(flappy->frames % 16)
    {
        case 0:
        {
            switch(os_random() % 2)
            {
                case 0:
                {
                    flappy->floor += 4;
                    if(flappy->floor > OLED_HEIGHT - 1)
                    {
                        flappy->floor = OLED_HEIGHT - 1;
                    }
                    break;
                }
                case 1:
                {
                    flappy->floor -= 4;
                    uint8_t minFloor = flappy->height + 1;
                    if(flappy->floor < minFloor)
                    {
                        flappy->floor = minFloor;
                    }
                    break;
                }
            }
            break;
        }
        case 8:
        {
            if(flappy->height > 16)
            {
                flappy->height--;
            }
            break;
        }
    }

    flappy->offset++;
    if(flappy->offset == CHUNK_WIDTH)
    {
        uint8_t fl = OLED_HEIGHT - (os_random() % (OLED_HEIGHT - flappy->height));

        flappy->offset = 0;
        ets_memmove(&(flappy->floors[0]), &(flappy->floors[1]), NUM_CHUNKS);
        ets_memmove(&(flappy->ceils[0]),  &(flappy->ceils[1]),  NUM_CHUNKS);

        flappy->floors[NUM_CHUNKS] = flappy->floor - (os_random() % RAND_WALLS_HEIGHT);
        os_printf("%d %d\n", flappy->floor, flappy->floors[NUM_CHUNKS]);
        flappy->ceils[NUM_CHUNKS] = (flappy->floor - flappy->height) + (os_random() % RAND_WALLS_HEIGHT);
    }

    uint8_t w;
    for(w = 0; w < NUM_CHUNKS + 1; w++)
    {
        plotLine(
            (w * CHUNK_WIDTH) - flappy->offset,
            flappy->floors[w],
            ((w + 1) * CHUNK_WIDTH) - flappy->offset,
            flappy->floors[w + 1],
            WHITE);
        plotLine(
            (w * CHUNK_WIDTH) - flappy->offset,
            flappy->ceils[w],
            ((w + 1) * CHUNK_WIDTH) - flappy->offset,
            flappy->ceils[w + 1],
            WHITE);
    }

    // // Draw a bar graph
    // uint8_t numBins = sizeof(folded_bins) / sizeof(folded_bins[0]);
    // uint8_t binWidth = OLED_WIDTH / numBins;
    // uint8_t i;
    // for(i = 0; i < numBins; i++)
    // {
    //     uint8_t height = (16 * folded_bins[i]) / 2048;
    //     fillDisplayArea(i * binWidth, OLED_HEIGHT - height, (i + 1) * binWidth, OLED_HEIGHT, WHITE);
    // }
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
    syncedTimerArm(&(flappy->updateTimer), 50, true);
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
    // TODO return if not in game mode

    // os_printf("%s %d\n", __func__, samp);
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
