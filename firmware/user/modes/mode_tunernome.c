/*
 * mode_tunernome.c
 *
 *  Created on: September 17th, 2020
 *      Author: bryce
 */
#include "user_config.h"

/*============================================================================
 * Includes
 *==========================================================================*/

#include <osapi.h>
#include <mem.h>
#include <stdint.h>
#include <limits.h>
#include <math.h>
#include <user_interface.h>

#include "buttons.h"
#include "user_main.h"
#include "embeddednf.h"
#include "oled.h"
#include "bresenham.h"
#include "assets.h"
#include "buttons.h"
#include "linked_list.h"
#include "font.h"
#include "mode_colorchord.h"

#include "embeddednf.h"
#include "embeddedout.h"

/*============================================================================
 * Defines, Structs, Enums
 *==========================================================================*/

#ifndef M_PI
#define M_PI		          3.14159265358979323846
#endif

#define TUNERNOME_UPDATE_MS   15

#define USE_ARROW_PNG         0 // otherwise, draw arrows with text (ugly)
#define NUM_GUITAR_STRINGS    6
#define NUM_UKELELE_STRINGS   4
#define GUITAR_OFFSET         0
#define CHROMATIC_OFFSET      6 // adjust start point by quartertones
#define SENSITIVITY           5

#define METRONOME_CENTER_X    OLED_WIDTH / 2
#define METRONOME_CENTER_Y    OLED_HEIGHT - 10
#define METRONOME_RADIUS      35
#define INITIAL_BPM           60
#define MAX_BPM               400
#define METRONOME_FLASH_MS    35
#define BPM_CHANGE_FIRST_MS   500
#define BPM_CHANGE_REPEAT_MS  50
#define PAUSE_WIDTH           3
#define PAUSE_SPACE_WIDTH     3
#define PAUSE_HEIGHT          10

/// Helper macro to return an integer clamped within a range (MIN to MAX)
#define CLAMP(X, MIN, MAX) ( ((X) > (MAX)) ? (MAX) : ( ((X) < (MIN)) ? (MIN) : (X)) )
/// Helper macro to return the absolute value of an integer
#define ABS(X) (((X) < 0) ? -(X) : (X))
/// Helper macro to return the highest of two integers
#define MAX(X, Y) ( ((X) > (Y)) ? (X) : (Y) )

typedef enum
{
    TN_TUNER,
    TN_METRONOME
} tnMode;

typedef enum
{
    GUITAR_TUNER = 0,
    UKELELE_TUNER,
    SEMITONE_0,
    SEMITONE_1,
    SEMITONE_2,
    SEMITONE_3,
    SEMITONE_4,
    SEMITONE_5,
    SEMITONE_6,
    SEMITONE_7,
    SEMITONE_8,
    SEMITONE_9,
    SEMITONE_10,
    SEMITONE_11,
    MAX_GUITAR_MODES
} tuner_mode_t;

typedef struct
{
    tnMode mode;
    tuner_mode_t curTunerMode;

    timer_t updateTimer;
    timer_t ledTimer;
    timer_t bpmButtonTimer;

    int audioSamplesProcessed;
    uint32_t intensities_filt[MAX(NUM_GUITAR_STRINGS, NUM_UKELELE_STRINGS)];
    int32_t diffs_filt[MAX(NUM_GUITAR_STRINGS, NUM_UKELELE_STRINGS)];

    bool pause;
    int bpm;
    uint32_t tLastUpdateUs;
    int32_t tAccumulatedUs;
    bool isClockwise;
    uint32_t usPerBeat;

    int lastBpmButton;

    uint32_t semitone_intensitiy_filt;
    int32_t semitone_diff_filt;

#if USE_ARROW_PNG
    pngHandle upArrowPng;
#endif
} tunernome_t;

/*============================================================================
 * Prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR tunernomeEnterMode(void);
void ICACHE_FLASH_ATTR tunernomeExitMode(void);
void ICACHE_FLASH_ATTR switchToSubmode(tnMode);
void ICACHE_FLASH_ATTR tunernomeButtonCallback(uint8_t state __attribute__((unused)),
        int button, int down);
void ICACHE_FLASH_ATTR increaseBpm(void* timer_arg __attribute__((unused)));
void ICACHE_FLASH_ATTR decreaseBpm(void* timer_arg __attribute__((unused)));
void ICACHE_FLASH_ATTR tunernomeSampleHandler(int32_t samp);
void ICACHE_FLASH_ATTR recalcMetronome(void);
static void ICACHE_FLASH_ATTR tunernomeUpdate(void* arg __attribute__((unused)));
void ICACHE_FLASH_ATTR ledReset(void* timer_arg __attribute__((unused)));
void ICACHE_FLASH_ATTR fasterBpmChange(void* timer_arg __attribute__((unused)));

static inline int16_t getMagnitude(uint16_t idx);
static inline int16_t getDiffAround(uint16_t idx);
static inline int16_t getSemiMagnitude(int16_t idx);
static inline int16_t getSemiDiffAround(uint16_t idx);

/*============================================================================
 * Variables
 *==========================================================================*/

