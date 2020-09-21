/*
 * mode_ddr->c
 *
 *  Created on: May 13, 2019
 *      Author: rick
 */
#include "user_config.h"

/*============================================================================
 * Includes
 *==========================================================================*/

#include <osapi.h>
#include <mem.h>
#include <stdlib.h>

#include "user_main.h"
#include "mode_ddr.h"
#include "hsv_utils.h"
#include "oled.h"
#include "sprite.h"
#include "font.h"
#include "bresenham.h"
#include "buttons.h"
#include "hpatimer.h"
#include "menu2d.h"

#include "assets.h"
#include "synced_timer.h"

/*============================================================================
 * Defines
 *==========================================================================*/

#define BTN_RAD    8

#define ARROW_ROW_MAX_COUNT 16
#define ARROW_PERFECT_HPOS 1650
#define ARROW_PERFECT_RADIUS 35
#define ARROW_HIT_RADIUS 80

#define ARROWS_TIMER 15
#define MAX_SIXTEENTH_TIMER (60000 / 4 / ARROWS_TIMER)

#define LEDS_TIMER ARROWS_TIMER
#define MAX_PULSE_TIMER (60000 / LEDS_TIMER)
#define ARROW_SPACING_FACTOR 0.12
#define START_PULSE_TIMER 3000

#define SONG_DURATION 1000 * 60

#define FEEDBACK_HIT_LATE 4
#define FEEDBACK_PERFECT 3
#define FEEDBACK_HIT_EARLY 2
#define FEEDBACK_MISS 1
#define FEEDBACK_NONE 0

#define MAX_FEEDBACK_TIMER 250

//#define DEBUG

#ifdef DEBUG
    #define DEBUG_QUARTER_NOTES
    #define DEBUG_EVERY_PERFECT
#endif

/*============================================================================
 * Prototypes
 *==========================================================================*/

static void ICACHE_FLASH_ATTR ddrEnterMode(void);
static void ICACHE_FLASH_ATTR ddrExitMode(void);
static void ICACHE_FLASH_ATTR ddrButtonCallback(uint8_t state __attribute__((unused)),
        int button, int down);

static void ICACHE_FLASH_ATTR ddrUpdateDisplay(void* arg __attribute__((unused)));
static void ICACHE_FLASH_ATTR ddrAnimateNotes(void* arg __attribute__((unused)));
static void ICACHE_FLASH_ATTR ddrLedFunc(void* arg __attribute__((unused)));
static void ICACHE_FLASH_ATTR ddrHandleArrows(void);
static void ICACHE_FLASH_ATTR ddrAnimateSuccessMeter(void* arg __attribute((unused)));
static void ICACHE_FLASH_ATTR ddrSongDurationFunc(void* arg __attribute((unused)));
//static void ICACHE_FLASH_ATTR ddrAnimateSprite(void* arg __attribute__((unused)));
//static void ICACHE_FLASH_ATTR ddrUpdateButtons(void* arg __attribute__((unused)));

static void ICACHE_FLASH_ATTR ddrMenuCb(const char* menuItem);

static void ICACHE_FLASH_ATTR fisherYates(int arr[], int n);
static void ICACHE_FLASH_ATTR ddrHandleHitEarly(void);
static void ICACHE_FLASH_ATTR ddrHandleHitLate(void);
static void ICACHE_FLASH_ATTR ddrHandlePerfect(void);
static void ICACHE_FLASH_ATTR ddrHandleMiss(void);
static void ICACHE_FLASH_ATTR ddrCheckSongEnd(void);
static void ICACHE_FLASH_ATTR ddrGameOver(void);
static void ICACHE_FLASH_ATTR ddrStartGame(int tempo, float eighthNoteProbabilityModifier,
        int restAvoidanceProbability);

/*============================================================================
 * Const data
 *==========================================================================*/


/*============================================================================
 * Variables
 *==========================================================================*/

