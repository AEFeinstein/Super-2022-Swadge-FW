/*
*   mode_color_movement.c
*
*   Created on: 10 Oct 2019
*               Author: bbkiw
*/

//TODO
//Could have buffers for values bigger than 120 so for
// example estimating bpm via tau could go lower than 60/119
// need to modify plotting on OLED to should most recent 120 values
// Use buttons to select options
//     ways to use accel or built in wave forms
//     estimation of bpm or DFT from cc

#include <osapi.h>
#include <user_interface.h>
#include <stdlib.h>
#include "maxtime.h"
#include "user_main.h"  //swadge mode
#include "mode_color_movement.h"
#include "mode_dance.h"
#include "ccconfig.h"
#include "embeddednf.h"
#include "DFT32.h"
#include "buttons.h"
#include "oled.h"       //display functions
#include "font.h"       //draw text
#include "bresenham.h"  //draw shapes
#include "hpatimer.h"   //for sound
#include "linked_list.h" //custom linked list
#include "custom_commands.h" //saving and loading high scores and last scores
#include "math.h"
#include "embeddedout.h"

/*============================================================================
 * Defines
 *==========================================================================*/

//#define CM_DEBUG_PRINT
#ifdef CM_DEBUG_PRINT
    #include <stdlib.h>
    #define CM_printf(...) os_printf(__VA_ARGS__)
#else
    #define CM_printf(...)
#endif


#ifndef max
    #define max(a,b) ((a) > (b) ? (a) : (b))
#endif
#ifndef min
    #define min(a,b) ((a) < (b) ? (a) : (b))
#endif

// controls (title)
#define BTN_TITLE_START_SCORES LEFT
#define BTN_TITLE_START_GAME RIGHT

// controls (game)
#define BTN_GAME_RIGHT RIGHT
#define BTN_GAME_LEFT LEFT

// update task (16 would give 60 fps like ipad, need read accel that fast too?)
#define UPDATE_TIME_MS 16

// time info.
#define MS_TO_US_FACTOR 1000
#define S_TO_MS_FACTOR 1000
#define BPM_SAMPLE_TIME 12000 // in ms
#define BPM_BUF_SIZE (BPM_SAMPLE_TIME / UPDATE_TIME_MS)
//#define US_TO_MS_FACTOR 0.001

#define NUM_DOTS 120
#define SOUND_ON true
#define ALPHA_FAST 0.3
#define ALPHA_SLOW 0.05
//#define ALPHA_CROSS 0.1
#define ALPHA_ACTIVE 0.03
#define CROSS_TOL 0
#define SPECIAL_EFFECT true

// LEDs relation to screen
#define LED_UPPER_LEFT LED_1
#define LED_UPPER_MID LED_2
#define LED_UPPER_RIGHT LED_3
#define LED_LOWER_RIGHT LED_4
#define LED_LOWER_MID LED_5
#define LED_LOWER_LEFT LED_6

#define SCREEN_BORDER 0

// any enums go here.

typedef enum
{
    CM_TITLE,   // title screen
    CM_GAME,    // play the actual game
} cmState_t;

typedef enum
{
    UPPER_LEFT,
    LOWER_LEFT,
    LOWER_RIGHT,
    UPPER_RIGHT
} exitSpot_t;

// Circular buffer used to store last NUM_DOTS of accelerometer
// readings. Need only to insert, read certain values and plot from
// oldest to newest
typedef struct circularBuffers
{
    int16_t* buffer;
    uint16_t insertHeadInd;
    uint16_t removeTailInd;
    uint16_t length;
} circularBuffer_t;


// Title screen info.

