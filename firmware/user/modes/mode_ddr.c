/*
 * mode_ddr.c
 *
 *  Created on: May 13, 2019
 *      Author: rick
 */
#include "user_config.h"

/*============================================================================
 * Includes
 *==========================================================================*/

#include <osapi.h>
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

#include "assets.h"
#include "synced_timer.h"

/*============================================================================
 * Defines
 *==========================================================================*/

#define BTN_CTR_X 96
#define BTN_CTR_Y 40
#define BTN_RAD    8
#define BTN_OFF   12

#define ARROW_ROW_MAX_COUNT 16
#define ARROW_PERFECT_HPOS 1650
#define ARROW_PERFECT_RADIUS 30
#define ARROW_HIT_RADIUS 60

#define ARROWS_TIMER 15
#define MAX_SIXTEENTH_TIMER (60000 / 4 / ARROWS_TIMER)

#define LEDS_TIMER 15
#define MAX_PULSE_TIMER (60000 / LEDS_TIMER)

#define FEEDBACK_PERFECT 3
#define FEEDBACK_HIT 2
#define FEEDBACK_MISS 1
#define FEEDBACK_NONE 0

#define MAX_FEEDBACK_TIMER 250

/*============================================================================
 * Prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR ddrEnterMode(void);
void ICACHE_FLASH_ATTR ddrExitMode(void);
void ICACHE_FLASH_ATTR ddrButtonCallback(uint8_t state __attribute__((unused)),
        int button, int down);
void ICACHE_FLASH_ATTR ddrAccelerometerHandler(accel_t* accel);

static void ICACHE_FLASH_ATTR ddrUpdateDisplay(void* arg __attribute__((unused)));
static void ICACHE_FLASH_ATTR ddrRotateBanana(void* arg __attribute__((unused)));
static void ICACHE_FLASH_ATTR ddrLedFunc(void* arg __attribute__((unused)));
static void ICACHE_FLASH_ATTR ddrHandleArrows(void* arg __attribute__((unused)));
//static void ICACHE_FLASH_ATTR ddrAnimateSprite(void* arg __attribute__((unused)));
//static void ICACHE_FLASH_ATTR ddrUpdateButtons(void* arg __attribute__((unused)));

void ddrUpdateButtons();
void ddrHandleHit();
void ddrHandlePerfect();
void ddrHandleMiss();

/*============================================================================
 * Const data
 *==========================================================================*/