swadgeMode ddrMode =
{
    .modeName = "ddr",
    .fnEnterMode = ddrEnterMode,
    .fnExitMode = ddrExitMode,
    .fnButtonCallback = ddrButtonCallback,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = NULL,
    .fnAudioCallback = NULL,
    .menuImg = "ddr-menu.gif"
};


typedef struct
{
    uint16_t arrows[ARROW_ROW_MAX_COUNT];
    uint8 start;
    uint8 count;
    int pressDirection;
} ddrArrowRow;

typedef enum
{
    DDR_MENU,
    DDR_GAME,
    DDR_SCORE
} ddrGameMode;

typedef struct
{
    // Callback variables
    uint8_t ButtonState;
    uint8_t ButtonDownState;

    // Timer variables
    timer_t TimerHandleLeds;
    timer_t TimerHandleArrows;
    timer_t timerAnimateNotes;
    timer_t timerUpdateDisplay;
    timer_t TimerAnimateSuccessMeter;
    timer_t TimerSongDuration;

    uint8_t NoteIdx;

    ddrArrowRow arrowRows[4];
    uint16_t tempo;
    uint16_t maxPressForgiveness;
    uint8_t sixteenths;
    uint16_t sixteenthNoteCounter;

    float eighthNoteProbabilityModifier;
    int restAvoidanceProbability;

    uint16_t PulseTimeLeft;

    uint8_t feedbackTimer;
    uint8_t currentFeedback;

    uint8_t successMeter;
    uint8_t successMeterShineStart;

    // Stats
    uint8_t perfects;
    uint8_t okays;
    uint8_t misses;

    uint8_t isSongOver;
    uint8_t doDisplayEndScreen;
    uint8_t didLose;

    ddrGameMode mode;

    menu_t* menu;

    pngSequenceHandle ddrSkullSequenceHandle;
} ddr_t;

ddr_t* ddr;

static const char ddr_title[]  = "StomP";
static const char ddr_easy[]   = "EASY";
static const char ddr_medium[] = "MED";
static const char ddr_hard[]   = "HARD";
static const char ddr_quit[]   = "QUIT";


/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * Initializer for ddr
 */
void ICACHE_FLASH_ATTR ddrEnterMode(void)
{
    enableDebounce(false);
    ddr = os_malloc(sizeof(ddr_t));
    ets_memset(ddr, 0, sizeof(ddr_t));

    allocPngSequence(&ddr->ddrSkullSequenceHandle, 4,
                     "skull01.png",
                     "skull02.png",
                     "skull01.png",
                     "skull03.png");

    ddr->mode = DDR_MENU;

    ddr->menu = initMenu(ddr_title, ddrMenuCb);
    addRowToMenu(ddr->menu);
    addItemToRow(ddr->menu, ddr_medium);
    addItemToRow(ddr->menu, ddr_hard);
    addItemToRow(ddr->menu, ddr_easy);
    addRowToMenu(ddr->menu);
    addItemToRow(ddr->menu, ddr_quit);
    drawMenu(ddr->menu);

    timerDisarm(&ddr->timerUpdateDisplay);
    timerSetFn(&ddr->timerUpdateDisplay, ddrUpdateDisplay, NULL);
    timerArm(&ddr->timerUpdateDisplay, 15, true);

    timerDisarm(&ddr->timerAnimateNotes);
    timerSetFn(&ddr->timerAnimateNotes, ddrAnimateNotes, NULL);
    timerArm(&ddr->timerAnimateNotes, 60, true);

    timerDisarm(&ddr->TimerHandleLeds);
    timerSetFn(&ddr->TimerHandleLeds, ddrLedFunc, NULL);
    timerArm(&ddr->TimerHandleLeds, LEDS_TIMER, true);

    timerDisarm(&ddr->TimerAnimateSuccessMeter);
    timerSetFn(&ddr->TimerAnimateSuccessMeter, ddrAnimateSuccessMeter, NULL);
    timerArm(&ddr->TimerAnimateSuccessMeter, 40, true);

    timerDisarm(&ddr->TimerSongDuration);
    timerSetFn(&ddr->TimerSongDuration, ddrSongDurationFunc, NULL);
    timerArm(&ddr->TimerSongDuration, SONG_DURATION, true);
}