swadgeMode tunernomeMode =
{
    .modeName = "tunernome",
    .fnEnterMode = tunernomeEnterMode,
    .fnExitMode = tunernomeExitMode,
    .fnButtonCallback = tunernomeButtonCallback,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = NULL,
    .fnAudioCallback = tunernomeSampleHandler,
    .menuImg = "tn-menu.gif"
};

tunernome_t* tunernome;

/*============================================================================
 * Const Variables
 *==========================================================================*/

/**
 * Indicies into fuzzed_bins[], a realtime DFT of sorts
 * fuzzed_bins[0] = A ... 1/2 steps are every 2.
 */
const uint16_t freqBinIdxsGuitar[NUM_GUITAR_STRINGS] =
{
    38, // E string needs to skip an octave... Can't read sounds this low.
    24, // A string is exactly at note #24
    34, // D = A + 5 half steps = 34
    44, // G
    52, // B
    62  // e
};

/**
 * Indicies into fuzzed_bins[], a realtime DFT of sorts
 * fuzzed_bins[0] = A ... 1/2 steps are every 2.
 */
const uint16_t freqBinIdxsUkelele[NUM_UKELELE_STRINGS] =
{
    68, // G
    54, // C
    62, // E
    72  // A
};

const uint16_t ukeleleStringIdxToLedIdx[NUM_UKELELE_STRINGS] =
{
    0,
    1,
    4,
    5
};

const char* guitarNoteNames[12] =
{
    "E2",
    "A2",
    "D3",
    "G3",
    "B3",
    "E4"
};

const char* ukeleleNoteNames[12] =
{
    "G4",
    "C4",
    "E4",
    "A4"
};

const char* semitoneNoteNames[12] =
{
    "C",
    "C#",
    "D",
    "D#",
    "E",
    "F",
    "F#",
    "G",
    "G#",
    "A",
    "A#",
    "B"
};

static const char tn_title[]  = "Tunernome";
static const char theWordGuitar[] = "Guitar";
static const char theWordUkelele[] = "Ukelele";
static const char leftStr[] = "< Exit";
static const char rightStrTuner[] = "Tuner >";
static const char rightStrMetronome[] = "Metronome >";
#if !USE_ARROW_PNG
static const char upArrowStr[] = "/\\";
static const char downArrowStr[] = "\\/";
#endif

/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * Initializer for tunernome
 */
void ICACHE_FLASH_ATTR tunernomeEnterMode(void)
{
    // Alloc and clear everything
    tunernome = os_malloc(sizeof(tunernome_t));
    ets_memset(tunernome, 0, sizeof(tunernome_t));

#if USE_ARROW_PNG
    allocPngAsset("uparrow.png", &(tunernome->upArrowPng));
#endif

    switchToSubmode(TN_TUNER);

    timerDisarm(&(tunernome->updateTimer));
    timerSetFn(&(tunernome->updateTimer), tunernomeUpdate, NULL);
    timerArm(&(tunernome->updateTimer), TUNERNOME_UPDATE_MS, true);

    timerDisarm(&(tunernome->ledTimer));
    timerSetFn(&(tunernome->ledTimer), ledReset, NULL);

    timerDisarm(&(tunernome->bpmButtonTimer));
    timerSetFn(&(tunernome->bpmButtonTimer), fasterBpmChange, NULL);

    enableDebounce(true);

    InitColorChord();
}

/**
 * Switch internal mode
 */
void ICACHE_FLASH_ATTR switchToSubmode(tnMode newMode)
{
    switch(newMode)
    {
        case TN_TUNER:
        {
            tunernome->mode = newMode;

            led_t leds[NUM_LIN_LEDS] = {{0}};
            setLeds(leds, sizeof(leds));

            enableDebounce(true);

            clearDisplay();
            
            break;
        }
        case TN_METRONOME:
        {
            tunernome-> mode = newMode;

            tunernome->pause = false;
            tunernome->bpm = INITIAL_BPM;
            tunernome->isClockwise = true;
            tunernome->tLastUpdateUs = 0;
            tunernome->tAccumulatedUs = 0;

            tunernome->lastBpmButton = 0;

            recalcMetronome();

            led_t leds[NUM_LIN_LEDS] = {{0}};
            setLeds(leds, sizeof(leds));

            enableDebounce(false);

            clearDisplay();
            break;
        }
        default:
        {
            break;
        }
    }
}

/**
 * Called when tunernome is exited
 */