// function prototypes go here.
/*============================================================================
 * Prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR cmInit(void);
void ICACHE_FLASH_ATTR cmDeInit(void);
void ICACHE_FLASH_ATTR cmButtonCallback(uint8_t state, int button, int down);
void ICACHE_FLASH_ATTR cmAccelerometerCallback(accel_t* accel);
void ICACHE_FLASH_ATTR cmSampleHandler(int32_t samp);

// game loop functions.
void ICACHE_FLASH_ATTR cmUpdate(void* arg);

// handle inputs.
void ICACHE_FLASH_ATTR cmTitleInput(void);
void ICACHE_FLASH_ATTR cmGameInput(void);

// update any input-unrelated logic.
void ICACHE_FLASH_ATTR cmTitleUpdate(void);
void ICACHE_FLASH_ATTR cmGameUpdate(void);

// draw the frame.
void ICACHE_FLASH_ATTR cmTitleDisplay(void);
void ICACHE_FLASH_ATTR cmGameDisplay(void);

// mode state management.
void ICACHE_FLASH_ATTR cmChangeState(cmState_t newState);

// input checking.
bool ICACHE_FLASH_ATTR cmIsButtonPressed(uint8_t button);
bool ICACHE_FLASH_ATTR cmIsButtonReleased(uint8_t button);
bool ICACHE_FLASH_ATTR cmIsButtonDown(uint8_t button);
bool ICACHE_FLASH_ATTR cmIsButtonUp(uint8_t button);

// drawing functions.
static void ICACHE_FLASH_ATTR plotCenteredText(uint8_t x0, uint8_t y, uint8_t x1, char* text, fonts font, color col);
static uint8_t getTextWidth(char* text, fonts font);

// Additional Helper
void ICACHE_FLASH_ATTR setCMLeds(led_t* ledData, uint8_t ledDataLen);
void ICACHE_FLASH_ATTR cmChangeLevel(void);

void ICACHE_FLASH_ATTR cmNewSetup(void);

void ICACHE_FLASH_ATTR initCircularBuffer(circularBuffer_t* cirbuff,  int16_t* buffer, uint16_t length);
int16_t ICACHE_FLASH_ATTR getCircularBufferAtIndex(circularBuffer_t cirbuff,  int16_t index);
void ICACHE_FLASH_ATTR circularPush(int16_t value, circularBuffer_t* cirbuff);
int32_t ICACHE_FLASH_ATTR sumOfBuffer(circularBuffer_t cbuf);
int16_t ICACHE_FLASH_ATTR IIRFilter(float alpha, int16_t input, int16_t output);
void ICACHE_FLASH_ATTR AdjustPlotDots(circularBuffer_t cbuf1, circularBuffer_t cbuf2);
void ICACHE_FLASH_ATTR AdjustPlotDotsSingle(circularBuffer_t cbuf);

/*============================================================================
 * Static Const Variables
 *==========================================================================*/

static const uint8_t cmBrightnesses[] =
{
    0x01,
    0x08,
    0x40,
    0x80,
    0xC0,
};




/*============================================================================
 * Variables
 *==========================================================================*/


// game logic operations.

swadgeMode colorMoveMode =
{
    .modeName = "ColorShake",
    .fnEnterMode = cmInit,
    .fnExitMode = cmDeInit,
    .fnButtonCallback = cmButtonCallback,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = cmAccelerometerCallback,
    .menuImageData = mnu_colorshake_0,
    .menuImageLen = sizeof(mnu_colorshake_0)
};

int gFRAMECOUNT_MOD_SHIFT_INTERVAL = 0;
int gROTATIONSHIFT = 0; //Amount of spinning of pattern around a LED ring

int cmSamplesProcessed = 0;
accel_t cmAccel = {0};
accel_t cmLastAccel = {0};

accel_t cmLastTestAccel = {0};

uint8_t cmButtonState = 0;
uint8_t cmLastButtonState = 0;

uint8_t cmBrightnessIdx = 2;
uint8_t ledOrderInd[] = {LED_UPPER_LEFT, LED_LOWER_LEFT, LED_LOWER_MID, LED_LOWER_RIGHT, LED_UPPER_RIGHT, LED_UPPER_MID};
static led_t leds[NUM_LIN_LEDS] = {{0}};
int CM_ledCount = 0;
static os_timer_t timerHandleUpdate = {0};

static uint32_t modeStartTime = 0; // time mode started in microseconds.
static uint32_t stateStartTime = 0; // time the most recent state started in microseconds.
static uint32_t deltaTime = 0;  // time elapsed since last update.
static uint32_t modeTime = 0;   // total time the mode has been running.
static uint32_t stateTime = 0;  // total time the game has been running.

static cmState_t currState = CM_TITLE;
static cmState_t prevState;


int16_t xAccel;
int16_t yAccel;
int16_t zAccel;
int16_t bpmFromCrossing;
int16_t bpmFromTau;
int16_t lastsign;

bool gameover;




circularBuffer_t  cirBufNormAccel;
circularBuffer_t  cirBufHighPassNormAccel;
circularBuffer_t  cirBufXaccel;
circularBuffer_t  cirBufLowPassXaccel;
circularBuffer_t  cirBufYaccel;
circularBuffer_t  cirBufLowPassYaccel;
circularBuffer_t  cirBufZaccel;
circularBuffer_t  cirBufLowPassZaccel;
circularBuffer_t  cirBufCrossings;

int16_t bufNormAccel[NUM_DOTS];
int16_t bufHighPassNormAccel[NUM_DOTS];
int16_t bufXaccel[NUM_DOTS];
int16_t bufLowPassXaccel[NUM_DOTS];
int16_t bufYaccel[NUM_DOTS];
int16_t bufLowPassYaccel[NUM_DOTS];
int16_t bufZaccel[NUM_DOTS];
int16_t bufLowPassZaccel[NUM_DOTS];
int16_t bufCrossings[BPM_BUF_SIZE];

int16_t deviations[NUM_DOTS];

float REVOLUTIONS_PER_BEAT = 1 / 8.0;
uint8_t PLOT_SCALE = 32;
uint8_t PLOT_SHIFT = 32;
// do via timer now 10 fps FRAME_RESET = 0 // 0 for 60 fps, 1 for 30 fps, 2 for 20 fps, k for 60/(k+1) fps
uint8_t FRAME_RESET = 0;