static void ICACHE_FLASH_ATTR ddrMenuCb(const char* menuItem)
{
    if(ddr_easy == menuItem)
    {
        ddrStartGame(80, 0.8f, 0);
    }
    else if (ddr_medium == menuItem)
    {
        ddrStartGame(85, 2.f, 10);
    }
    else if (ddr_hard == menuItem)
    {
        ddrStartGame(90, 3.f, 40);
    }

    else if (ddr_quit == menuItem)
    {
        switchToSwadgeMode(0);
    }
}

static void ICACHE_FLASH_ATTR ddrStartGame(int tempo, float eighthNoteProbabilityModifier, int restAvoidanceProbability)
{

    // Draw a gif
    //drawGifFromAsset("ragequit.gif", 0, 0, false, false, 0, &ddr->gHandle);

    // reset arrows
    for (int i = 0; i < 4; i++)
    {
        ddr->arrowRows[i].count = 0;
        ddr->arrowRows[i].start = 0;
    }

    ddr->arrowRows[0].pressDirection = LEFT; // bottommost
    ddr->arrowRows[1].pressDirection = RIGHT;
    ddr->arrowRows[2].pressDirection = UP;
    ddr->arrowRows[3].pressDirection = DOWN; // topmost

    ddr->tempo = tempo;
    ddr->eighthNoteProbabilityModifier = eighthNoteProbabilityModifier;
    ddr->restAvoidanceProbability = restAvoidanceProbability;

    ddr->sixteenths = 6;
    ddr->sixteenthNoteCounter = MAX_SIXTEENTH_TIMER;

    ddr->ButtonDownState = 0;

    ddr->PulseTimeLeft = START_PULSE_TIMER;

    ddr->currentFeedback = 0;
    ddr->feedbackTimer = 0;

    ddr->successMeter = 80;
    ddr->successMeterShineStart = 0;
    ddr->isSongOver = 0;
    ddr->doDisplayEndScreen = 0;
    ddr->didLose = 0;

    ddr->okays = 0;
    ddr->perfects = 0;
    ddr->misses = 0;

    ddr->mode = DDR_GAME;
}

/**
 * Called when ddr is exited
 */
static void ICACHE_FLASH_ATTR ddrExitMode(void)
{
    timerDisarm(&ddr->timerAnimateNotes);
    timerDisarm(&ddr->TimerHandleLeds);
    timerDisarm(&ddr->TimerHandleArrows);
    timerDisarm(&ddr->TimerAnimateSuccessMeter);
    timerDisarm(&ddr->timerUpdateDisplay);
    timerDisarm(&ddr->TimerSongDuration);
    timerFlush();

    freePngSequence(&ddr->ddrSkullSequenceHandle);
    deinitMenu(ddr->menu);
    os_free(ddr);
}

/**
 * @brief called on a timer, this blinks an LED pattern
 *
 * @param arg
 */