void ICACHE_FLASH_ATTR tunernomeExitMode(void)
{
#if USE_ARROW_PNG
    freePngAsset(&(tunernome->upArrowPng));
#endif

    timerDisarm(&(tunernome->updateTimer));
    timerDisarm(&(tunernome->ledTimer));
    timerDisarm(&(tunernome->bpmButtonTimer));
    timerFlush();

    os_free(tunernome);
}

/**
 * Inline helper function to get the magnitude of a frequency bin from fuzzed_bins[]
 *
 * @param idx The index to get a magnitude from
 * @return A signed magnitude, even though fuzzed_bins[] is unsigned
 */
static inline int16_t getMagnitude(uint16_t idx)
{
    return fuzzed_bins[idx];
}

/**
 * Inline helper function to get the difference in magnitudes around a given
 * frequency bin from fuzzed_bins[]
 *
 * @param idx The index to get a difference in magnitudes around
 * @return The difference in magnitudes of the bins before and after the given index
 */
static inline int16_t getDiffAround(uint16_t idx)
{
    return getMagnitude(idx + 1) - getMagnitude(idx - 1);
}

/**
 * Inline helper function to get the magnitude of a frequency bin from folded_bins[]
 *
 * @param idx The index to get a magnitude from
 * @return A signed magnitude, even though folded_bins[] is unsigned
 */
static inline int16_t getSemiMagnitude(int16_t idx)
{
    if(idx < 0)
    {
        idx += FIXBPERO;
    }
    if(idx > FIXBPERO - 1)
    {
        idx -= FIXBPERO;
    }
    return folded_bins[idx];
}

/**
 * Inline helper function to get the difference in magnitudes around a given
 * frequency bin from folded_bins[]
 *
 * @param idx The index to get a difference in magnitudes around
 * @return The difference in magnitudes of the bins before and after the given index
 */
static inline int16_t getSemiDiffAround(uint16_t idx)
{
    return getSemiMagnitude(idx + 1) - getSemiMagnitude(idx - 1);
}

/**
 * Recalculate the per-bpm values for the metronome
 */
void ICACHE_FLASH_ATTR recalcMetronome(void) {
    // Figure out how many microseconds are in one beat
    tunernome->usPerBeat = (60 * 1000000) / tunernome->bpm;
    
}

/**
 * @brief called on a timer, updates the game state
 *
 * @param arg
 */