int16_t showcount = 0; // used for skipping frames
int16_t adj = SCREEN_BORDER;
int16_t wid = 128 - 2 * SCREEN_BORDER;

uint8_t ledcycle = 0;

int16_t lowPassNormAccel = 0;
int16_t lowPassXaccel = 0;
int16_t lowPassYaccel = 0;
int16_t lowPassZaccel = 0;
int16_t smoothNormAccel = 0;
int16_t smoothXaccel = 0;
int16_t smoothYaccel = 0;
int16_t smoothZaccel = 0;
int16_t prevHighPassNormAccel = 0;
int16_t smoothActivity = 0;
int64_t ledDirection = 1;

// for zero crossing measure time is number of cycles of update
uint16_t crossIntervalCounter = 0;
uint32_t ledPrevIncTime = 0;
uint16_t avePeriodMs = 5;
//uint16_t crossInterval = 5;
bool showCrossOnLed = false;

bool pause = false;
bool skipNextCross = false;
bool still = true;

// Helpers

void ICACHE_FLASH_ATTR initCircularBuffer(circularBuffer_t* cirbuff,  int16_t* buffer, uint16_t length)
{
    cirbuff->length = length;
    cirbuff->buffer = buffer;
    cirbuff->insertHeadInd = 0;
    cirbuff->removeTailInd = 0;
    //TODO is this correct use of memset?
    memset(buffer, 0, length * sizeof(int16_t));
}

int16_t ICACHE_FLASH_ATTR getCircularBufferAtIndex(circularBuffer_t cirbuff,  int16_t index)
{
    // index can be positive, zero, or negative
    // index = -1 is last value placed
    // index = -2 is second to last value placed etc.
    // index = 0 is oldest value
    int16_t i = cirbuff.insertHeadInd + index;
    while (i < 0)
    {
        i += cirbuff.length;
    }
    return cirbuff.buffer[i % cirbuff.length];
}

void ICACHE_FLASH_ATTR circularPush(int16_t value, circularBuffer_t* cirbuff)
{
    cirbuff->buffer[cirbuff->insertHeadInd] = value;
    cirbuff->insertHeadInd = (cirbuff->insertHeadInd + 1) % cirbuff->length;
}

int16_t ICACHE_FLASH_ATTR IIRFilter(float alpha, int16_t  input, int16_t output)
{
    // return updated output and returns it
    return  (1 - alpha) * output + alpha * input;
}

void ICACHE_FLASH_ATTR AdjustPlotDots(circularBuffer_t cbuf1, circularBuffer_t cbuf2)
{
    // Plots a graph with x from 0 to 119 and y from cbuf1.buffer - cbuf2.buffer
    uint16_t i;
    uint16_t i1 = cbuf1.insertHeadInd; // oldest
    uint16_t i2 = cbuf2.insertHeadInd;
    for (i = 0; i < NUM_DOTS; i++, i1++, i2++)
    {
        i1 = (i1 < cbuf1.length) ? i1 : 0;
        i2 = (i2 < cbuf2.length) ? i2 : 0;
        drawPixel(i, (cbuf1.buffer[i1] - cbuf2.buffer[i2]) / 4 + PLOT_SHIFT, WHITE);
    }
}
void ICACHE_FLASH_ATTR AdjustPlotDotsSingle(circularBuffer_t cbuf)
{
    // Plots a graph with x from 0 to 119 and y from cbuf.buffer
    uint16_t i;
    uint16_t i1 = cbuf.insertHeadInd; // oldest
    for (i = 0; i < NUM_DOTS; i++, i1++)
    {
        i1 = (i1 < cbuf.length) ? i1 : 0;
        drawPixel(i, cbuf.buffer[i1] / 30 + PLOT_SHIFT, WHITE);
    }
}

int32_t ICACHE_FLASH_ATTR sumOfBuffer(circularBuffer_t cbuf)
{
    // computes sum
    uint16_t i;
    int32_t sum = 0;
    for (i = 0; i < cbuf.length; i++)
    {
        sum += cbuf.buffer[i];
    }
    return sum;
}

void ICACHE_FLASH_ATTR cmInit(void)
{
    // External from mode_dance to set brightness when using dance mode display
    setDanceBrightness(2);

    // Use ColorChord code to process accelerometer samples
    InitColorChord();
    cmSamplesProcessed = 0;

    // Give us reliable button input.
    enableDebounce(false);

    // Reset mode time tracking.
    modeStartTime = system_get_time();
    modeTime = 0;

    // Reset state stuff.
    cmChangeState(CM_TITLE);

    // Set up all initialization and allocation of memory
    cmNewSetup();

    // Start the update loop.
    os_timer_disarm(&timerHandleUpdate);
    os_timer_setfn(&timerHandleUpdate, (os_timer_func_t*)cmUpdate, NULL);
    os_timer_arm(&timerHandleUpdate, UPDATE_TIME_MS, 1);
}

void ICACHE_FLASH_ATTR cmDeInit(void)
{
    os_timer_disarm(&timerHandleUpdate);
}