const sprite_t ddr_rotating_banana[] ICACHE_RODATA_ATTR =
{
    // frame_0_delay-0.07s.png
    {
        .width = 16,
        .height = 16,
        .data =
        {
            0b0000000010000000,
            0b0000000110000000,
            0b0000000011000000,
            0b0000000010100000,
            0b0000000010010000,
            0b0000000100010000,
            0b0000000100001000,
            0b0000000100001000,
            0b0000000100001000,
            0b0000001000001000,
            0b0000010000001000,
            0b0001100000010000,
            0b0110000000100000,
            0b0100000011000000,
            0b0110011100000000,
            0b0001100000000000,
        }
    },
    // frame_1_delay-0.07s.png
    {
        .width = 16,
        .height = 16,
        .data =
        {
            0b0000000100000000,
            0b0000000110000000,
            0b0000000110000000,
            0b0000000110000000,
            0b0000001001000000,
            0b0000001001000000,
            0b0000001000100000,
            0b0000001000100000,
            0b0000010000100000,
            0b0000010000100000,
            0b0000010000100000,
            0b0000100000100000,
            0b0001000001000000,
            0b0001000011000000,
            0b0000111100000000,
            0b0000000000000000,
        }
    },
    // frame_2_delay-0.07s.png
    {
        .width = 16,
        .height = 16,
        .data =
        {
            0b0000000100000000,
            0b0000001100000000,
            0b0000001100000000,
            0b0000010100000000,
            0b0000010100000000,
            0b0000100100000000,
            0b0000100010000000,
            0b0001000010000000,
            0b0001000010000000,
            0b0001000010000000,
            0b0001000001000000,
            0b0000100001000000,
            0b0000100001000000,
            0b0000011001000000,
            0b0000000110000000,
            0b0000000000000000,
        }
    },
    // frame_3_delay-0.07s.png
    {
        .width = 16,
        .height = 16,
        .data =
        {
            0b0000001100000000,
            0b0000001100000000,
            0b0000011000000000,
            0b0000101000000000,
            0b0001001000000000,
            0b0010001000000000,
            0b0010000100000000,
            0b0010000100000000,
            0b0010000100000000,
            0b0010000010000000,
            0b0001000001000000,
            0b0001000000110000,
            0b0000100000001000,
            0b0000011000000100,
            0b0000000111111000,
            0b0000000000000000,
        }
    },
    // frame_4_delay-0.07s.png
    {
        .width = 16,
        .height = 16,
        .data =
        {
            0b0000001100000000,
            0b0000001100000000,
            0b0000011000000000,
            0b0000101000000000,
            0b0001001100000000,
            0b0010000100000000,
            0b0010000100000000,
            0b0010000100000000,
            0b0010000010000000,
            0b0010000010000000,
            0b0001000001000000,
            0b0001000000100000,
            0b0000100000011000,
            0b0000010000000100,
            0b0000001100000100,
            0b0000000011111100,
        }
    },
    // frame_5_delay-0.07s.png
    {
        .width = 16,
        .height = 16,
        .data =
        {
            0b0000000100000000,
            0b0000001100000000,
            0b0000001100000000,
            0b0000010010000000,
            0b0000010010000000,
            0b0000100010000000,
            0b0000100001000000,
            0b0000100001000000,
            0b0000100001000000,
            0b0000100001000000,
            0b0000100000100000,
            0b0000010000100000,
            0b0000010000100000,
            0b0000001000010000,
            0b0000001000010000,
            0b0000000111110000,
        }
    },
    // frame_6_delay-0.07s.png
    {
        .width = 16,
        .height = 16,
        .data =
        {
            0b0000000100000000,
            0b0000000110000000,
            0b0000000101000000,
            0b0000000100100000,
            0b0000000100100000,
            0b0000001000100000,
            0b0000001000010000,
            0b0000001000010000,
            0b0000001000010000,
            0b0000010000010000,
            0b0000010000100000,
            0b0000010000100000,
            0b0000010000100000,
            0b0000010000100000,
            0b0000100001000000,
            0b0000111110000000,
        }
    },
    // frame_7_delay-0.07s.png
    {
        .width = 16,
        .height = 16,
        .data =
        {
            0b0000000110000000,
            0b0000000011000000,
            0b0000000010100000,
            0b0000000010100000,
            0b0000000010010000,
            0b0000000100001000,
            0b0000000100001000,
            0b0000000100001000,
            0b0000000100001000,
            0b0000001000001000,
            0b0000010000010000,
            0b0000100000010000,
            0b0001000000100000,
            0b0010000001000000,
            0b0100000010000000,
            0b0111111100000000,
        }
    },
};

const char ddrSprites[][16] =
{
    "baby_dino.png",
    "blacky.png",
    "coldhead.png",
    "dino.png",
    "horn_dino.png",
    "serpent.png",
    "turd.png",
    "bear.png",
    "blob.png",
    "crouch.png",
    "dragon.png",
    "hothead.png",
    "slug.png",
    "wing_snake.png"
};

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
    .fnAccelerometerCallback = ddrAccelerometerHandler
};

typedef struct 
{
    uint16_t hPos;
} ddrArrow;

typedef struct 
{
    ddrArrow arrows[ARROW_ROW_MAX_COUNT];
    uint8 start;
    uint8 count;
    int pressDirection;
} ddrArrowRow;


struct
{
    // Callback variables
    accel_t Accel;
    uint8_t ButtonState;
    uint8_t ButtonDownState;

    // Timer variables
    syncedTimer_t TimerHandleLeds;
    syncedTimer_t TimerHandleArrows;
    syncedTimer_t timerHandleBanana;
    syncedTimer_t timerUpdateDisplay;

    uint8_t BananaIdx;

    ddrArrowRow arrowRows[4];
    uint16_t tempo;
    uint16_t maxPressForgiveness;
    uint8_t sixteenths;
    uint16_t sixteenthNoteCounter;

    uint16_t rotation;
    gifHandle gHandle;

    uint16_t PulseTimeLeft;

    uint8_t feedbackTimer;
    uint8_t currentFeedback;
} ddr;

/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * Initializer for ddr
 */