static void ICACHE_FLASH_ATTR tunernomeUpdate(void* arg __attribute__((unused)))
{
    switch(tunernome->mode)
    {
        default:
        case TN_TUNER:
        {
            clearDisplay();

            // Instructions at top of display
            plotText(0, 0, "Blu= Flat   Wht= OK   Red= Sharp", TOM_THUMB, WHITE);

            // Left/Right button functions at bottom of display
            plotText(0, OLED_HEIGHT - FONT_HEIGHT_TOMTHUMB, leftStr, TOM_THUMB, WHITE);
            plotText(OLED_WIDTH - textWidth(rightStrMetronome, TOM_THUMB), OLED_HEIGHT - FONT_HEIGHT_TOMTHUMB, rightStrMetronome, TOM_THUMB, WHITE);

            // Up/Down arrows in middle of display around current note/mode
#if USE_ARROW_PNG
            drawPng(&(tunernome->upArrowPng),
                    (OLED_WIDTH - tunernome->upArrowPng.width) / 2,
                    (OLED_HEIGHT - FONT_HEIGHT_IBMVGA8) / 2 - tunernome->upArrowPng.height - 10,
                    false, false, 0);
            drawPng(&(tunernome->upArrowPng),
                    (OLED_WIDTH - tunernome->upArrowPng.width) / 2,
                    (OLED_HEIGHT + FONT_HEIGHT_IBMVGA8) / 2 + 10,
                    true, false, 0);
#else
            plotText((OLED_WIDTH - textWidth(upArrowStr, TOM_THUMB)) / 2,
                     (OLED_HEIGHT - FONT_HEIGHT_IBMVGA8) / 2 - FONT_HEIGHT_TOMTHUMB - 10,
                     upArrowStr, TOM_THUMB, WHITE);
            plotText((OLED_WIDTH - textWidth(downArrowStr, TOM_THUMB)) / 2,
                     (OLED_HEIGHT + FONT_HEIGHT_IBMVGA8) / 2 + 10,
                     downArrowStr, TOM_THUMB, WHITE);
#endif

            // Current note/mode in middle of display
            switch(tunernome->curTunerMode)
            {
                case GUITAR_TUNER:
                {
                    // Mode name
                    plotText((OLED_WIDTH - textWidth(theWordGuitar, IBM_VGA_8)) / 2,
                             (OLED_HEIGHT - FONT_HEIGHT_IBMVGA8) / 2,
                             theWordGuitar, IBM_VGA_8, WHITE);

                    // Note names of strings, arranged to match LED positions
                    for(int i = 0; i < NUM_GUITAR_STRINGS / 2; i++)
                    {
                        plotText((OLED_WIDTH - textWidth(theWordGuitar, IBM_VGA_8)) / 2 - textWidth("G4 ", IBM_VGA_8),
                                 (OLED_HEIGHT - FONT_HEIGHT_IBMVGA8) / 2 + (FONT_HEIGHT_IBMVGA8 + 5) * (1 - i),
                                 guitarNoteNames[i], IBM_VGA_8, WHITE);
                    }
                    for(int i = NUM_GUITAR_STRINGS / 2; i < NUM_GUITAR_STRINGS; i++)
                    {
                        plotText((OLED_WIDTH + textWidth(theWordGuitar, IBM_VGA_8)) / 2 + textWidth(" ", IBM_VGA_8),
                                 (OLED_HEIGHT - FONT_HEIGHT_IBMVGA8) / 2 + (FONT_HEIGHT_IBMVGA8 + 5) * (i - (NUM_GUITAR_STRINGS / 2) - 1),
                                 guitarNoteNames[i], IBM_VGA_8, WHITE);
                    }

                    break;
                }
                case UKELELE_TUNER:
                {
                    // Mode name
                    plotText((OLED_WIDTH - textWidth(theWordUkelele, IBM_VGA_8)) / 2,
                             (OLED_HEIGHT - FONT_HEIGHT_IBMVGA8) / 2,
                             theWordUkelele, IBM_VGA_8, WHITE);

                    // Note names of strings, arranged to match LED positions
                    for(int i = 0; i < NUM_UKELELE_STRINGS / 2; i++)
                    {
                        plotText((OLED_WIDTH - textWidth(theWordUkelele, IBM_VGA_8)) / 2 - textWidth("G4 ", IBM_VGA_8),
                                 OLED_HEIGHT / 2 + (FONT_HEIGHT_IBMVGA8 + 5) * (- i),
                                 ukeleleNoteNames[i], IBM_VGA_8, WHITE);
                    }
                    for(int i = NUM_UKELELE_STRINGS / 2; i < NUM_UKELELE_STRINGS; i++)
                    {
                        plotText((OLED_WIDTH + textWidth(theWordUkelele, IBM_VGA_8)) / 2 + textWidth(" ", IBM_VGA_8),
                                 OLED_HEIGHT / 2 + (FONT_HEIGHT_IBMVGA8 + 5) * (i - (NUM_UKELELE_STRINGS / 2) - 1),
                                 ukeleleNoteNames[i], IBM_VGA_8, WHITE);
                    }

                    break;
                }
                case MAX_GUITAR_MODES:
                    break;
                case SEMITONE_0:
                case SEMITONE_1:
                case SEMITONE_2:
                case SEMITONE_3:
                case SEMITONE_4:
                case SEMITONE_5:
                case SEMITONE_6:
                case SEMITONE_7:
                case SEMITONE_8:
                case SEMITONE_9:
                case SEMITONE_10:
                case SEMITONE_11:
                default:
                {
                    uint8_t semitoneNum = (tunernome->curTunerMode - SEMITONE_0);
                    plotText((OLED_WIDTH - textWidth(semitoneNoteNames[semitoneNum], IBM_VGA_8)) / 2,
                             (OLED_HEIGHT - FONT_HEIGHT_IBMVGA8) / 2,
                             semitoneNoteNames[semitoneNum], IBM_VGA_8, WHITE);
                    
                    break;
                }
            }

            break;
        }
        case TN_METRONOME:
        {
            clearDisplay();
    
            char bpmStr[8];
            ets_sprintf(bpmStr, "%d bpm", tunernome->bpm);

            plotText((OLED_WIDTH - textWidth(bpmStr, IBM_VGA_8)) / 2, 0, bpmStr, IBM_VGA_8, WHITE);
            plotText(0, OLED_HEIGHT - FONT_HEIGHT_TOMTHUMB, leftStr, TOM_THUMB, WHITE);
            plotText(OLED_WIDTH - textWidth(rightStrTuner, TOM_THUMB), OLED_HEIGHT - FONT_HEIGHT_TOMTHUMB, rightStrTuner, TOM_THUMB, WHITE);
            
            // Don't do anything when paused
            if(tunernome->pause)
            {
                plotRect((OLED_WIDTH - PAUSE_SPACE_WIDTH) / 2 - PAUSE_WIDTH,
                         (OLED_HEIGHT - PAUSE_HEIGHT) / 2,
                         (OLED_WIDTH - PAUSE_SPACE_WIDTH) / 2,
                         (OLED_HEIGHT + PAUSE_HEIGHT) / 2,
                         WHITE);
                plotRect((OLED_WIDTH + PAUSE_SPACE_WIDTH) / 2,
                         (OLED_HEIGHT - PAUSE_HEIGHT) / 2,
                         (OLED_WIDTH + PAUSE_SPACE_WIDTH) / 2 + PAUSE_WIDTH,
                         (OLED_HEIGHT + PAUSE_HEIGHT) / 2,
                         WHITE);
                return;
            }
            if(0 == tunernome->tLastUpdateUs)
            {
                // Initialize the last time this function was called
                tunernome->tLastUpdateUs = system_get_time();
            }
            else
            {
                // Get the current time and the time elapsed since the last call
                uint32_t tNowUs = system_get_time();
                uint32_t tElapsedUs = tNowUs - tunernome->tLastUpdateUs;
                // If the arm is sweeping clockwise
                if(tunernome->isClockwise)
                {
                    // Add to tAccumulatedUs
                    tunernome->tAccumulatedUs += tElapsedUs;
                    // If it's crossed the threshold for one beat
                    if(tunernome->tAccumulatedUs >= tunernome->usPerBeat)
                    {
                        // Debug
                        os_printf("Tick\n");
                        // Flip the metronome arm
                        tunernome->isClockwise = false;
                        // Start counting down by subtacting the excess time from tAccumulatedUs
                        tunernome->tAccumulatedUs = tunernome->usPerBeat - (tunernome->tAccumulatedUs - tunernome->usPerBeat);
                        // Blink LED Tick color
                        led_t leds[NUM_LIN_LEDS] = {{0}};
                            for(int i = 0; i < NUM_LIN_LEDS; i++)
                            {
                                leds[i].r = 0xFF;
                                leds[i].g = 0xFF;
                                leds[i].b = 0xFF;
                            }
                            setLeds(leds, sizeof(leds));

                            timerDisarm(&(tunernome->ledTimer));
                            timerArm(&(tunernome->ledTimer), METRONOME_FLASH_MS, false);
                    } // if(tAccumulatedUs >= tunernome->usPerBeat)
                } // if(tunernome->isClockwise)
                else
                {
                    // Subtract from tAccumulatedUs
                    tunernome->tAccumulatedUs -= tElapsedUs;
                    // If it's crossed the threshold for one beat
                    if(tunernome->tAccumulatedUs <= 0)
                    {
                        // Debug
                        os_printf("Tock\n");
                        // Flip the metronome arm
                        tunernome->isClockwise = true;
                        // Start counting up by flipping the excess time from negative to positive
                        tunernome->tAccumulatedUs = -(tunernome->tAccumulatedUs);
                        // Blink LED Tock color
                        led_t leds[NUM_LIN_LEDS] = {{0}};
                            for(int i = 0; i < NUM_LIN_LEDS; i++)
                            {
                                leds[i].r = 0xDD;
                                leds[i].g = 0xDD;
                                leds[i].b = 0x00;
                            }
                            leds[2].r = 0x00;
                            leds[2].g = 0x00;
                            leds[2].b = 0x00;
                            leds[3].r = 0x00;
                            leds[3].g = 0x00;
                            leds[3].b = 0x00;
                            setLeds(leds, sizeof(leds));

                            timerDisarm(&(tunernome->ledTimer));
                            timerArm(&(tunernome->ledTimer), METRONOME_FLASH_MS, false);
                    } // if(tunernome->tAccumulatedUs <= 0)
                } // if(!tunernome->isClockwise)
                // Draw metronome arm based on the value of tAccumulatedUs, which is between (0, usPerBeat)
                float intermedX = -1 * cosf(tunernome->tAccumulatedUs * M_PI / tunernome->usPerBeat );
                float intermedY = -1 * sinf(tunernome->tAccumulatedUs * M_PI / tunernome->usPerBeat );
                int x = round(METRONOME_CENTER_X - (intermedX * METRONOME_RADIUS));
                int y = round(METRONOME_CENTER_Y - (ABS(intermedY) * METRONOME_RADIUS));
                plotLine(METRONOME_CENTER_X, METRONOME_CENTER_Y, x, y, WHITE);
                // Set the last update time to the current time for the next call
                tunernome->tLastUpdateUs = tNowUs;
            } // if(0 != tunernome->tLastUpdateUs)
            break;
        } // case TN_METRONOME:
    } // switch(tunernome->mode)
}

