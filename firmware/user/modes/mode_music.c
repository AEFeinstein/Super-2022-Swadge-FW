/*
 * mode_music.c
 *
 *  Created on: 14 Nov 2019
 *      Author: bbkiwi
 */

// TODO adjust tempos
// TODO when L button changes rhythm or other parameters pop up temp window

/*============================================================================
 * Includes
 *==========================================================================*/

#include <osapi.h>
#include "buzzer.h" // music notes
#include "hpatimer.h" //buzzer functions
#include "embeddedout.h" // EHSVtoHEX
#include "user_main.h"
#include "mode_music.h"
#include "DFT32.h"
//#include "embeddedout.h"
#include "oled.h"
#include "font.h"
#include "MMA8452Q.h"
#include "bresenham.h"
#include "buttons.h"
#include "math.h"



/*============================================================================
 * Defines
 *==========================================================================*/

// LEDs relation to screen
#define LED_UPPER_LEFT LED_1
#define LED_UPPER_MID LED_2
#define LED_UPPER_RIGHT LED_3
#define LED_LOWER_RIGHT LED_4
#define LED_LOWER_MID LED_5
#define LED_LOWER_LEFT LED_6

#define MAX_NUM_NOTES 30

// update task (16 would give 60 fps like ipad, need read accel that fast too?)
#define UPDATE_TIME_MS 16
/*============================================================================
 * Prototypes
 *==========================================================================*/
bool ICACHE_FLASH_ATTR getIsMutedOptionOveride(void);
void ICACHE_FLASH_ATTR musicEnterMode(void);
void ICACHE_FLASH_ATTR musicExitMode(void);
void ICACHE_FLASH_ATTR musicSampleHandler(int32_t samp);
void ICACHE_FLASH_ATTR musicButtonCallback(uint8_t state __attribute__((unused)),
        int button, int down);
void ICACHE_FLASH_ATTR musicAccelerometerHandler(accel_t* accel);

void ICACHE_FLASH_ATTR music_updateDisplay(void);
//uint16_t ICACHE_FLASH_ATTR norm(int16_t xc, int16_t yc);
void ICACHE_FLASH_ATTR setMusicLeds(led_t* ledData, uint8_t ledDataLen);

// other prototypes in mode_music.h and ode_solvers.h

/*============================================================================
 * Static Const Variables
 *==========================================================================*/

static const uint8_t musicBrightnesses[] =
{
    0x01,
    0x08,
    0x40,
    0x80,
};


/*============================================================================
 * Variables
 *==========================================================================*/

swadgeMode musicMode =
{
    .modeName = "music",
    .fnEnterMode = musicEnterMode,
    .fnExitMode = musicExitMode,
    .fnButtonCallback = musicButtonCallback,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = musicAccelerometerHandler
};


struct
{
    uint8_t currentRhythm;
    uint8_t numRhythms;
    accel_t Accel;
    accel_t AccelHighPass;
    uint8_t ButtonState;
    uint8_t Brightnessidx;
    led_t leds[NUM_LIN_LEDS];
    uint8_t ledOrderInd[6];
    int LedCount;
    notePeriod_t midiNote;
    uint8_t numNotes;
    uint8_t midiScale[MAX_NUM_NOTES];
    float scxc;
    float scyc;
    float scxchold;
    float scychold;
    bool useHighPassAccel;
    bool useSmoothAccel;
    bool playButtonDown;
    float xAccel;
    float yAccel;
    float zAccel;
    float alphaSlow;
    float alphaSmooth;
    float xAccelSlowAve;
    float yAccelSlowAve;
    float zAccelSlowAve;
    float xAccelHighPassSmoothed;
    float yAccelHighPassSmoothed;
    float zAccelHighPassSmoothed;
    float len;
    uint8_t noteNum;
    int16_t countframes;
    uint32_t colorToShow;
    uint8_t ledr;
    uint8_t ledg;
    uint8_t ledb;
} music;

static os_timer_t timerHandleUpdate = {0};
static float pi = 3.15159;
notePeriod_t currentMusicNote;

// 'Songs' for rhythms and riffs
// TODO adjust tempos