void ICACHE_FLASH_ATTR ddrEnterMode(void)
{
    enableDebounce(false);

    // Clear everything
    ets_memset(&ddr, 0, sizeof(ddr));

    // Test the buzzer
    // uint32_t songLen;
    // startBuzzerSong((song_t*)getAsset("carmen.rtl", &songLen), false);

    // Test the display with a rotating banana
    syncedTimerDisarm(&ddr.timerHandleBanana);
    syncedTimerSetFn(&ddr.timerHandleBanana, ddrRotateBanana, NULL);
    syncedTimerArm(&ddr.timerHandleBanana, 30, true);

    syncedTimerDisarm(&ddr.TimerHandleArrows);
    syncedTimerSetFn(&ddr.TimerHandleArrows, ddrHandleArrows, NULL);
    syncedTimerArm(&ddr.TimerHandleArrows, 15, true);

    syncedTimerDisarm(&ddr.timerUpdateDisplay);
    syncedTimerSetFn(&ddr.timerUpdateDisplay, ddrUpdateDisplay, NULL);
    syncedTimerArm(&ddr.timerUpdateDisplay, 15, true);

    // Test the LEDs
    syncedTimerDisarm(&ddr.TimerHandleLeds);
    syncedTimerSetFn(&ddr.TimerHandleLeds, ddrLedFunc, NULL);
    syncedTimerArm(&ddr.TimerHandleLeds, LEDS_TIMER, true);

    // Draw a gif
    //drawGifFromAsset("ragequit.gif", 0, 0, false, false, 0, &ddr.gHandle);

    // reset arrows
    for (int i = 0; i < 4; i++)
    {
        ddr.arrowRows[i].count = 0;
        ddr.arrowRows[i].start = 0;
    }

    ddr.arrowRows[0].pressDirection = DOWN;
    ddr.arrowRows[1].pressDirection = RIGHT;
    ddr.arrowRows[2].pressDirection = LEFT;
    ddr.arrowRows[3].pressDirection = UP;

    ddr.arrowRows[0].count=1;
    ddr.arrowRows[0].arrows[0].hPos = 400;

    ddr.arrowRows[2].count=1;
    ddr.arrowRows[2].arrows[0].hPos = 400;
    ddr.arrowRows[3].count=1;
    ddr.arrowRows[3].arrows[0].hPos = 0;

    ddr.tempo = 110;
    ddr.sixteenths = 6;
    ddr.sixteenthNoteCounter = MAX_SIXTEENTH_TIMER;

    ddr.ButtonDownState = 0;

    ddr.PulseTimeLeft = MAX_PULSE_TIMER;

    ddr.currentFeedback = 0;
    ddr.feedbackTimer = 0;
}

/**
 * Called when ddr is exited
 */
void ICACHE_FLASH_ATTR ddrExitMode(void)
{
    stopBuzzerSong();
    syncedTimerDisarm(&ddr.timerHandleBanana);
    syncedTimerDisarm(&ddr.TimerHandleLeds);
    syncedTimerDisarm(&ddr.TimerHandleArrows);
    syncedTimerDisarm(&ddr.timerUpdateDisplay);
    syncedTimerDisarm(&ddr.TimerHandleLeds);
}

/**
 * @brief called on a timer, this blinks an LED pattern
 *
 * @param arg
 */
static void ICACHE_FLASH_ATTR ddrLedFunc(void* arg __attribute__((unused)))
{
    led_t leds[NUM_LIN_LEDS] = {{0}};

    if (ddr.currentFeedback)
    {
        if (LEDS_TIMER > ddr.feedbackTimer)
        {
            ddr.feedbackTimer = MAX_FEEDBACK_TIMER;
            ddr.currentFeedback = FEEDBACK_NONE;
        } 
        else
        {
            ddr.feedbackTimer -= LEDS_TIMER;
            switch(ddr.currentFeedback)
            {
                case FEEDBACK_PERFECT:
                    leds[2].g=250;
                    leds[3].g=250;
                    break;
                    
                case FEEDBACK_HIT:
                    leds[2].g=100;
                    leds[2].r=50;
                    leds[3].g=100;
                    leds[3].r=50;
                    break;

                default:// FEEDBACK_MISS:
                    leds[2].r=50;
                    leds[3].r=50;
            }
        }
        
    }

    uint16_t pulseTimeReduction = ddr.tempo;
    if (pulseTimeReduction > ddr.PulseTimeLeft)
    {
        ddr.PulseTimeLeft = MAX_PULSE_TIMER - pulseTimeReduction + ddr.PulseTimeLeft;
    }
    else 
    {
        ddr.PulseTimeLeft -= pulseTimeReduction;
        if (ddr.PulseTimeLeft < 1000)
        {
            leds[0].b=128;
            leds[1].b=128;
        
            leds[NUM_LIN_LEDS-1].b=128;
            leds[NUM_LIN_LEDS-2].b=128;
        }
    } 

    setLeds(leds, sizeof(leds));
}

/**
 * @brief Called on a timer, this rotates the banana by picking the next sprite
 *
 * @param arg unused
 */