/**
 * TODO
 *
 * @param state  A bitmask of all button states, unused
 * @param button The button which triggered this event
 * @param down   true if the button was pressed, false if it was released
 */
void ICACHE_FLASH_ATTR tunernomeButtonCallback( uint8_t state __attribute__((unused)),
        int button, int down)
{
    switch (tunernome->mode)
    {
        default:
        case TN_TUNER:
        {
            if(down)
            {
                switch(button)
                {
                    case UP:
                    {
                        tunernome->curTunerMode = (tunernome->curTunerMode + 1) % MAX_GUITAR_MODES;
                        break;
                    }
                    case DOWN:
                    {
                        if(0 == tunernome->curTunerMode)
                        {
                            tunernome->curTunerMode = MAX_GUITAR_MODES - 1;
                        }
                        else
                        {
                            tunernome->curTunerMode--;
                        }
                        break;
                    }
                    case ACTION:
                    {
                        cycleColorchordSensitivity();
                        break;
                    }
                    case RIGHT:
                    {
                        switchToSubmode(TN_METRONOME);
                        break;
                    }
                    case LEFT:
                    {
                        switchToSwadgeMode(0);
                        break;
                    }
                    default:
                    {
                        break;
                    }
                } // switch(button)
            } // if(down)
            break;
        } // case TN_TUNER:
        case TN_METRONOME:
        {
            os_printf("state=%d, button=%d, down=%d\n", state, button, down);

            if(down)
            {
                switch(button)
                {
                    case UP:
                    {
                        increaseBpm(NULL);
                        tunernome->lastBpmButton = button;
                        timerDisarm(&(tunernome->bpmButtonTimer));
                        timerSetFn(&(tunernome->bpmButtonTimer), fasterBpmChange, NULL);
                        timerArm(&(tunernome->bpmButtonTimer), BPM_CHANGE_FIRST_MS, true);
                        break;
                    }
                    case DOWN:
                    {
                        decreaseBpm(NULL);
                        tunernome->lastBpmButton = button;
                        timerDisarm(&(tunernome->bpmButtonTimer));
                        timerSetFn(&(tunernome->bpmButtonTimer), fasterBpmChange, NULL);
                        timerArm(&(tunernome->bpmButtonTimer), BPM_CHANGE_FIRST_MS, true);
                        break;
                    }
                    case ACTION:
                    {
                        tunernome->pause = !tunernome->pause;
                        tunernome->tAccumulatedUs = 0;
                        tunernome->tLastUpdateUs = 0;
                        break;
                    }
                    case RIGHT:
                    {
                        switchToSubmode(TN_TUNER);
                        break;
                    }
                    case LEFT:
                    {
                        switchToSwadgeMode(0);
                        break;
                    }
                    default:
                    {
                        break;
                    }
                } // switch(button)
            } // if(down)
            else
            {
                switch(button)
                {
                    case UP:
                    case DOWN:
                    {
                        if(button == tunernome->lastBpmButton)
                        {
                            tunernome->lastBpmButton = 0;
                            timerDisarm(&(tunernome->bpmButtonTimer));
                        }
                        break;
                    }
                    default:
                    {
                        break;
                    }
                }
            }
            break;
        } // case TN_METRONOME:
    }
}