const song_t oneNoteRhythm RODATA_ATTR =
{
    .notes = {
        {.note = CURRENT_MUSIC_MODE_NOTE, .timeMs = 5000},
    },
    .numNotes = 1,
    .shouldLoop = true
};


const song_t didididahRhythm RODATA_ATTR =
{
    .notes = {
        {.note = CURRENT_MUSIC_MODE_NOTE, .timeMs = 230},
        {.note = SILENCE, .timeMs = 20},
        {.note = CURRENT_MUSIC_MODE_NOTE, .timeMs = 230},
        {.note = SILENCE, .timeMs = 20},
        {.note = CURRENT_MUSIC_MODE_NOTE, .timeMs = 230},
        {.note = SILENCE, .timeMs = 20},
        {.note = CURRENT_MUSIC_MODE_NOTE, .timeMs = 740},
        {.note = SILENCE, .timeMs = 10},
    },
    .numNotes = 8,
    .shouldLoop = true
};

const song_t majorChord RODATA_ATTR =
{
    .notes = {
        {.note = CURRENT_MUSIC_MODE_NOTE, .timeMs = 300},
        {.note = SILENCE, .timeMs = 10},
        {.note = CURRENT_MUSIC_MODE_NOTE_UP_3RD, .timeMs = 300},
        {.note = SILENCE, .timeMs = 10},
        {.note = CURRENT_MUSIC_MODE_NOTE_UP_5TH, .timeMs = 300},
        {.note = SILENCE, .timeMs = 30},
    },
    .numNotes = 6,
    .shouldLoop = true
};

const song_t dacsRhythm RODATA_ATTR =
{
    .notes = {
        {.note = CURRENT_MUSIC_MODE_NOTE, .timeMs = 175},
        {.note = SILENCE, .timeMs = 25},
    },
    .numNotes = 2,
    .shouldLoop = true
};

const song_t backbeatRhythm RODATA_ATTR =
{
    .notes = {
        {.note = CURRENT_MUSIC_MODE_NOTE, .timeMs = 240},
        {.note = SILENCE, .timeMs = 160},
        {.note = CURRENT_MUSIC_MODE_NOTE, .timeMs = 350},
        {.note = SILENCE, .timeMs = 50},
        {.note = CURRENT_MUSIC_MODE_NOTE, .timeMs = 200},
        {.note = SILENCE, .timeMs = 200},
        {.note = CURRENT_MUSIC_MODE_NOTE, .timeMs = 350},
        {.note = SILENCE, .timeMs = 50},
    },
    .numNotes = 8,
    .shouldLoop = true
};

const song_t* rhythmPatterns[] =
{
    &oneNoteRhythm,
    &dacsRhythm,
    &didididahRhythm,
    &backbeatRhythm,
    &majorChord,
};

/*============================================================================
 * Functions
 *==========================================================================*/
bool ICACHE_FLASH_ATTR getIsMutedOptionOveride(void)
{
    return false;
}

/**
 * Initializer for music
 */