static void ICACHE_FLASH_ATTR ddrHandleArrows(void* arg __attribute__((unused)))
{
    ddrArrowRow* curRow;
    ddrArrow* curArrow;
    int curStart;
    int curCount;
    int curEnd;

    bool canSpawnArrow = false;
    int percentChanceSpawn = 0;

    if (ddr.tempo > ddr.sixteenthNoteCounter) 
    {
        ddr.sixteenthNoteCounter = MAX_SIXTEENTH_TIMER - ddr.tempo + ddr.sixteenthNoteCounter;
        ddr.sixteenths = (ddr.sixteenths + 1 ) % 16;

        canSpawnArrow = true;
        
        if (0 == ddr.sixteenths)
        {
            percentChanceSpawn = 30; // 30 percent chance on first beat
        }
         else if ( 8 == ddr.sixteenths)
        {
            percentChanceSpawn = 20; // 20 percent chance on 3rd beat
        }
        else if ( 0 == ddr.sixteenths % 4)
        {
            percentChanceSpawn = 10; // 10 percent chance on 2nd/4th beat
        }
        else if ( 0 == ddr.sixteenths % 2)
        {
            percentChanceSpawn = 5; // 5 percent chance on half beats
        }
        else 
        {
            percentChanceSpawn = 0; // 0 percent chance on other 16th beats
        }
        
    }
    else 
    {
        ddr.sixteenthNoteCounter -= ddr.tempo;
    }

    for (int rowIdx = 0; rowIdx < 4; rowIdx++)
    {
        curRow = &(ddr.arrowRows[rowIdx]);
        curStart = curRow->start;
        curCount = curRow->count;
        curEnd = (curStart + curCount) % ARROW_ROW_MAX_COUNT;

        for(int arrowIdx = curStart; arrowIdx != curEnd; arrowIdx = (arrowIdx + 1) % ARROW_ROW_MAX_COUNT)
        {
            curArrow = &(curRow->arrows[arrowIdx]);
            curArrow->hPos += ddr.tempo * 0.07;

            uint16_t arrowDist = abs(curArrow->hPos - ARROW_PERFECT_HPOS);

            if (arrowDist <= ARROW_PERFECT_RADIUS)
            {
                if(ddr.ButtonDownState & curRow->pressDirection)
                { //assumes that no more than one arrow per row can be in hit zone at a time
                    curRow->count--;
                    curRow->start = (curRow->start + 1) % ARROW_ROW_MAX_COUNT;
                    
                    // reset down state
                    ddr.ButtonDownState = ddr.ButtonDownState & ~curRow->pressDirection;
                    ddrHandlePerfect();
                }
            }
            else if (arrowDist <= ARROW_HIT_RADIUS)
            {
                if(ddr.ButtonDownState & curRow->pressDirection)
                { //assumes that no more than one arrow per row can be in hit zone at a time
                    curRow->count--;
                    curRow->start = (curRow->start + 1) % ARROW_ROW_MAX_COUNT;
                    
                    // reset down state
                    ddr.ButtonDownState = ddr.ButtonDownState & ~curRow->pressDirection;
                    ddrHandleHit();
                }
            }
            else if (curArrow->hPos > ARROW_PERFECT_HPOS)
            {
                curRow->count--;
                curRow->start = (curRow->start + 1) % ARROW_ROW_MAX_COUNT;
                ddrHandleMiss();
            }
            
        }

        if (canSpawnArrow)
        {
            if (rand() % 100 < percentChanceSpawn)
            {
                curRow->count++;
                curRow->arrows[(curRow->start+curRow->count) % ARROW_ROW_MAX_COUNT].hPos = 0;
            }
        }
    }
}

/**
 * @brief Called on a timer, this rotates the banana by picking the next sprite
 *
 * @param arg unused
 */
static void ICACHE_FLASH_ATTR ddrAnimateSprite(void* arg __attribute__((unused)))
{
    // ddr.rotation = (ddr.rotation + 90) % 360;
    ddr.rotation = (ddr.rotation + 3) % 360;

    //ddrUpdateDisplay();

    ddr.gHandle.rotateDeg = ddr.rotation;
}

/**
 * @brief Called on a timer, this rotates the banana by picking the next sprite
 *
 * @param arg unused
 */
static void ICACHE_FLASH_ATTR ddrRotateBanana(void* arg __attribute__((unused)))
{
    ddr.BananaIdx = (ddr.BananaIdx + 1) % (sizeof(ddr_rotating_banana) / sizeof(ddr_rotating_banana[0]));
    // testUpdateDisplay();
}