static void ICACHE_FLASH_ATTR ddrLedFunc(void* arg __attribute__((unused)))
{
    led_t leds[NUM_LIN_LEDS] = {{0}};
    switch (ddr->mode)
    {
        default:
        case DDR_MENU:
        {
            break;
        }
        case DDR_GAME:
        {
            if (ddr->currentFeedback)
            {
                if (LEDS_TIMER > ddr->feedbackTimer)
                {
                    ddr->feedbackTimer = MAX_FEEDBACK_TIMER;
                    ddr->currentFeedback = FEEDBACK_NONE;
                }
                else
                {
                    ddr->feedbackTimer -= LEDS_TIMER;
                    switch(ddr->currentFeedback)
                    {
                        case FEEDBACK_PERFECT:
                            leds[2].g = 150;
                            leds[2].b = 50;
                            leds[3].g = 150;
                            leds[3].b = 50;
                            break;

                        case FEEDBACK_HIT_EARLY:
                            leds[2].g = 50;
                            leds[3].g = 50;
                            break;

                        case FEEDBACK_HIT_LATE:
                            leds[2].g = 100;
                            leds[2].r = 150;
                            leds[3].g = 100;
                            leds[3].r = 150;
                            break;

                        default:
                        case FEEDBACK_MISS:
                            leds[2].r = 50;
                            leds[3].r = 50;
                    }
                }
            }

            uint16_t pulseTimeReduction = ddr->tempo;
            if (pulseTimeReduction > ddr->PulseTimeLeft)
            {
                ddr->PulseTimeLeft = MAX_PULSE_TIMER - pulseTimeReduction + ddr->PulseTimeLeft;
            }
            else
            {
                ddr->PulseTimeLeft -= pulseTimeReduction;

                int pulseWindow = 4000;
                float halfWindow = (float)pulseWindow * 0.5f;

                if (ddr->PulseTimeLeft < pulseWindow)
                {
                    float intensity_mod = 1.0 - ((float)abs(ddr->PulseTimeLeft - halfWindow)) / halfWindow;

                    int blue = 128 * (intensity_mod * intensity_mod);

                    leds[0].b = blue;
                    leds[1].b = blue;

                    leds[NUM_LIN_LEDS - 3].b = blue;
                    leds[NUM_LIN_LEDS - 4].b = blue;
                }
            }
            break;
        }
        case DDR_SCORE:
        {
            break;
        }
    }

    setLeds(leds, sizeof(leds));
}

static void ICACHE_FLASH_ATTR fisherYates(int arr[], int n)
{
    for (int i = n - 1; i > 0; i--)
    {
        int j = rand() % (i + 1);
        int temp = arr[i];
        arr[i] = arr[j];
        arr[j] = temp;
    }
}

/**
 * @brief Called on a timer, this moves the arrows and checks for hits/misses
 *
 * @param arg unused
 */