void ICACHE_FLASH_ATTR cmButtonCallback(uint8_t state, int button __attribute__((unused)),
                                        int down __attribute__((unused)))
{
    cmButtonState = state;  // Set the state of all buttons
}

void ICACHE_FLASH_ATTR cmAccelerometerCallback(accel_t* accel)
{
    // Set the accelerometer values
    // x coor relates to left right on OLED
    // y coor relates to up down on OLED
    cmAccel.x = accel->y;
    cmAccel.y = accel->x;
    cmAccel.z = accel->z;
}

/**
 * Intermediate function which adjusts brightness and sets the LEDs
 *
 * @param ledData    The LEDs to be scaled, then set
 * @param ledDataLen The length of the LEDs to set
 */
void ICACHE_FLASH_ATTR setCMLeds(led_t* ledData, uint8_t ledDataLen)
{
    uint8_t i;
    led_t ledsAdjusted[NUM_LIN_LEDS];
    for(i = 0; i < ledDataLen / sizeof(led_t); i++)
    {
        ledsAdjusted[i].r = ledData[i].r / cmBrightnesses[cmBrightnessIdx];
        ledsAdjusted[i].g = ledData[i].g / cmBrightnesses[cmBrightnessIdx];
        ledsAdjusted[i].b = ledData[i].b / cmBrightnesses[cmBrightnessIdx];
    }
    setLeds(ledsAdjusted, ledDataLen);
}

int32_t mzMaxSamp = -30000;
int32_t mzMinSamp = 30000;
/**
 * Just run colorchord but will give accelerometer samples
 *
 * @param samp A 32 bit sample
 */
void ICACHE_FLASH_ATTR cmSampleHandler(int32_t samp)
{
    //os_printf("%d\n", samp);
    if (abs(samp) > 0)
    {
        PushSample32( samp );
    }
    else
    {
        PushSample32(0);
        //os_printf("%d ", samp);
    }
    // Here  -2700 < samp < 2780 from audio in demo mode
    // if (samp < mzMinSamp)
    // {
    //     mzMinSamp = samp;
    //     os_printf("range %d to %d\n", mzMinSamp, mzMaxSamp);
    // }
    // if (samp > mzMaxSamp)
    // {
    //     mzMaxSamp = samp;
    //     os_printf("range %d to %d\n", mzMinSamp, mzMaxSamp);
    // }

    cmSamplesProcessed++;

    // If at least NUM_SAMPLES_PER_FRAME samples have been processed
    if( cmSamplesProcessed >= NUM_SAMPLES_PER_FRAME )
    {
        //os_printf("COLORCHORD_ACTIVE %d DFTIIR %d\n", COLORCHORD_ACTIVE, DFTIIR);
        // Don't bother if colorchord is inactive
        if( !COLORCHORD_ACTIVE )
        {
            return;
        }


        // Colorchord magic
        HandleFrameInfo();

        // Set LEDs


        gFRAMECOUNT_MOD_SHIFT_INTERVAL++;
        if ( gFRAMECOUNT_MOD_SHIFT_INTERVAL >= COLORCHORD_SHIFT_INTERVAL )
        {
            gFRAMECOUNT_MOD_SHIFT_INTERVAL = 0;
        }
        //printf("MOD FRAME %d ******\n", gFRAMECOUNT_MOD_SHIFT_INTERVAL);

        switch( COLORCHORD_OUTPUT_DRIVER )
        {
            case 254:
                PureRotatingLEDs();
                break;
            case 255:
                DFTInLights();
                break;
            default:
                UpdateLinearLEDs(); // have variety of display options and uses COLORCHORD_OUTPUT_DRIVER to select them
        };

        // could use this if want overall brightness control
        //setCMLeds( (led_t*)ledOut, NUM_LIN_LEDS * 3 );
        setLeds( (led_t*)ledOut, NUM_LIN_LEDS * 3 );

        // Reset the sample count
        cmSamplesProcessed = 0;
    }
}


void ICACHE_FLASH_ATTR cmUpdate(void* arg __attribute__((unused)))
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
        case CM_TITLE:
        {
            cmTitleInput();
            break;
        }
        case CM_GAME:
        {
            cmGameInput();
            break;
        }
        default:
            break;
    };

    // Mark what our inputs were the last time we acted on them.
    cmLastButtonState = cmButtonState;
    cmLastAccel = cmAccel;

    // Handle Game Logic (based on the state)
    switch( currState )
    {
        case CM_TITLE:
        {
            cmTitleUpdate();
            break;
        }
        case CM_GAME:
        {
            cmGameUpdate();
            break;
        }
        default:
            break;
    };

    // Handle Drawing Frame (based on the state)
    switch( currState )
    {
        case CM_TITLE:
        {
            cmTitleDisplay();
            break;
        }
        case CM_GAME:
        {
            cmGameDisplay();
            break;
        }
        default:
            break;
    };
}