void ddrUpdateDownButtonState(uint8_t mask)
{
    uint8_t state = ddr.ButtonState & mask;
    uint8_t startDownState = ddr.ButtonDownState & mask;

    if (state || startDownState)
    {
        ddr.ButtonDownState = (ddr.ButtonDownState & ~mask) + (state & ~startDownState);
    }
}

void ddrUpdateButtons()
{
    ddrUpdateDownButtonState(LEFT);
    ddrUpdateDownButtonState(DOWN);
    ddrUpdateDownButtonState(UP);
    ddrUpdateDownButtonState(RIGHT);
}

/**
 * TODO
 */
static void ICACHE_FLASH_ATTR ddrUpdateDisplay(void* arg __attribute__((unused)))
{
    // Clear the display
    clearDisplay();

    // Draw a title
    //plotText(0, 0, "DDR MODE", RADIOSTARS, WHITE);

    // Draw the banana
    //plotSprite(50, 40, &ddr_rotating_banana[ddr.BananaIdx], WHITE);
    //plotSprite(70, 40, &ddr_rotating_banana[(ddr.BananaIdx + 5) % (sizeof(ddr_rotating_banana) / sizeof(ddr_rotating_banana[0]))], WHITE);
    //plotSprite(30, 40, &ddr_rotating_banana[(ddr.BananaIdx + 10) % (sizeof(ddr_rotating_banana) / sizeof(ddr_rotating_banana[0]))], WHITE);

    //ddrUpdateButtons();
    ddrArrowRow* curRow;
    ddrArrow* curArrow;
    int curStart;
    int curCount;
    int curEnd;

    for (int rowIdx = 0; rowIdx < 4; rowIdx++)
    {
        curRow = &(ddr.arrowRows[rowIdx]);
        curStart = curRow->start;
        curCount = curRow->count;
        curEnd = (curStart + curCount) % ARROW_ROW_MAX_COUNT;

        for(int arrowIdx = curStart; arrowIdx != curEnd; arrowIdx = (arrowIdx + 1) % ARROW_ROW_MAX_COUNT)
        {
            curArrow = &(curRow->arrows[arrowIdx]);
            plotSprite((curArrow->hPos-400)/12, 48 - rowIdx * 16, &ddr_rotating_banana[ddr.BananaIdx], WHITE);
        }
    }
    
    plotCircle(110, 55-16*3, BTN_RAD-3, WHITE);
    plotCircle(110, 55-16*2, BTN_RAD-3, WHITE);
    plotCircle(110, 55-16*1, BTN_RAD-3, WHITE);
    plotCircle(110, 55-16*0, BTN_RAD-3, WHITE);

    if(ddr.ButtonDownState & UP)
    {
        // A
        plotCircle(110, 55-16*3, BTN_RAD, WHITE);
    }

    if(ddr.ButtonDownState & LEFT)
    {
        // S
        plotCircle(110, 55-16*2, BTN_RAD, WHITE);
    }
    
    if(ddr.ButtonDownState & RIGHT)
    {
        // D
        plotCircle(110, 55-16*1, BTN_RAD, WHITE);
    }

    if(ddr.ButtonDownState & DOWN)
    {
        // F
        plotCircle(110, 55-16*0, BTN_RAD, WHITE);
    }


    ddr.ButtonDownState = 0;
}


void ddrHandleHit()
{
    ddr.feedbackTimer = MAX_FEEDBACK_TIMER;
    ddr.currentFeedback = FEEDBACK_HIT;
}

void ddrHandlePerfect()
{
    ddr.feedbackTimer = MAX_FEEDBACK_TIMER;
    ddr.currentFeedback = FEEDBACK_PERFECT;
}

void ddrHandleMiss(){
    ddr.feedbackTimer = MAX_FEEDBACK_TIMER;
    ddr.currentFeedback = FEEDBACK_MISS;
}

/**
 * TODO
 *
 * @param state  A bitmask of all button states, unused
 * @param button The button which triggered this event
 * @param down   true if the button was pressed, false if it was released
 */
void ICACHE_FLASH_ATTR ddrButtonCallback( uint8_t state,
        int button, int down)
{
    ddr.ButtonState = state;
    ddr.ButtonDownState = (ddr.ButtonDownState & ~(1<<button)) + (down << button);
}

/**
 * Store the acceleration data to be displayed later
 *
 * @param x
 * @param x
 * @param z
 */
void ICACHE_FLASH_ATTR ddrAccelerometerHandler(accel_t* accel)
{
    ddr.Accel.x = accel->x;
    ddr.Accel.y = accel->y;
    ddr.Accel.z = accel->z;
    // ddrUpdateDisplay();
}