static void ICACHE_FLASH_ATTR ddrHandleArrows(void)
{
    ddrArrowRow* curRow;
    uint16_t* curArrow;
    int curStart;
    int curCount;
    int curEnd;

    bool canSpawnArrow = false;
    int percentChanceSpawn = 0;

    if (ddr->tempo > ddr->sixteenthNoteCounter)
    {
        ddr->sixteenthNoteCounter = MAX_SIXTEENTH_TIMER - ddr->tempo + ddr->sixteenthNoteCounter;
        ddr->sixteenths = (ddr->sixteenths + 1 ) % 16;

#ifdef DEBUG_QUARTER_NOTES
        // 100 percent chance on each beat
        percentChanceSpawn = 100;
        canSpawnArrow = 0 == ddr->sixteenths % 4;
#else
        canSpawnArrow = true;

        if (0 == ddr->sixteenths)
        {
            percentChanceSpawn = 30; // 30 percent chance on first beat
        }
        else if ( 8 == ddr->sixteenths)
        {
            percentChanceSpawn = 25; // 25 percent chance on 3rd beat
        }
        else if ( 0 == ddr->sixteenths % 4)
        {
            percentChanceSpawn = 20; // 20 percent chance on 2nd/4th beat
        }
        else if ( 0 == ddr->sixteenths % 2)
        {
            percentChanceSpawn = 5 * ddr->eighthNoteProbabilityModifier; // ~5 percent chance on half beats
        }
        else
        {
            percentChanceSpawn = 0; // 0 percent chance on other 16th beats
        }
#endif

    }
    else
    {
        ddr->sixteenthNoteCounter -= ddr->tempo;
    }

    // generate arrows for rows in random order to avoid tending to "run out"
    // before row 2 & 3
    int rowIdxs[] = {0, 1, 2, 3};
    fisherYates(rowIdxs, 4);

    int arrowsSpawnedThisBeat = 0;
    for (int i = 0; i < 4; i++)
    {
        int rowIdx = rowIdxs[i];
        curRow = &(ddr->arrowRows[rowIdx]);
        curStart = curRow->start;
        curCount = curRow->count;
        curEnd = (curStart + curCount) % ARROW_ROW_MAX_COUNT;

        for(int arrowIdx = curStart; arrowIdx != curEnd; arrowIdx = (arrowIdx + 1) % ARROW_ROW_MAX_COUNT)
        {
            curArrow = &(curRow->arrows[arrowIdx]);
            *curArrow += ddr->tempo * ARROW_SPACING_FACTOR;

            int16_t arrowDiff = (int) * curArrow - ARROW_PERFECT_HPOS;
            uint16_t arrowDist = abs(arrowDiff);

            if (arrowDist <= ARROW_PERFECT_RADIUS)
            {
#ifdef DEBUG_EVERY_PERFECT
                if(true)
#else
                if(ddr->ButtonDownState & curRow->pressDirection)
#endif
                {
                    curRow->count--;
                    curRow->start = (curRow->start + 1) % ARROW_ROW_MAX_COUNT;

                    // reset down state
                    ddr->ButtonDownState = ddr->ButtonDownState & ~curRow->pressDirection;
                    ddrHandlePerfect();
                }
            }
            else if (arrowDist <= ARROW_HIT_RADIUS)
            {
                if(ddr->ButtonDownState & curRow->pressDirection)
                {
                    curRow->count--;
                    curRow->start = (curRow->start + 1) % ARROW_ROW_MAX_COUNT;

                    // reset down state
                    ddr->ButtonDownState = ddr->ButtonDownState & ~curRow->pressDirection;

                    if (arrowDiff > 0)
                    {
                        ddrHandleHitLate();
                    }
                    else
                    {
                        ddrHandleHitEarly();
                    }
                }
            }
            else if (*curArrow > ARROW_PERFECT_HPOS)
            {
                curRow->count--;
                curRow->start = (curRow->start + 1) % ARROW_ROW_MAX_COUNT;
                ddrHandleMiss();
            }

        }

        if (!ddr->isSongOver && canSpawnArrow && arrowsSpawnedThisBeat < 2)
        {
            bool willSpawn =  rand() % 100 < percentChanceSpawn;

            // if last opportunity to spawn, check if we can actually rest
            if (i == 3 && !willSpawn && arrowsSpawnedThisBeat == 0)
            {
                willSpawn = rand() % 100 < ddr->restAvoidanceProbability;
            }

            if (willSpawn)
            {
                arrowsSpawnedThisBeat++;
                curRow->arrows[(curRow->start + curRow->count) % ARROW_ROW_MAX_COUNT] = 0;
                curRow->count++;
            }
        }
    }
}

/**
 * @brief Called on a timer, this animates the note asset
 *
 * @param arg unused
 */
static void ICACHE_FLASH_ATTR ddrAnimateNotes(void* arg __attribute__((unused)))
{
    ddr->NoteIdx = (ddr->NoteIdx + 1) % 4;
    // testUpdateDisplay();
}