void ICACHE_FLASH_ATTR cmTitleInput(void)
{
    //button a = start game
    if(cmIsButtonPressed(BTN_TITLE_START_GAME))
    {
        cmChangeState(CM_GAME);
    }
    //button b = go to score screen
    else if(cmIsButtonPressed(BTN_TITLE_START_SCORES))
    {
        cmChangeState(CM_GAME);
    }
}

void ICACHE_FLASH_ATTR cmGameInput(void)
{
    //button b = abort and restart at same level
    if(cmIsButtonPressed(BTN_GAME_RIGHT))
    {
        cmNewSetup();
        cmChangeState(CM_TITLE);
    }
    //button a = abort and automatically do cm
    else if(cmIsButtonPressed(BTN_GAME_LEFT))
    {
        //cmChangeState(CM_AUTO);
    }
}

void ICACHE_FLASH_ATTR cmTitleUpdate(void)
{
}

void ICACHE_FLASH_ATTR cmGameUpdate(void)
{
    static struct maxtime_t CM_updatedisplay_timer = { .name = "CM_updateDisplay"};
    maxTimeBegin(&CM_updatedisplay_timer);

    //os_printf("modeTime %d\n", modeTime);

    crossIntervalCounter++;

    clearDisplay();


    showcount += 1;
    if (showcount > FRAME_RESET)
    {
        showcount = 0;
    }
    if (showcount > 0)
    {
        return;
    }
    if (pause)
    {
        return;
    }

    //#define BUILT_IN_INPUT
#ifdef BUILT_IN_INPUT
#define BPM_GEN 16
    xAccel = 200 * sin(6.2831853 * BPM_GEN / 60  * modeTime / 1000000);
    yAccel = 300 * sin(3 * 6.2831853 * BPM_GEN / 60  * modeTime / 1000000);
    zAccel = 255;
#else
    xAccel = cmAccel.x;
    yAccel = cmAccel.y;
    zAccel = cmAccel.z;
#endif

    // IPAD accel = list(map(lambda x, y : x + y, motion.get_gravity(),  motion.get_user_acceleration()))
    // NOTE taking the norm doubles the frequency
    int16_t normAccel = sqrt( (double)xAccel * (double)xAccel + (double)yAccel * (double)yAccel + (double)zAccel *
                              (double)zAccel  );

    // empirical adjustment
    //normAccel *= 3;

    //os_printf("%d %d %d %d\n", xAccel, yAccel, zAccel,   normAccel);

    if (SPECIAL_EFFECT)
    {
        // intertwine raw signal with smoothed
        // only need these buffers if want special effect
        circularPush(xAccel, &cirBufXaccel);
        circularPush(yAccel, &cirBufYaccel);
        circularPush(zAccel, &cirBufZaccel);
    }
    // slightly smoothed signal
    float alphaFast = ALPHA_FAST;

    smoothNormAccel = IIRFilter(alphaFast, normAccel, smoothNormAccel);
    circularPush(smoothNormAccel, &cirBufNormAccel);

    smoothXaccel = IIRFilter(alphaFast, xAccel, smoothXaccel);
    circularPush(smoothXaccel, &cirBufXaccel);

    smoothYaccel = IIRFilter(alphaFast, yAccel, smoothYaccel);
    circularPush(smoothYaccel, &cirBufYaccel);

    smoothZaccel = IIRFilter(alphaFast, zAccel, smoothZaccel);
    circularPush(smoothZaccel, &cirBufZaccel);

    // Identify stillness
    float alphaActive = ALPHA_ACTIVE; // smoothing of deviaton from mean
    smoothActivity = IIRFilter(alphaActive, abs(getCircularBufferAtIndex(cirBufHighPassNormAccel, -1)), smoothActivity);


    // high pass by removing highly smoothed low pass (dc bias)
    float alphaSlow = ALPHA_SLOW;
    // Low pass using IIR
    lowPassNormAccel = IIRFilter(alphaSlow, normAccel, lowPassNormAccel);
    // Low pass via running average
    lowPassNormAccel = sumOfBuffer(cirBufNormAccel) / NUM_DOTS;

    int16_t sample = 10 * (smoothNormAccel - lowPassNormAccel);
    // This is plotted on OLED
    circularPush(sample, &cirBufHighPassNormAccel);

    // Use a simple variation of the YIN method for finding the fundamental freq of signal
    // Calculate slow running average via IIR of deviations of current signal
    //      current signal shifted by tau (where tau from 12 to NUM_DOTS-1)
    // Since accelerometer runs at 10 Hz, 5 Hz is highest freq to detect
    // This is 300 bpm. The frame rate producing the samples here is 60 Hz
    // Ignore tau < 12 ( 12 would estimate 60/12 = 5 Hz signal = 300 bpm)
    // First tau > 11 with smallest deviation gives fundamental period
    // so estimate fundamental freq as sample freq / tau
    // sample freq is 1000 / UPDATE_TIME_MS
    // so lowest freq that can be sensed is sample freq / (NUM_DOTS - 1)
    // ex. 60/119 = 30.25

    int16_t minDeviation = 0x7FFF;
    uint8_t tauArgMin = 0;
    for (uint8_t tau = 12; tau < NUM_DOTS; tau++)
    {
        deviations[tau] = IIRFilter(alphaSlow, abs(getCircularBufferAtIndex(cirBufHighPassNormAccel,
                                    -1) - getCircularBufferAtIndex(cirBufHighPassNormAccel, -1 - tau)), deviations[tau]);
        drawPixel(tau, OLED_HEIGHT - deviations[tau] / 30, WHITE);
        if (deviations[tau] < minDeviation)
        {
            tauArgMin = tau;
            minDeviation = deviations[tau];
        }
    }
    plotLine(tauArgMin, 0, tauArgMin, OLED_HEIGHT, WHITE);
    //#define TRY_COLOR_CHORD
    // send this to be dealt with by color chord
#ifdef TRY_COLOR_CHORD
    cmSampleHandler(sample);
#endif
    // average the last 6 readings as accel sampled at 10hz while this is called at 60hz
    //cmSampleHandler(10 / 6 * (getCircularBufferAtIndex(cirBufHighPassNormAccel,
    //   -1) + getCircularBufferAtIndex(cirBufHighPassNormAccel, -2) + getCircularBufferAtIndex(cirBufHighPassNormAccel,
    //           -3) + getCircularBufferAtIndex(cirBufHighPassNormAccel, -4) + getCircularBufferAtIndex(cirBufHighPassNormAccel,
    //                   -5) + getCircularBufferAtIndex(cirBufHighPassNormAccel, -6) ));

    // low pass for the three axes
    lowPassXaccel = IIRFilter(alphaSlow, xAccel, lowPassXaccel);
    circularPush(lowPassXaccel, &cirBufLowPassXaccel);

    lowPassYaccel = IIRFilter(alphaSlow, yAccel, lowPassYaccel);
    circularPush(lowPassYaccel, &cirBufLowPassYaccel);

    lowPassZaccel = IIRFilter(alphaSlow, zAccel, lowPassZaccel);
    circularPush(lowPassZaccel, &cirBufLowPassZaccel);

    // Plot slightly smoothed less dc bias by adjusting the dots
    AdjustPlotDotsSingle(cirBufHighPassNormAccel);
    //AdjustPlotDots(cirBufXaccel, cirBufLowPassXaccel);
    //AdjustPlotDots(cirBufYaccel, cirBufLowPassYaccel);
    //AdjustPlotDots(cirBufZaccel, cirBufLowPassZaccel);

#ifdef TRY_COLOR_CHORD
    for (uint8_t  i = 0; i < FIXBINS; i++)
    {
        drawPixel(i, OLED_HEIGHT - fuzzed_bins[i] / 2000, WHITE);
        //os_printf("%d ", fuzzed_bins[i]);
    }
    //os_printf("fuzzed_bins[0] = %d\n", fuzzed_bins[0]);
#endif

    CM_printf("smoothActivity = %d\n", smoothActivity);

    // Estimate bpm when movement activity

    //float alphaCross = ALPHA_CROSS; // for smoothing gaps between zero crossing

    CM_printf("%d %d  smooth: %d\n", getCircularBufferAtIndex(cirBufHighPassNormAccel, -2),
              getCircularBufferAtIndex(cirBufHighPassNormAccel, -1), smoothActivity);

    // Ignore low level input so only do if enough activity
#define ACTIVITY_BOUND 15
    if (smoothActivity > ACTIVITY_BOUND )
    {
        circularPush(0x0000, &cirBufCrossings);
        //Compute upward crossings and note it
        if (lastsign >= 0 && sample < 0)
        {
            lastsign = -1;
        }
        else if (lastsign <= 0 && sample > 0)
        {
            lastsign = 1;
            showCrossOnLed = true;
            circularPush(0x0001, &cirBufCrossings);
            crossIntervalCounter = 0;
        }
    }

    // BPM Estimation from upward crossings (can highly overestimate)
    bpmFromCrossing = 60 * S_TO_MS_FACTOR * sumOfBuffer(cirBufCrossings) / BPM_SAMPLE_TIME;

    // BPM Estimation from min shifted deviation
    bpmFromTau = 60 * S_TO_MS_FACTOR / UPDATE_TIME_MS / tauArgMin;


    if (bpmFromTau > 0)
    {
        avePeriodMs = 60 * S_TO_MS_FACTOR  / bpmFromTau;
    }
    else
    {
        avePeriodMs = 60 * S_TO_MS_FACTOR;
    }

    //TODO should be in cmGameDisplay()
    // graphical view of bpm could be here

    // graphical view of amp could be here



    //TODO pitch could simple be computed by tilt
    //   as dac suggestd, button to actuate sound so
    //   make a musical instrument
    if (SOUND_ON)
    {

        // TODO choose nice range of bpm and map to nice pitch range
        // could be from 110 to 880 say if buzzer can accept changing
        // freq. Or could be discrete jumps to pentatonic scale
        //  from buzzer.h to play freq f need  compute period (5,000,000 / (2 * f))
        //  to set up a notePeriod_t
        //  then set up musicalNote_t with this and a duration

        //pitch = (min( max(bpmFromTau, 75), 150) - 75) / 75;


        // song_t testSong =
        // {
        //     .shouldLoop = true,
        //     .numNotes = 1,
        //     .notes = FIX to take period

        // };

        // Play this song continous and stop then change to another freq
        // or use button to start and stop

        // Is it possible to play bufHighPassNormAccel at 160 times faster?
        // this would convert 30 to 300 bpm to 80 to 800 Hz
        // audible indication of bpm
        //sound.play_effect('game:Beep', volume = 300*smoothActivity/500, pitch=pitch)
    }


#ifndef TRY_COLOR_CHORD
    //Clear leds
    memset(leds, 0, sizeof(leds));

    CM_printf("bpmFromCrossing %d, bpmFromTau %d, activity %d\n", bpmFromCrossing, bpmFromTau, smoothActivity);

    uint8_t ledr;
    uint8_t ledg;
    uint8_t ledb;

    // Various led options
    // colors via 3 axes strength
    //scaleLed = 8;
    ledr = getCircularBufferAtIndex(cirBufXaccel, -1);
    ledg = getCircularBufferAtIndex(cirBufYaccel, -1);
    ledb = getCircularBufferAtIndex(cirBufZaccel, -1);
    //os_printf("r:%d  g:%d  b:%d \n", ledr, ledg, ledb);
    //ledr = scaleLed * math.fabs(graphRed[-1].position[1]-384)/384;
    //ledg = scaleLed * math.fabs(graphGreen[-1].position[1]-384)/384;
    //ledb = scaleLed * math.fabs(graphBlue[-1].position[1]-384)/384;

    //Color and intensity related to bpm and amount of shaking using color wheel
    // map 50 to 150 bpm to 0 to 255 for angle
    uint8_t hue = (min( max(bpmFromTau, 30), 300) - 30) * 255 / 100;
    // map 15 to 100 smoothActivity to 0 to 255
    uint8_t val = (min( max(smoothActivity, 15), 900) - 15) * 255 / 885;
    uint32_t colorToShow = EHSVtoHEX(hue, 0xFF, val);

    ledr = (colorToShow >>  0) & 0xFF;
    ledg = (colorToShow >>  8) & 0xFF;
    ledb = (colorToShow >> 16) & 0xFF;

    // Spin the leds syncronized to bpm
    if (modeTime - ledPrevIncTime > MS_TO_US_FACTOR * avePeriodMs / NUM_LIN_LEDS / REVOLUTIONS_PER_BEAT)
    {
        //os_printf("modeTime %d, ledPrevIncTime %d\n", modeTime, ledPrevIncTime);
        ledPrevIncTime = modeTime;
        ledcycle += ledDirection;
        ledcycle = (ledcycle + NUM_LIN_LEDS) % NUM_LIN_LEDS;
    }

    // allcolored the same
#define SHOW_NUM_LEDS (NUM_LIN_LEDS / 2)
    //#define SHOW_NUM_LEDS (NUM_LIN_LEDS)
    for (uint8_t i = 0; i < SHOW_NUM_LEDS; i++)
    {
        //use axis colors or hue colors computed above
        leds[(i + ledcycle) % NUM_LIN_LEDS].r = ledr;
        leds[(i + ledcycle) % NUM_LIN_LEDS].g = ledg;
        leds[(i + ledcycle) % NUM_LIN_LEDS].b = ledb;
    }


    //Flip direction on crossing
    if (showCrossOnLed && crossIntervalCounter == 0)
    {
        //ledDirection *= -1;
    }

    //Brighten on crossing
    if (showCrossOnLed && crossIntervalCounter < 10)
    {
        cmBrightnessIdx = 1;
    }
    else
    {
        showCrossOnLed = false;
        cmBrightnessIdx = 4;
    }



    setCMLeds(leds, sizeof(leds));
#endif
    maxTimeEnd(&CM_updatedisplay_timer);
}