void ICACHE_FLASH_ATTR musicEnterMode(void)
{
    getIsMutedOption = &getIsMutedOptionOveride;

    // Start the update loop.
    os_timer_disarm(&timerHandleUpdate);
    os_timer_setfn(&timerHandleUpdate, (os_timer_func_t*)music_updateDisplay, NULL);
    os_timer_arm(&timerHandleUpdate, UPDATE_TIME_MS, 1);

    music.useHighPassAccel = true; //adjust for position being held
    music.useSmoothAccel = true;
    music.currentRhythm = 0;
    //os_printf("%d\n", sizeof(rhythmPatterns) / sizeof( song_t*));
    music.numRhythms = sizeof(rhythmPatterns) / sizeof( song_t*);
    music.ButtonState = 0;
    music.Brightnessidx = 1;
    music.LedCount = 0;
    music.scxc = 0;
    music.scyc = 0;
    music.alphaSlow = 0.02; // for finding long term moving average
    music.alphaSmooth = 0.3; // for slight smoothing of High Pass Accel
    music.ledOrderInd[0] = LED_UPPER_LEFT;
    music.ledOrderInd[1] = LED_LOWER_LEFT;
    music.ledOrderInd[2] = LED_LOWER_MID;
    music.ledOrderInd[3] = LED_LOWER_RIGHT;
    music.ledOrderInd[4] = LED_UPPER_RIGHT;
    music.ledOrderInd[5] = LED_UPPER_MID;

    enableDebounce(false);
    music.numNotes = 6;
    uint8_t intervals[] = {2, 3, 2, 2, 3}; // pentatonic
    // uint8_t intervals[] =  {2,2,1,2,2,2,1}; //major
    // uint8_t intervals[] =  {2,1,2,2,1,2,2}; //minor
    // uint8_t intervals[] =  {4,3,5}; //major arpeggio
    // uint8_t intervals[] =  {3,4,5}; //minor arpeggio
    // uint8_t intervals[] =  {3}; //diminished
    // uint8_t intervals[] =  {4}; //augmented
    // uint8_t intervals[] =  {2}; //whole tone
    // uint8_t intervals[] =  {1}; //chromatic
    // uint8_t intervals[] =  {7,-5,7,-5,7,-5,-5}; //cirle of 5th

    generateScale(music.midiScale, music.numNotes, intervals, sizeof(intervals) );
}

/**
 * Called when music is exited
 */
void ICACHE_FLASH_ATTR musicExitMode(void)
{
    os_timer_disarm(&timerHandleUpdate);
}



void ICACHE_FLASH_ATTR music_updateDisplay(void)
{
    // Clear the display
    clearDisplay();

    //NOTE bug in Expressif OS can't print floating point! Must cast as int
    //Save accelerometer reading in global storage
    //TODO can get values bigger than 1. here, my accelerometer has 14 bits
    music.xAccel = music.Accel.x / 256.0;
    music.yAccel = music.Accel.y / 256.0;
    music.zAccel = music.Accel.z / 256.0;

    //os_printf("%d %d %d\n", (int)(100 * music.xAccel), (int)(100 * music.yAccel), (int)(100 * music.zAccel));

    // Insure solution coordinates on OLED for the moving ball
    // OLED xcoord from 0 (left) to 127, ycoord from 0(top) to 63
    // flat corresponds to the average postion and level measured as deviation from this
    // Using center of screen as orgin, position ball on circle of radius 32 with direction x,y component of Accel
    // acts as level with ball at lowest spot

    music.scxc = music.Accel.x;
    music.scyc = music.Accel.y;

    music.len = sqrt(music.scxc * music.scxc + music.scyc * music.scyc);
    // Larger IGNORE_RADIUS ignore nearly rest position readings
#define IGNORE_RADIUS 0.2
    if (music.len > IGNORE_RADIUS)
    {
        // scale normalized vector to length 28 to keep ball within bounds of screen
        music.scxc = 64.0 + 28.0 * music.scxc / music.len;
        music.scyc = 32.0 + 28.0 * music.scyc / music.len;
        music.scxchold = music.scxc;
        music.scychold = music.scyc;
    }
    else
    {
        music.scxc = music.scxchold;
        music.scyc = music.scychold;
    }

    char uiStr[32] = {0};
    ets_snprintf(uiStr, sizeof(uiStr), "%d", music.currentRhythm);
    plotText(120, 53, uiStr, IBM_VGA_8, WHITE);

    // Draw note sectors
    for (uint8_t i = 0; i < music.numNotes; i++)
    {
        plotLine(64, 32, 64 + 28 * sin(2 * pi * i / music.numNotes), 32 + 28 * cos(2 * pi * i / music.numNotes), WHITE );
    }

    // Draw virtual ball
    //plotCircle(music.scxc, music.scyc, 5, WHITE);
    plotCircle(music.scxc, music.scyc, 3, WHITE);
    plotCircle(music.scxc, music.scyc, 1, WHITE);
    //os_printf("(%d, %d\n", (int)music.scxc, (int)music.scyc);

    // Set midiNote
    music.noteNum = (int)(music.numNotes * (1 + atan2(music.scxc - 64, music.scyc - 32) / 2.0 / pi)) % music.numNotes;
    music.midiNote = midi2note(music.midiScale[music.noteNum]);
    currentMusicNote = music.midiNote;
    //os_printf("notenum = %d,   midi = %d,  music.midiNote = %d\n", notenum, music.midiScale[notenum], music.midiNote);

    // LEDs, all off
    ets_memset(music.leds, 0, sizeof(music.leds));

    // All LEDs glow with note color - dim if not sounding, bright is sounding
    if (music.playButtonDown)
    {
        music.colorToShow = EHSVtoHEX((music.midiScale[music.noteNum] % 12) * 255 / 12, 0xFF, 0xFF);
    }
    else
    {
        music.colorToShow = EHSVtoHEX((music.midiScale[music.noteNum] % 12) * 255 / 12, 0xFF, 0x7F);
    }
    music.ledr = (music.colorToShow >>  0) & 0xFF;
    music.ledg = (music.colorToShow >>  8) & 0xFF;
    music.ledb = (music.colorToShow >> 16) & 0xFF;

    //NOTE set to  == if want to test on my mockup with 16 leds
#define USE_6_LEDS
#ifndef USE_6_LEDS
    for (uint8_t indLed = 0; indLed < NUM_LIN_LEDS ; indLed++)
    {
        music.leds[indLed].r = music.ledr;
        music.leds[indLed].g = music.ledg;
        music.leds[indLed].b = music.ledb;
    }
#else
    for (uint8_t indLed = 0; indLed < 6 ; indLed++)
    {
        music.leds[music.ledOrderInd[indLed]].r = music.ledr;
        music.leds[music.ledOrderInd[indLed]].g = music.ledg;
        music.leds[music.ledOrderInd[indLed]].b = music.ledb;
    }
#endif

    setMusicLeds(music.leds, sizeof(music.leds));
}