static void ICACHE_FLASH_ATTR ddrUpdateDisplay(void* arg __attribute__((unused)))
{
    switch(ddr->mode)
    {
        default:
        case DDR_MENU:
        {
            drawMenu(ddr->menu);
            break;
        }
        case DDR_SCORE:
        {
            clearDisplay();
            char perfectsText[24];
            char hitsText[24];
            char missesText[24];

            ets_snprintf(perfectsText, sizeof(perfectsText),   "Perfects:    %03d", ddr->perfects);
            ets_snprintf(hitsText, sizeof(perfectsText),       "Okays:       %03d", ddr->okays);
            ets_snprintf(missesText, sizeof(perfectsText),     "Misses:      %03d", ddr->misses);

            if (ddr->didLose)
            {
                plotText(38, 5, "You died", IBM_VGA_8, WHITE);
            }
            else
            {
                plotText(38, 5, "You win!", IBM_VGA_8, WHITE);
            }

            plotText(36, 30, perfectsText, TOM_THUMB, WHITE);
            plotText(36, 40, hitsText, TOM_THUMB, WHITE);
            plotText(36, 50, missesText, TOM_THUMB, WHITE);
            break;
        }
        case DDR_GAME:
        {
            //Game update logic
            ddrHandleArrows();

            // Clear the display
            clearDisplay();

            //ddrUpdateButtons();
            ddrArrowRow* curRow;
            uint16_t* curArrow;
            int curStart;
            int curCount;
            int curEnd;

            for (int rowIdx = 0; rowIdx < 4; rowIdx++)
            {
                int16_t rowRot = 0;
                bool rowHFlip = false;
                switch(rowIdx)
                {
                    default:
                    case 0:
                        rowRot = 90;
                        rowHFlip = false;
                    case 1:
                        break;
                    case 2:
                        rowHFlip = true;
                        break;
                    case 3:
                        rowRot = 270;
                        rowHFlip = true;
                        break;
                }

                curRow = &(ddr->arrowRows[rowIdx]);
                curStart = curRow->start;
                curCount = curRow->count;
                curEnd = (curStart + curCount) % ARROW_ROW_MAX_COUNT;

                for(int arrowIdx = curStart; arrowIdx != curEnd; arrowIdx = (arrowIdx + 1) % ARROW_ROW_MAX_COUNT)
                {
                    curArrow = &(curRow->arrows[arrowIdx]);


                    drawPngSequence(&ddr->ddrSkullSequenceHandle, (*curArrow - 400) / 12, 48 - rowIdx * 16, rowHFlip, false, rowRot,
                                    ddr->NoteIdx);
                }
            }

            plotCircle(110, 55 - 16 * 3, BTN_RAD - 3, WHITE); // Top
            plotCircle(110, 55 - 16 * 3 - 3, 2, WHITE); // subcircle

            plotCircle(110, 55 - 16 * 2, BTN_RAD - 3, WHITE);
            plotCircle(110 - 3, 55 - 16 * 2, 2, WHITE); // subcircle

            plotCircle(110, 55 - 16 * 1, BTN_RAD - 3, WHITE);
            plotCircle(110 + 3, 55 - 16 * 1, 2, WHITE); // subcircle

            plotCircle(110, 55 - 16 * 0, BTN_RAD - 3, WHITE); // Bottom
            plotCircle(110, 55 - 16 * 0 + 3, 2, WHITE); // subcircle

            // press feedback
            for (int rowNum = 0 ; rowNum < 4 ; rowNum++)
            {
                if(ddr->ButtonDownState & ddr->arrowRows[rowNum].pressDirection)
                {
                    plotCircle(110, 55 - 16 * rowNum, BTN_RAD, WHITE);
                }
            }

            if ( ddr->successMeter == 100 )
            {
                plotLine(OLED_WIDTH - 1, 0, OLED_WIDTH - 1, OLED_HEIGHT - 1, WHITE);
                for (int y = OLED_HEIGHT - 1 - ddr->successMeterShineStart; y > 1 ; y -= 5 )
                {
                    drawPixel(OLED_WIDTH - 1, y, BLACK);
                    drawPixel(OLED_WIDTH - 1, y - 1, BLACK);
                }
            }
            else
            {
                plotLine(OLED_WIDTH - 1, (OLED_HEIGHT - 1) - (float)ddr->successMeter / 100.0f * (OLED_HEIGHT - 1), OLED_WIDTH - 1,
                         OLED_HEIGHT - 1, WHITE);
            }

            ddr->ButtonDownState = 0;
            break;
        }
    }

}