/**
 * Increases the bpm by 1
 *
 * @param timer_arg unused
 */
void ICACHE_FLASH_ATTR increaseBpm(void* timer_arg __attribute__((unused)))
{
    tunernome->bpm = CLAMP(tunernome->bpm + 1, 1, MAX_BPM);
    recalcMetronome();
}

/**
 * Decreases the bpm by 1
 *
 * @param timer_arg unused
 */
void ICACHE_FLASH_ATTR decreaseBpm(void* timer_arg __attribute__((unused)))
{
    tunernome->bpm = CLAMP(tunernome->bpm - 1, 1, MAX_BPM);
    recalcMetronome();
}

/**
 * This is called every time an audio sample is read from the ADC
 * This processes the sample and will display update the LEDs every
 * 128 samples
 *
 * @param samp A 32 bit audio sample read from the ADC (microphone)
 */
void ICACHE_FLASH_ATTR tunernomeSampleHandler(int32_t samp)
{
    if(tunernome->mode == TN_TUNER)
    {
        //os_printf("%s :: %d\r\n", __func__, __LINE__);
        PushSample32( samp );
        tunernome->audioSamplesProcessed++;

        // If at least 128 samples have been processed
        if( tunernome->audioSamplesProcessed >= 128 )
        {
            // Don't bother if colorchord is inactive
            if( !COLORCHORD_ACTIVE )
            {
                return;
            }

            // Colorchord magic
            HandleFrameInfo();

            led_t colors[NUM_GUITAR_STRINGS] = {{0}};

            switch(tunernome->curTunerMode)
            {
                case GUITAR_TUNER:
                {
                    // Guitar tuner magic
                    uint32_t i;
                    for( i = 0; i < NUM_GUITAR_STRINGS; i++ )
                    {
                        // Pick out the current magnitude and filter it
                        tunernome->intensities_filt[i] = (getMagnitude(freqBinIdxsGuitar[i] + GUITAR_OFFSET)  + tunernome->intensities_filt[i]) -
                                            (tunernome->intensities_filt[i] >> 5);

                        // Pick out the difference around current magnitude and filter it too
                        tunernome->diffs_filt[i] =       (getDiffAround(freqBinIdxsGuitar[i] + GUITAR_OFFSET) + tunernome->diffs_filt[i]) -
                                            (tunernome->diffs_filt[i] >> 5);

                        // This is the magnitude of the target frequency bin, cleaned up
                        int16_t intensity = (tunernome->intensities_filt[i] >> SENSITIVITY) - 40; // drop a baseline.
                        intensity = CLAMP(intensity, 0, 255);

                        // This is the tonal difference.  You "calibrate" out the intensity.
                        int16_t tonalDiff = (tunernome->diffs_filt[i] >> SENSITIVITY) * 200 / (intensity + 1);

                        int32_t red, grn, blu;
                        // Is the note in tune, i.e. is the magnitude difference in surrounding bins small?
                        if( (ABS(tonalDiff) < 10) )
                        {
                            // Note is in tune, make it white
                            red = 255;
                            grn = 255;
                            blu = 255;
                        }
                        else
                        {
                            // Check if the note is sharp or flat
                            if( tonalDiff > 0 )
                            {
                                // Note too sharp, make it red
                                red = 255;
                                grn = blu = 255 - (tonalDiff) * 15;
                            }
                            else
                            {
                                // Note too flat, make it blue
                                blu = 255;
                                grn = red = 255 - (-tonalDiff) * 15;
                            }

                            // Make sure LED output isn't more than 255
                            red = CLAMP(red, INT_MIN, 255);
                            grn = CLAMP(grn, INT_MIN, 255);
                            blu = CLAMP(blu, INT_MIN, 255);
                        }

                        // Scale each LED's brightness by the filtered intensity for that bin
                        red = (red >> 3 ) * ( intensity >> 3);
                        grn = (grn >> 3 ) * ( intensity >> 3);
                        blu = (blu >> 3 ) * ( intensity >> 3);

                        // Set the LED, ensure each channel is between 0 and 255
                        colors[i].r = CLAMP(red, 0, 255);
                        colors[i].g = CLAMP(grn, 0, 255);
                        colors[i].b = CLAMP(blu, 0, 255);
                    }
                    break;
                } // case GUITAR_TUNER:
                case UKELELE_TUNER:
                {
                    // Guitar tuner magic, but ukelele
                    uint32_t i;
                    for( i = 0; i < NUM_UKELELE_STRINGS; i++ )
                    {
                        // Pick out the current magnitude and filter it
                        tunernome->intensities_filt[i] = (getMagnitude(freqBinIdxsUkelele[i] + GUITAR_OFFSET)  + tunernome->intensities_filt[i]) -
                                            (tunernome->intensities_filt[i] >> 5);

                        // Pick out the difference around current magnitude and filter it too
                        tunernome->diffs_filt[i] =       (getDiffAround(freqBinIdxsUkelele[i] + GUITAR_OFFSET) + tunernome->diffs_filt[i]) -
                                            (tunernome->diffs_filt[i] >> 5);

                        // This is the magnitude of the target frequency bin, cleaned up
                        int16_t intensity = (tunernome->intensities_filt[i] >> SENSITIVITY) - 40; // drop a baseline.
                        intensity = CLAMP(intensity, 0, 255);

                        // This is the tonal difference.  You "calibrate" out the intensity.
                        int16_t tonalDiff = (tunernome->diffs_filt[i] >> SENSITIVITY) * 200 / (intensity + 1);

                        int32_t red, grn, blu;
                        // Is the note in tune, i.e. is the magnitude difference in surrounding bins small?
                        if( (ABS(tonalDiff) < 10) )
                        {
                            // Note is in tune, make it white
                            red = 255;
                            grn = 255;
                            blu = 255;
                        }
                        else
                        {
                            // Check if the note is sharp or flat
                            if( tonalDiff > 0 )
                            {
                                // Note too sharp, make it red
                                red = 255;
                                grn = blu = 255 - (tonalDiff) * 15;
                            }
                            else
                            {
                                // Note too flat, make it blue
                                blu = 255;
                                grn = red = 255 - (-tonalDiff) * 15;
                            }

                            // Make sure LED output isn't more than 255
                            red = CLAMP(red, INT_MIN, 255);
                            grn = CLAMP(grn, INT_MIN, 255);
                            blu = CLAMP(blu, INT_MIN, 255);
                        }

                        // Scale each LED's brightness by the filtered intensity for that bin
                        red = (red >> 3 ) * ( intensity >> 3);
                        grn = (grn >> 3 ) * ( intensity >> 3);
                        blu = (blu >> 3 ) * ( intensity >> 3);

                        // Set the LED, ensure each channel is between 0 and 255
                        colors[ukeleleStringIdxToLedIdx[i]].r = CLAMP(red, 0, 255);
                        colors[ukeleleStringIdxToLedIdx[i]].g = CLAMP(grn, 0, 255);
                        colors[ukeleleStringIdxToLedIdx[i]].b = CLAMP(blu, 0, 255);
                    }
                    break;
                } // case UKELELE_TUNER:
                case MAX_GUITAR_MODES:
                    break;
                case SEMITONE_0:
                case SEMITONE_1:
                case SEMITONE_2:
                case SEMITONE_3:
                case SEMITONE_4:
                case SEMITONE_5:
                case SEMITONE_6:
                case SEMITONE_7:
                case SEMITONE_8:
                case SEMITONE_9:
                case SEMITONE_10:
                case SEMITONE_11:
                default:
                {
                    uint8_t semitoneIdx = (tunernome->curTunerMode - SEMITONE_0) * 2;
                    // Pick out the current magnitude and filter it
                    tunernome->semitone_intensitiy_filt = (getSemiMagnitude(semitoneIdx + CHROMATIC_OFFSET)  + tunernome->semitone_intensitiy_filt) -
                                            (tunernome->semitone_intensitiy_filt >> 5);

                    // Pick out the difference around current magnitude and filter it too
                    tunernome->semitone_diff_filt =       (getSemiDiffAround(semitoneIdx + CHROMATIC_OFFSET) + tunernome->semitone_diff_filt)       -
                                            (tunernome->semitone_diff_filt >> 5);

                    // This is the magnitude of the target frequency bin, cleaned up
                    int16_t intensity = (tunernome->semitone_intensitiy_filt >> SENSITIVITY) - 40; // drop a baseline.
                    intensity = CLAMP(intensity, 0, 255);

                    //This is the tonal difference.  You "calibrate" out the intensity.
                    int16_t tonalDiff = (tunernome->semitone_diff_filt >> SENSITIVITY) * 200 / (intensity + 1);

                    // tonal diff is -32768 to 32767. if its within -10 to 10, it's in tune. positive means too sharp, negative means too flat
                    // intensity is how 'loud' that frequency is, 0 to 255. you'll have to play around with values
                    //os_printf("thaeli, semitone %2d, tonal diff %6d, intensity %3d\r\n", tunernome->curTunerMode, tonalDiff, intensity);
                    int32_t red, grn, blu;
                    // Is the note in tune, i.e. is the magnitude difference in surrounding bins small?
                    if( (ABS(tonalDiff) < 10) )
                    {
                        // Note is in tune, make it white
                        red = 255;
                        grn = 255;
                        blu = 255;
                    }
                    else
                    {
                        // Check if the note is sharp or flat
                        if( tonalDiff > 0 )
                        {
                            // Note too sharp, make it red
                            red = 255;
                            grn = blu = 255 - (tonalDiff) * 15;
                        }
                        else
                        {
                            // Note too flat, make it blue
                            blu = 255;
                            grn = red = 255 - (-tonalDiff) * 15;
                        }

                        // Make sure LED output isn't more than 255
                        red = CLAMP(red, INT_MIN, 255);
                        grn = CLAMP(grn, INT_MIN, 255);
                        blu = CLAMP(blu, INT_MIN, 255);
                    }

                    // Scale each LED's brightness by the filtered intensity for that bin
                    red = (red >> 3 ) * ( intensity >> 3);
                    grn = (grn >> 3 ) * ( intensity >> 3);
                    blu = (blu >> 3 ) * ( intensity >> 3);

                    // Set the LED, ensure each channel is between 0 and 255
                    uint32_t i;
                    for (i = 0; i < NUM_GUITAR_STRINGS; i++)
                    {
                        colors[i].r = CLAMP(red, 0, 255);
                        colors[i].g = CLAMP(grn, 0, 255);
                        colors[i].b = CLAMP(blu, 0, 255);
                    }

                    break;
                } // default:
            }

            // Draw the LEDs
            setLeds( colors, sizeof(colors) );

            // Reset the sample count
            tunernome->audioSamplesProcessed = 0;
        }
    } // if(tunernome-> mode == TN_TUNER)
}

/**
 * This timer function is called after a metronome flash to reset the LEDs to off.
 *
 * @param timer_arg unused
 */
void ICACHE_FLASH_ATTR ledReset(void* timer_arg __attribute__((unused)))
{
    led_t leds[NUM_LIN_LEDS] = {{0}};
    setLeds(leds, sizeof(leds));
}

/**
 * This timer function is called after the up or down button is held long enough in metronome mode.
 * It repeats the button press while the button is held.
 *
 * @param timer_arg unused
 */
void ICACHE_FLASH_ATTR fasterBpmChange(void* timer_arg __attribute__((unused)))
{
    timerDisarm(&(tunernome->bpmButtonTimer));
    switch(tunernome->lastBpmButton)
    {
        case UP:
        {
            timerSetFn(&(tunernome->bpmButtonTimer), increaseBpm, NULL);
            timerArm(&(tunernome->bpmButtonTimer), BPM_CHANGE_REPEAT_MS, true);
            break;
        }
        case DOWN:
        {
            timerSetFn(&(tunernome->bpmButtonTimer), decreaseBpm, NULL);
            timerArm(&(tunernome->bpmButtonTimer), BPM_CHANGE_REPEAT_MS, true);
            break;
        }
        default:
        {
            break;
        }
    }
}