/**
 *
 * @param state  A bitmask of all button states, unused
 * @param button The button which triggered this event
 * @param down   true if the button was pressed, false if it was released
 */
void ICACHE_FLASH_ATTR musicButtonCallback( uint8_t state,
        int button __attribute__((unused)), int down __attribute__((unused)))
{
    music.ButtonState = state;
    //music_updateDisplay();

    if(down)
    {
        if(2 == button)
        {
            // play notes given by ball using a specified Rythmn
            music.playButtonDown = true;
            startBuzzerSong(rhythmPatterns[music.currentRhythm]);
        }
        if(1 == button)
        {
            // Cycle movement methods
            music.currentRhythm = (music.currentRhythm + 1) % music.numRhythms;
            os_printf("music.currentRhythm = %d\n", music.currentRhythm);
        }
    }
    else
    {
        music.playButtonDown = false;
        //os_printf("up \n");
        stopBuzzerSong();
    }
}

/**
 * Store the acceleration data to be displayed later
 * Also based on flags uses High Pass filter and Slight Smoothing
 *
 * @param x
 * @param x
 * @param z
 */
void ICACHE_FLASH_ATTR musicAccelerometerHandler(accel_t* accel)
{
    music.Accel.x = accel->y;
    music.Accel.y = accel->x;
    music.Accel.z = accel->z;

    if (music.useHighPassAccel)
    {
        music.xAccelSlowAve = (1.0 - music.alphaSlow) * music.xAccelSlowAve + music.alphaSlow * (float)music.Accel.x;
        music.yAccelSlowAve = (1.0 - music.alphaSlow) * music.yAccelSlowAve + music.alphaSlow * (float)music.Accel.y;
        music.zAccelSlowAve = (1.0 - music.alphaSlow) * music.zAccelSlowAve + music.alphaSlow * (float)music.Accel.z;

        music.Accel.x = music.Accel.x - music.xAccelSlowAve;
        music.Accel.y = music.Accel.y - music.yAccelSlowAve;
        music.Accel.z = music.Accel.z - music.zAccelSlowAve;
    }
    if (music.useSmoothAccel)
    {
        music.xAccelHighPassSmoothed = (1.0 - music.alphaSmooth) * music.xAccelHighPassSmoothed + music.alphaSmooth *
                                       (float)music.Accel.x;
        music.yAccelHighPassSmoothed = (1.0 - music.alphaSmooth) * music.yAccelHighPassSmoothed + music.alphaSmooth *
                                       (float)music.Accel.y;
        music.zAccelHighPassSmoothed = (1.0 - music.alphaSmooth) * music.zAccelHighPassSmoothed + music.alphaSmooth *
                                       (float)music.Accel.z;

        music.Accel.x = music.xAccelHighPassSmoothed;
        music.Accel.y = music.yAccelHighPassSmoothed;
        music.Accel.z = music.zAccelHighPassSmoothed;
    }



    //music_updateDisplay();
}