static void ICACHE_FLASH_ATTR ddrHandleHitEarly(void)
{
    ddr->okays += 1;
    ddr->feedbackTimer = MAX_FEEDBACK_TIMER;
    ddr->currentFeedback = FEEDBACK_HIT_EARLY;

    if (ddr->successMeter >= 99)
    {
        ddr->successMeter = 100;
    }
    else
    {
        ddr->successMeter += 1;
    }

    ddrCheckSongEnd();
}

static void ICACHE_FLASH_ATTR ddrHandleHitLate(void)
{
    ddr->okays += 1;
    ddr->feedbackTimer = MAX_FEEDBACK_TIMER;
    ddr->currentFeedback = FEEDBACK_HIT_LATE;

    if (ddr->successMeter >= 99)
    {
        ddr->successMeter = 100;
    }
    else
    {
        ddr->successMeter += 1;
    }

    ddrCheckSongEnd();
}

static void ICACHE_FLASH_ATTR ddrHandlePerfect(void)
{
    ddr->perfects += 1;
    ddr->feedbackTimer = MAX_FEEDBACK_TIMER;
    ddr->currentFeedback = FEEDBACK_PERFECT;

    if (ddr->successMeter >= 97)
    {
        ddr->successMeter = 100;
    }
    else
    {
        ddr->successMeter += 3;
    }

    ddrCheckSongEnd();
}

static void ICACHE_FLASH_ATTR ddrHandleMiss(void)
{
    ddr->misses += 1;
    ddr->feedbackTimer = MAX_FEEDBACK_TIMER;
    ddr->currentFeedback = FEEDBACK_MISS;

    if (ddr->successMeter <= 5)
    {
        ddr->successMeter = 0;
        ddrGameOver();
    }
    else
    {
        ddr->successMeter -= 5;
    }

    ddrCheckSongEnd();
}

static void ICACHE_FLASH_ATTR ddrGameOver()
{
    ddr->arrowRows[0].count = 0;
    ddr->arrowRows[1].count = 0;
    ddr->arrowRows[2].count = 0;
    ddr->arrowRows[3].count = 0;
    ddr->isSongOver = true;
    ddr->didLose = true;
}

static void ICACHE_FLASH_ATTR ddrCheckSongEnd()
{
    if (ddr->isSongOver
        && ddr->arrowRows[0].count == 0
        && ddr->arrowRows[1].count == 0
        && ddr->arrowRows[2].count == 0
        && ddr->arrowRows[3].count == 0 )
    {
        ddr->mode = DDR_SCORE;
    }
}


static void ICACHE_FLASH_ATTR ddrSongDurationFunc(void* arg __attribute__((unused)))
{
    ddr->isSongOver = true;
}


/**
 * TODO
 *
 * @param state  A bitmask of all button states, unused
 * @param button The button which triggered this event
 * @param down   true if the button was pressed, false if it was released
 */
static void ICACHE_FLASH_ATTR ddrButtonCallback( uint8_t state,
        int button, int down)
{
    switch(ddr->mode)
    {
        default:
        case DDR_MENU:
        {
            if(down)
            {
                menuButton(ddr->menu, button);
            }
            break;
        }
        case DDR_GAME:
        {
            ddr->ButtonState = state;
            ddr->ButtonDownState = (ddr->ButtonDownState & ~(1 << button)) + (down << button);
            break;
        }

        case DDR_SCORE:
        {
            if (state & 0x10)
            {
                ddr->mode = DDR_MENU;
            }
            break;
        }

    }
}

static void ICACHE_FLASH_ATTR ddrAnimateSuccessMeter(void* arg __attribute((unused)))
{
    ddr->successMeterShineStart = (ddr->successMeterShineStart + 1 ) % 4;
}