void ICACHE_FLASH_ATTR cmTitleDisplay(void)
{
    // Clear the display.
    clearDisplay();

    // Shake It
    plotCenteredText(0, 5, 127, "SHAKE-COLOR", RADIOSTARS, WHITE);

    //plotCenteredText(0, OLED_HEIGHT / 2, 127, (char*)levelName[cmLevel], IBM_VGA_8, WHITE);

    // SCORES   START
    plotText(0, OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)), "SCORES", IBM_VGA_8, WHITE);
    plotText(OLED_WIDTH - getTextWidth("START", IBM_VGA_8), OLED_HEIGHT - (1 * (FONT_HEIGHT_IBMVGA8 + 1)), "START",
             IBM_VGA_8, WHITE);


}

void ICACHE_FLASH_ATTR cmGameDisplay(void)
{
    char uiStr[32] = {0};
    ets_snprintf(uiStr, sizeof(uiStr), "%d", bpmFromCrossing);
    plotCenteredText(0, 1, OLED_WIDTH / 3, uiStr, IBM_VGA_8, WHITE);

    ets_snprintf(uiStr, sizeof(uiStr), "%d", bpmFromTau);
    plotCenteredText(OLED_WIDTH / 3, 1, 2 * OLED_WIDTH / 3, uiStr, IBM_VGA_8, WHITE);

    ets_snprintf(uiStr, sizeof(uiStr), "%d", smoothActivity);
    plotCenteredText(2 * OLED_WIDTH / 3, 1, OLED_WIDTH, uiStr, IBM_VGA_8, WHITE);

    // ets_snprintf(uiStr, sizeof(uiStr), "%d", avePeriodMs);
    // plotCenteredText(0, OLED_HEIGHT - 1 - FONT_HEIGHT_IBMVGA8, OLED_WIDTH, uiStr, IBM_VGA_8, WHITE);
}