/**
 * Intermediate function which adjusts brightness and sets the LEDs
 *
 * @param ledData    The LEDs to be scaled, then set
 * @param ledDataLen The length of the LEDs to set
 */
void ICACHE_FLASH_ATTR setMusicLeds(led_t* ledData, uint8_t ledDataLen)
{
    uint8_t i;
    for(i = 0; i < ledDataLen / sizeof(led_t); i++)
    {
        ledData[i].r = ledData[i].r / musicBrightnesses[music.Brightnessidx];
        ledData[i].g = ledData[i].g / musicBrightnesses[music.Brightnessidx];
        ledData[i].b = ledData[i].b / musicBrightnesses[music.Brightnessidx];
    }
    setLeds(ledData, ledDataLen);
}


/*
def midiToFreq(mid):
    if mid is None:
        return (None, None, None, None, None)
    letNames = {0:'c', 1:'c#', 2:'d', 3:'d#', 4:'e', 5:'f', 6:'f#', 7:'g', 8:'g#', 9:'a', 10:'a#', 11:'b'}
    freq = 55 * pow(2, (mid - 33 )/12)
    oct = int(mid / 12) - 1
    note = mid % 12
    k = int(abs(oct - 2.5))
    symbol = letNames[note] + "'" * k if oct > 2.5 else letNames[note].upper() + "," * k
    return (freq, symbol , oct, note, colorsys.hsv_to_rgb(note/12,1,1)  )
*/

/**
 * Converts midi number to notePeriod_t to be used as note
 *
 * @param mid
 */
notePeriod_t ICACHE_FLASH_ATTR midi2note(uint8_t mid)
{
    int32_t freq = 55.0 * pow(2.0, ((float)mid - 33.0 ) / 12.0);
    return 2500000 / freq;
}

void ICACHE_FLASH_ATTR generateScale(uint8_t* midiScale, uint8_t numNotes, uint8_t intervals[], uint8_t nIntervals)
{
    //intervals are jumps in the scale
    //nIntervals must be the dimension of intervals
    uint8_t i;
    uint8_t j = 0;
    //os_printf("n = %d\n", nIntervals);
    uint8_t baseMidi = 90; // 80; // with major and 9 notes
    midiScale[0] = baseMidi;
    for (i = 1; i < numNotes; i++)
    {
        if (i >= MAX_NUM_NOTES)
        {
            return;
        }
        midiScale[i] = midiScale[i - 1] + intervals[j];
        j++;
        if (j >= nIntervals)
        {
            j = 0;
        }
    }
}


/*
def generateScale(musicalScale, numNotes=6, baseMidi=60, name='pentatonic'):
    '''baseMidi is in the middle and work forward and backward
    '''
    intervals = dict(
        pentatonic=[2,3,2,2,3],
        major=[2,2,1,2,2,2,1],
        minor=[2,1,2,2,1,2,2],
        arpeggiomaj=[4,3,5],
        arpeggiomin=[3,4,5],
        diminished=[3,3,3,3],
        augmented=[4,4,4],
        wholetone=[2,2,2,2,2,2],
        chromatic=[1],
        circleof5=[7,-5,7,-5,7,-5,-5])

    x = baseMidi
    for k in range(int(numNotes/2)):
        nextint = intervals[name].pop()
        intervals[name].insert(0,nextint)
        x -= nextint

    for n in range(numNotes):
        musicalScale.append(x)
        nextint = intervals[name].pop(0)
        intervals[name].append(nextint)
        x += nextint
*/