// helper functions.

/**
 * @brief Initializer for cm, allocates memory for work arrays
 *
 * @param
 * @return pointers to the work array???
 */
void ICACHE_FLASH_ATTR cmNewSetup(void)
{
    //NO NEED FOR dynamic as all buffers always same size

    initCircularBuffer(&cirBufNormAccel, bufNormAccel, NUM_DOTS);
    initCircularBuffer(&cirBufHighPassNormAccel, bufHighPassNormAccel, NUM_DOTS);
    initCircularBuffer(&cirBufXaccel, bufXaccel, NUM_DOTS);
    initCircularBuffer(&cirBufLowPassXaccel, bufLowPassXaccel, NUM_DOTS);
    initCircularBuffer(&cirBufYaccel, bufYaccel, NUM_DOTS);
    initCircularBuffer(&cirBufLowPassYaccel, bufLowPassYaccel, NUM_DOTS);
    initCircularBuffer(&cirBufZaccel, bufZaccel, NUM_DOTS);
    initCircularBuffer(&cirBufLowPassZaccel, bufLowPassZaccel, NUM_DOTS);
    initCircularBuffer(&cirBufCrossings, bufCrossings, BPM_BUF_SIZE);

    CM_printf("%d leds\n", NUM_LIN_LEDS);
    //os_printf("%d %d sum: %d\n", getCircularBufferAtIndex(cirBufCrossings, 0), getCircularBufferAtIndex(cirBufCrossings,
    //         -1), sumOfBuffer(cirBufCrossings));
    memset(leds, 0, sizeof(leds));
    memset(deviations, 0, sizeof(deviations));
    gameover = false;


    REVOLUTIONS_PER_BEAT = 1.0 / NUM_LIN_LEDS;
    PLOT_SCALE = 32;
    PLOT_SHIFT = 32;
    // do via timer now 10 fps FRAME_RESET = 0 // 0 for 60 fps, 1 for 30 fps, 2 for 20 fps, k for 60/(k+1) fps

    showcount = 0; // used for skipping frames
    adj = SCREEN_BORDER;
    wid = 128 - 2 * SCREEN_BORDER;

    ledcycle = 0;
    lastsign = 0;

    lowPassNormAccel = 0;
    lowPassXaccel = 0;
    lowPassYaccel = 0;
    lowPassZaccel = 0;
    smoothNormAccel = 0;
    smoothXaccel = 0;
    smoothYaccel = 0;
    smoothZaccel = 0;
    prevHighPassNormAccel = 0;
    smoothActivity = 0;

    crossIntervalCounter = 0;
    ledPrevIncTime = 0;
    avePeriodMs = 5;
    //crossInterval = 5;

    ledDirection = 1;
    pause = false;
    skipNextCross = false;
    still = false;

}

void ICACHE_FLASH_ATTR cmChangeState(cmState_t newState)
{
    prevState = currState;
    currState = newState;
    stateStartTime = system_get_time();
    stateTime = 0;

    switch( currState )
    {
        case CM_TITLE:
            // Clear leds
            memset(leds, 0, sizeof(leds));
            setCMLeds(leds, sizeof(leds));
            break;
        case CM_GAME:
            // All game restart functions happen here.
            break;
        default:
            break;
    };
}

bool ICACHE_FLASH_ATTR cmIsButtonPressed(uint8_t button)
{
    return (cmButtonState & button) && !(cmLastButtonState & button);
}

bool ICACHE_FLASH_ATTR cmIsButtonReleased(uint8_t button)
{
    return !(cmButtonState & button) && (cmLastButtonState & button);
}

bool ICACHE_FLASH_ATTR cmIsButtonDown(uint8_t button)
{
    return cmButtonState & button;
}

bool ICACHE_FLASH_ATTR cmIsButtonUp(uint8_t button)
{
    return !(cmButtonState & button);
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
    uint8_t textWidth = plotText(0, 0, text, font,
                                 INVERSE) - 1; // minus one accounts for the return being where the cursor is.

    // Then we draw the inverse back over it to restore it.
    plotText(0, 0, text, font, INVERSE);
    return textWidth;
}
