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
#include "cndraw.h"
#include "assets.h"
#include "buttons.h"
#include "linked_list.h"
#include "font.h"
#include "mode_colorchord.h"
#include "hsv_utils.h"

#include "embeddednf.h"
#include "embeddedout.h"

/*============================================================================
 * Defines, Structs, Enums
 *==========================================================================*/

#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

#define NUM_GUITAR_STRINGS    6
#define NUM_VIOLIN_STRINGS    4
#define NUM_UKULELE_STRINGS   4
#define GUITAR_OFFSET         0
#define CHROMATIC_OFFSET      6 // adjust start point by quartertones
#define SENSITIVITY           5
#define TONAL_DIFF_IN_TUNE_DEVIATION 10

#define METRONOME_CENTER_X    OLED_WIDTH / 2
#define METRONOME_CENTER_Y    OLED_HEIGHT - 10
#define METRONOME_RADIUS      35
#define INITIAL_BPM           60
#define MAX_BPM               400
#define METRONOME_FLASH_MS    35
#define BPM_CHANGE_FIRST_MS   500
#define BPM_CHANGE_FAST_MS    2000
#define BPM_CHANGE_REPEAT_MS  50

#define US_TO_QUIT 1048576 // 2^20, makes division easy

/// Helper macro to return an integer clamped within a range (MIN to MAX)
#define CLAMP(X, MIN, MAX) ( ((X) > (MAX)) ? (MAX) : ( ((X) < (MIN)) ? (MIN) : (X)) )
/// Helper macro to return the absolute value of an integer
#define ABS(X) (((X) < 0) ? -(X) : (X))
/// Helper macro to return the highest of two integers
// #define MAX(X, Y) ( ((X) > (Y)) ? (X) : (Y) )

#define NUM_SEMITONES 12

typedef enum
{
    TN_TUNER,
    TN_METRONOME
} tnMode;

typedef enum
{
    GUITAR_TUNER = 0,
    VIOLIN_TUNER,
    UKULELE_TUNER,
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
    LISTENING,
    MAX_GUITAR_MODES
} tuner_mode_t;

typedef struct
{
    tnMode mode;
    tuner_mode_t curTunerMode;

    timer_t ledTimer;
    timer_t bpmButtonTimer;
    int lastBpmButton;
    uint32_t bpmButtonTimerUs;

    int audioSamplesProcessed;
    uint32_t intensities_filt[NUM_LIN_LEDS];
    int32_t diffs_filt[NUM_LIN_LEDS];

    uint8_t tSigIdx;
    uint8_t beatCtr;
    int bpm;
    uint32_t tLastUpdateUs;
    int32_t tAccumulatedUs;
    bool isClockwise;
    int32_t usPerBeat;

    uint32_t semitone_intensitiy_filt[NUM_SEMITONES];
    int32_t semitone_diff_filt[NUM_SEMITONES];
    int16_t tonalDiff[NUM_SEMITONES];
    int16_t intensity[NUM_SEMITONES];

    pngHandle upArrowPng;
    pngHandle flatPng;

    timer_t exitTimer;
    uint32_t exitTimeAccumulatedUs;
    uint32_t tLastCallUs;
    bool shouldExit;
} tunernome_t;

typedef struct
{
    uint8_t top;
    uint8_t bottom;
} timeSignature;

/*============================================================================
 * Prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR tunernomeEnterMode(void);
void ICACHE_FLASH_ATTR tunernomeExitMode(void);
void ICACHE_FLASH_ATTR switchToSubmode(tnMode);
void ICACHE_FLASH_ATTR tunernomeButtonCallback(uint8_t state __attribute__((unused)),
        int button, int down);
void ICACHE_FLASH_ATTR modifyBpm(int16_t bpmMod);
void ICACHE_FLASH_ATTR tunernomeSampleHandler(int32_t samp);
void ICACHE_FLASH_ATTR recalcMetronome(void);
void ICACHE_FLASH_ATTR plotInstrumentNameAndNotes(const char* instrumentName, const char** instrumentNotes,
        uint16_t numNotes);
void ICACHE_FLASH_ATTR plotTopSemiCircle(int xm, int ym, int r, color col);
void ICACHE_FLASH_ATTR instrumentTunerMagic(const uint16_t freqBinIdxs[], uint16_t numStrings, led_t colors[],
        const uint16_t stringIdxToLedIdx[]);
bool ICACHE_FLASH_ATTR tunernomeRenderTask(void);
void ICACHE_FLASH_ATTR ledReset(void* timer_arg __attribute__((unused)));
void ICACHE_FLASH_ATTR fasterBpmChange(void* timer_arg __attribute__((unused)));

static inline int16_t getMagnitude(uint16_t idx);
static inline int16_t getDiffAround(uint16_t idx);
static inline int16_t getSemiMagnitude(int16_t idx);
static inline int16_t getSemiDiffAround(uint16_t idx);
void ICACHE_FLASH_ATTR tnExitTimerFn(void* arg);

/*============================================================================
 * Variables
 *==========================================================================*/

swadgeMode tunernomeMode =
{
    .modeName = "tunernome",
    .fnEnterMode = tunernomeEnterMode,
    .fnExitMode = tunernomeExitMode,
    .fnButtonCallback = tunernomeButtonCallback,
    .fnRenderTask = tunernomeRenderTask,
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
const uint16_t freqBinIdxsViolin[NUM_VIOLIN_STRINGS] =
{
    44, // G
    58, // D
    72, // A
    86  // E
};

/**
 * Indicies into fuzzed_bins[], a realtime DFT of sorts
 * fuzzed_bins[0] = A ... 1/2 steps are every 2.
 */
const uint16_t freqBinIdxsUkulele[NUM_UKULELE_STRINGS] =
{
    68, // G
    54, // C
    62, // E
    72  // A
};

const uint16_t fourNoteStringIdxToLedIdx[4] =
{
    0,
    1,
    4,
    5
};

const char* guitarNoteNames[6] =
{
    "E2",
    "A2",
    "D3",
    "G3",
    "B3",
    "E4"
};

const char* violinNoteNames[4] =
{
    "G3",
    "D4",
    "A4",
    "E5"
};

const char* UkuleleNoteNames[4] =
{
    "G4",
    "C4",
    "E4",
    "A4"
};

// End a string with "\1" to draw the flat symbol
const char* semitoneNoteNames[NUM_SEMITONES] =
{
    "C",
    "C#/D\1",
    "D",
    "D#/E\1",
    "E",
    "F",
    "F#/G\1",
    "G",
    "G#/A\1",
    "A",
    "A#/B\1",
    "B"
};

static const char tn_title[] = "Tunernome";
static const char theWordGuitar[] = "Guitar";
static const char theWordViolin[] = "Violin";
static const char theWordUkulele[] = "Ukulele";
static const char leftStr[] = "< Exit";
static const char rightStrTuner[] = "Tuner >";
static const char rightStrMetronome[] = "Metronome >";

// TODO: these should be const after being assigned
static int TUNER_FLAT_THRES_X;
static int TUNER_SHARP_THRES_X;
static int TUNER_THRES_Y;

static const timeSignature tSigs[] =
{
    {.top = 4, .bottom = 4},
    {.top = 3, .bottom = 4},
    {.top = 2, .bottom = 4},
    {.top = 1, .bottom = 4},
    {.top = 8, .bottom = 4},
    {.top = 7, .bottom = 4},
    {.top = 6, .bottom = 4},
    {.top = 5, .bottom = 4},
};

/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * Initializer for tunernome
 */
void ICACHE_FLASH_ATTR tunernomeEnterMode(void)
{
    float intermedX = cosf(TONAL_DIFF_IN_TUNE_DEVIATION * M_PI / 17 );
    float intermedY = sinf(TONAL_DIFF_IN_TUNE_DEVIATION * M_PI / 17 );
    TUNER_SHARP_THRES_X = round(METRONOME_CENTER_X - (intermedX * METRONOME_RADIUS));
    TUNER_FLAT_THRES_X = round(METRONOME_CENTER_X + (intermedX * METRONOME_RADIUS));
    TUNER_THRES_Y = round(METRONOME_CENTER_Y - (ABS(intermedY) * METRONOME_RADIUS));

    // Alloc and clear everything
    tunernome = os_malloc(sizeof(tunernome_t));
    ets_memset(tunernome, 0, sizeof(tunernome_t));

    allocPngAsset("uparrow.png", &(tunernome->upArrowPng));
    allocPngAsset("flat.png", &(tunernome->flatPng));

    tunernome->tSigIdx = 0;
    tunernome->beatCtr = 0;
    tunernome->bpm = INITIAL_BPM;
    tunernome->curTunerMode = GUITAR_TUNER;

    switchToSubmode(TN_TUNER);

    timerDisarm(&(tunernome->ledTimer));
    timerSetFn(&(tunernome->ledTimer), ledReset, NULL);

    timerDisarm(&(tunernome->bpmButtonTimer));
    timerSetFn(&(tunernome->bpmButtonTimer), fasterBpmChange, NULL);

    enableDebounce(true);

    InitColorChord();

    tunernome->exitTimeAccumulatedUs = 0;
    tunernome->tLastCallUs = 0;
    tunernome->shouldExit = false;
    timerSetFn(&(tunernome->exitTimer), tnExitTimerFn, NULL);
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

            tunernome->isClockwise = true;
            tunernome->tSigIdx = 0;
            tunernome->beatCtr = 0;
            tunernome->tLastUpdateUs = 0;
            tunernome->tAccumulatedUs = 0;

            tunernome->lastBpmButton = 0;
            tunernome->bpmButtonTimerUs = 0;

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
    freePngAsset(&(tunernome->upArrowPng));
    freePngAsset(&(tunernome->flatPng));

    timerDisarm(&(tunernome->ledTimer));
    timerDisarm(&(tunernome->bpmButtonTimer));
    timerDisarm(&(tunernome->exitTimer));
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
void ICACHE_FLASH_ATTR recalcMetronome(void)
{
    // Figure out how many microseconds are in one beat
    tunernome->usPerBeat = (60 * 1000000) / tunernome->bpm;

}

// TODO: make this compatible with instruments with an odd number of notes
/**
 * Plot instrument name and the note names of strings, arranged to match LED positions, in middle of display
 * @param instrumentName The name of the instrument to plot to the display
 * @param instrumentNotes The note names of the strings of the instrument to plot to the display
 * @param numNotes The number of notes in instrumentsNotes
 */
void ICACHE_FLASH_ATTR plotInstrumentNameAndNotes(const char* instrumentName, const char** instrumentNotes,
        uint16_t numNotes)
{
    // Mode name
    plotText((OLED_WIDTH - textWidth(instrumentName, IBM_VGA_8)) / 2,
             (OLED_HEIGHT - FONT_HEIGHT_IBMVGA8) / 2,
             instrumentName, IBM_VGA_8, WHITE);

    // Note names of strings, arranged to match LED positions
    bool oddNumLedRows = (numNotes / 2) % 2;
    for(int i = 0; i < numNotes / 2; i++)
    {
        int y;
        if(oddNumLedRows)
        {
            y = (OLED_HEIGHT - FONT_HEIGHT_IBMVGA8) / 2 + (FONT_HEIGHT_IBMVGA8 + 5) * (1 - i);
        }
        else
        {
            y = OLED_HEIGHT / 2 + (FONT_HEIGHT_IBMVGA8 + 5) * (- i) + 2;
        }

        plotText((OLED_WIDTH - textWidth(instrumentName,
                                         IBM_VGA_8)) / 2 - textWidth(/*placeholder for widest note name + ' '*/ "G4 ", IBM_VGA_8),
                 y,
                 instrumentNotes[i], IBM_VGA_8, WHITE);
    }
    for(int i = numNotes / 2; i < numNotes; i++)
    {
        int y;
        if(oddNumLedRows)
        {
            y = (OLED_HEIGHT - FONT_HEIGHT_IBMVGA8) / 2 + (FONT_HEIGHT_IBMVGA8 + 5) * (i - (numNotes / 2) - 1);
        }
        else
        {
            y = OLED_HEIGHT / 2 + (FONT_HEIGHT_IBMVGA8 + 5) * (i - (numNotes / 2) - 1) + 2;
        }

        plotText((OLED_WIDTH + textWidth(instrumentName, IBM_VGA_8)) / 2 + textWidth(" ", IBM_VGA_8),
                 y,
                 instrumentNotes[i], IBM_VGA_8, WHITE);
    }
}

void ICACHE_FLASH_ATTR plotTopSemiCircle(int xm, int ym, int r, color col)
{
    int x = -r, y = 0, err = 2 - 2 * r; /* bottom left to top right */
    do
    {
        //drawPixel(xm - x, ym + y, col); /*   I. Quadrant +x +y */
        //drawPixel(xm - y, ym - x, col); /*  II. Quadrant -x +y */
        drawPixel(xm + x, ym - y, col);   /* III. Quadrant -x -y */
        drawPixel(xm + y, ym + x, col);   /*  IV. Quadrant +x -y */
        r = err;
        if (r <= y)
        {
            err += ++y * 2 + 1; /* e_xy+e_y < 0 */
        }
        if (r > x || err > y) /* e_xy+e_x > 0 or no 2nd y-step */
        {
            err += ++x * 2 + 1; /* -> x-step now */
        }
    } while (x < 0);
}

/**
 * Instrument-agnostic tuner magic. Updates LEDs
 * @param freqBinIdxs An array of the indices of notes for the instrument's strings. See freqBinIdxsGuitar for an example.
 * @param numStrings The number of strings on the instrument, also the number of elements in freqBinIdxs and stringIdxToLedIdx, if applicable
 * @param colors The RGB colors of the LEDs to set
 * @param stringIdxToLedIdx A remapping from each index into freqBinIdxs (same index into stringIdxToLedIdx), to the index of an LED to map that string/freqBinIdx to. Set to NULL to skip remapping.
 */
void ICACHE_FLASH_ATTR instrumentTunerMagic(const uint16_t freqBinIdxs[], uint16_t numStrings, led_t colors[],
        const uint16_t stringIdxToLedIdx[])
{
    uint32_t i;
    for( i = 0; i < numStrings; i++ )
    {
        // Pick out the current magnitude and filter it
        tunernome->intensities_filt[i] = (getMagnitude(freqBinIdxs[i] + GUITAR_OFFSET) + tunernome->intensities_filt[i]) -
                                         (tunernome->intensities_filt[i] >> 5);

        // Pick out the difference around current magnitude and filter it too
        tunernome->diffs_filt[i] = (getDiffAround(freqBinIdxs[i] + GUITAR_OFFSET) + tunernome->diffs_filt[i]) -
                                   (tunernome->diffs_filt[i] >> 5);

        // This is the magnitude of the target frequency bin, cleaned up
        int16_t intensity = (tunernome->intensities_filt[i] >> SENSITIVITY) - 40; // drop a baseline.
        intensity = CLAMP(intensity, 0, 255);

        // This is the tonal difference. You "calibrate" out the intensity.
        int16_t tonalDiff = (tunernome->diffs_filt[i] >> SENSITIVITY) * 200 / (intensity + 1);

        int32_t red, grn, blu;
        // Is the note in tune, i.e. is the magnitude difference in surrounding bins small?
        if( (ABS(tonalDiff) < TONAL_DIFF_IN_TUNE_DEVIATION) )
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
        colors[(stringIdxToLedIdx != NULL) ? stringIdxToLedIdx[i] : i].r = CLAMP(red, 0, 255);
        colors[(stringIdxToLedIdx != NULL) ? stringIdxToLedIdx[i] : i].g = CLAMP(grn, 0, 255);
        colors[(stringIdxToLedIdx != NULL) ? stringIdxToLedIdx[i] : i].b = CLAMP(blu, 0, 255);
    }
}

/**
 * This is called periodically to render the OLED image
 *
 * @return true if there is something to render, false otherwise
 */
bool ICACHE_FLASH_ATTR tunernomeRenderTask(void)
{
    // If a timer says to quit, quit
    if(true == tunernome->shouldExit)
    {
        switchToSwadgeMode(0);
        return false;
    }

    switch(tunernome->mode)
    {
        default:
        case TN_TUNER:
        {
            clearDisplay();

            // Instructions at top of display
            plotText(0, 0, "Blue=Flat", TOM_THUMB, WHITE);
            plotText((OLED_WIDTH - textWidth("White=OK", TOM_THUMB)) / 2, 0, "White=OK", TOM_THUMB, WHITE);
            plotText(OLED_WIDTH - textWidth("Red=Sharp", TOM_THUMB) + 1, 0, "Red=Sharp", TOM_THUMB, WHITE);

            // Left/Right button functions at bottom of display
            int16_t afterExit = plotText(0, OLED_HEIGHT - FONT_HEIGHT_TOMTHUMB - 1, leftStr, TOM_THUMB, WHITE);
            plotText(OLED_WIDTH - textWidth(rightStrMetronome, TOM_THUMB) + 1, OLED_HEIGHT - FONT_HEIGHT_TOMTHUMB - 1,
                     rightStrMetronome, TOM_THUMB, WHITE);

            char gainStr[16] = {0};
            ets_snprintf(gainStr, sizeof(gainStr) - 1, "Gain:%d", 1 + ((CCS.gINITIAL_AMP - AMP_OFFSET) / AMP_STEP_SIZE));
            plotText(8 + afterExit, OLED_HEIGHT - FONT_HEIGHT_TOMTHUMB - 1, gainStr, TOM_THUMB, WHITE);

            // Up/Down arrows in middle of display around current note/mode
            drawPng(&(tunernome->upArrowPng),
                    (OLED_WIDTH - tunernome->upArrowPng.width) / 2 + 1,
                    FONT_HEIGHT_TOMTHUMB + 4,
                    false, false, 0);
            drawPng(&(tunernome->upArrowPng),
                    (OLED_WIDTH - tunernome->upArrowPng.width) / 2 + 1,
                    OLED_HEIGHT - tunernome->upArrowPng.height,
                    false, true, 0);

            // Current note/mode in middle of display
            switch(tunernome->curTunerMode)
            {
                case GUITAR_TUNER:
                {
                    plotInstrumentNameAndNotes(theWordGuitar, guitarNoteNames, NUM_GUITAR_STRINGS);
                    break;
                }
                case VIOLIN_TUNER:
                {
                    plotInstrumentNameAndNotes(theWordViolin, violinNoteNames, NUM_VIOLIN_STRINGS);
                    break;
                }
                case UKULELE_TUNER:
                {
                    plotInstrumentNameAndNotes(theWordUkulele, UkuleleNoteNames, NUM_UKULELE_STRINGS);
                    break;
                }
                case LISTENING:
                {
                    // Find the note that has the highest intensity. Must be larger than 100
                    int16_t maxIntensity = 100;
                    int8_t semitoneNum = -1;
                    for(uint8_t semitone = 0; semitone < NUM_SEMITONES; semitone++)
                    {
                        if(tunernome->intensity[semitone] > maxIntensity)
                        {
                            maxIntensity = tunernome->intensity[semitone];
                            semitoneNum = semitone;
                        }
                    }

                    led_t leds[NUM_LIN_LEDS] = {{0}};

                    // If some note is intense
                    if(-1 != semitoneNum)
                    {
                        // Plot text on top of everything else
                        bool shouldDrawFlat = (semitoneNoteNames[semitoneNum][ets_strlen(semitoneNoteNames[semitoneNum]) - 1] == 1);
                        int16_t tWidth = textWidth(semitoneNoteNames[semitoneNum], IBM_VGA_8);
                        if(shouldDrawFlat)
                        {
                            tWidth += tunernome->flatPng.width + 1;
                        }
                        int16_t textEnd = plotText((OLED_WIDTH - tWidth) / 2 + 1,
                                                   (OLED_HEIGHT - FONT_HEIGHT_IBMVGA8) / 2,
                                                   semitoneNoteNames[semitoneNum], IBM_VGA_8, WHITE);

                        // Append the png for a flat
                        if(shouldDrawFlat)
                        {
                            drawPng(&tunernome->flatPng, textEnd, (OLED_HEIGHT - FONT_HEIGHT_IBMVGA8) / 2, false, false, 0);
                        }

                        // Set the LEDs to a colorchord-like value
                        uint32_t toneColor = EHSVtoHEX((semitoneNum * 256) / NUM_SEMITONES, 0xFF, 0x80);
                        for(uint8_t i = 0; i < NUM_LIN_LEDS; i++)
                        {
                            leds[i].r = (toneColor >>  0) & 0xFF;
                            leds[i].g = (toneColor >>  8) & 0xFF;
                            leds[i].b = (toneColor >> 16) & 0xFF;
                        }
                    }

                    // Set LEDs, this may turn them off
                    setLeds(leds, sizeof(leds));
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
                    // Draw tuner needle based on the value of tonalDiff, which is at most -32768 to 32767
                    // clamp it to the range -180 -> 180
                    int16_t clampedTonalDiff = CLAMP(tunernome->tonalDiff[tunernome->curTunerMode - SEMITONE_0] / 2, -180, 180);

                    // If the signal isn't intense enough, don't move the needle
                    if(tunernome->semitone_intensitiy_filt[tunernome->curTunerMode - SEMITONE_0] < 1000)
                    {
                        clampedTonalDiff = -180;
                    }

                    // Find the end point of the unit-length needle
                    float intermedX = sinf(clampedTonalDiff * M_PI / 360.0f );
                    float intermedY = cosf(clampedTonalDiff * M_PI / 360.0f );

                    // Find the actual end point of the full-length needle
                    int x = round(METRONOME_CENTER_X + (intermedX * METRONOME_RADIUS));
                    int y = round(METRONOME_CENTER_Y - (intermedY * METRONOME_RADIUS));

                    // Plot the needle
                    plotLine(METRONOME_CENTER_X, METRONOME_CENTER_Y, x, y, WHITE);
                    // Plot dashed lines indicating the 'in tune' range
                    plotDashedLine(METRONOME_CENTER_X, METRONOME_CENTER_Y, TUNER_FLAT_THRES_X, TUNER_THRES_Y, WHITE);
                    plotDashedLine(METRONOME_CENTER_X, METRONOME_CENTER_Y, TUNER_SHARP_THRES_X, TUNER_THRES_Y, WHITE);
                    // Plot a semicircle around it all
                    plotTopSemiCircle(METRONOME_CENTER_X, METRONOME_CENTER_Y, METRONOME_RADIUS, WHITE);

                    // Plot text on top of everything else
                    uint8_t semitoneNum = (tunernome->curTunerMode - SEMITONE_0);
                    bool shouldDrawFlat = (semitoneNoteNames[semitoneNum][ets_strlen(semitoneNoteNames[semitoneNum]) - 1] == 1);
                    int16_t tWidth = textWidth(semitoneNoteNames[semitoneNum], IBM_VGA_8);
                    if(shouldDrawFlat)
                    {
                        tWidth += tunernome->flatPng.width + 1;
                    }
                    fillDisplayArea((OLED_WIDTH - tWidth) / 2,
                                    (OLED_HEIGHT - FONT_HEIGHT_IBMVGA8) / 2 - 1,
                                    (OLED_WIDTH - tWidth) / 2 + tWidth,
                                    ((OLED_HEIGHT - FONT_HEIGHT_IBMVGA8) / 2) + FONT_HEIGHT_IBMVGA8,
                                    BLACK);
                    int16_t textEnd = plotText((OLED_WIDTH - tWidth) / 2 + 1,
                                               (OLED_HEIGHT - FONT_HEIGHT_IBMVGA8) / 2,
                                               semitoneNoteNames[semitoneNum], IBM_VGA_8, WHITE);

                    // Append the png for a flat
                    if(shouldDrawFlat)
                    {
                        drawPng(&tunernome->flatPng, textEnd, (OLED_HEIGHT - FONT_HEIGHT_IBMVGA8) / 2, false, false, 0);
                    }
                    break;
                }
            }

            break;
        }
        case TN_METRONOME:
        {
            clearDisplay();

            char bpmStr[32];
            ets_sprintf(bpmStr, "%d bpm, %d/%d", tunernome->bpm, tSigs[tunernome->tSigIdx].top, tSigs[tunernome->tSigIdx].bottom);

            plotText((OLED_WIDTH - textWidth(bpmStr, IBM_VGA_8)) / 2, 0, bpmStr, IBM_VGA_8, WHITE);
            plotText(0, OLED_HEIGHT - FONT_HEIGHT_TOMTHUMB - 1, leftStr, TOM_THUMB, WHITE);
            plotText(OLED_WIDTH - textWidth(rightStrTuner, TOM_THUMB), OLED_HEIGHT - FONT_HEIGHT_TOMTHUMB - 1, rightStrTuner,
                     TOM_THUMB,
                     WHITE);

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
                bool shouldBlink = false;
                // If the arm is sweeping clockwise
                if(tunernome->isClockwise)
                {
                    // Add to tAccumulatedUs
                    tunernome->tAccumulatedUs += tElapsedUs;
                    // If it's crossed the threshold for one beat
                    if(tunernome->tAccumulatedUs >= tunernome->usPerBeat)
                    {
                        // Flip the metronome arm
                        tunernome->isClockwise = false;
                        // Start counting down by subtacting the excess time from tAccumulatedUs
                        tunernome->tAccumulatedUs = tunernome->usPerBeat - (tunernome->tAccumulatedUs - tunernome->usPerBeat);
                        // Blink LED Tick color
                        shouldBlink = true;
                    } // if(tAccumulatedUs >= tunernome->usPerBeat)
                } // if(tunernome->isClockwise)
                else
                {
                    // Subtract from tAccumulatedUs
                    tunernome->tAccumulatedUs -= tElapsedUs;
                    // If it's crossed the threshold for one beat
                    if(tunernome->tAccumulatedUs <= 0)
                    {
                        // Flip the metronome arm
                        tunernome->isClockwise = true;
                        // Start counting up by flipping the excess time from negative to positive
                        tunernome->tAccumulatedUs = -(tunernome->tAccumulatedUs);
                        // Blink LED Tock color
                        shouldBlink = true;
                    } // if(tunernome->tAccumulatedUs <= 0)
                } // if(!tunernome->isClockwise)

                if(shouldBlink)
                {
                    tunernome->beatCtr = (tunernome->beatCtr + 1) % tSigs[tunernome->tSigIdx].top;

                    led_t leds[NUM_LIN_LEDS] = {{0}};

                    if(0 == tunernome->beatCtr)
                    {
                        for(int i = 0; i < NUM_LIN_LEDS; i++)
                        {
                            leds[i].r = 0x40;
                            leds[i].g = 0xFF;
                            leds[i].b = 0x00;
                        }
                    }
                    else
                    {
                        for(int i = 0; i < NUM_LIN_LEDS; i++)
                        {
                            leds[i].r = 0x40;
                            leds[i].g = 0x00;
                            leds[i].b = 0xFF;
                        }
                        leds[2].r = 0x00;
                        leds[2].g = 0x00;
                        leds[2].b = 0x00;
                        leds[3].r = 0x00;
                        leds[3].g = 0x00;
                        leds[3].b = 0x00;
                    }

                    setLeds(leds, sizeof(leds));
                    timerDisarm(&(tunernome->ledTimer));
                    timerArm(&(tunernome->ledTimer), METRONOME_FLASH_MS, false);
                }

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

    // If the quit button is being held
    if(tunernome->exitTimeAccumulatedUs > 0)
    {
        // Draw a bar
        plotLine(0, OLED_HEIGHT - 1, (OLED_WIDTH * tunernome->exitTimeAccumulatedUs) / US_TO_QUIT, OLED_HEIGHT - 1, WHITE);
    }
    return true;
}

/**
 * TODO
 *
 * @param state A bitmask of all button states, unused
 * @param button The button which triggered this event
 * @param down true if the button was pressed, false if it was released
 */
void ICACHE_FLASH_ATTR tunernomeButtonCallback( uint8_t state __attribute__((unused)),
        int button, int down)
{
    if(LEFT == button)
    {
        if(down)
        {
            // Start the timer to exit
            timerArm(&(tunernome->exitTimer), 1, true);
        }
        else
        {
            // Stop the timer to exit
            tunernome->exitTimeAccumulatedUs = 0;
            tunernome->tLastCallUs = 0;
            timerDisarm(&(tunernome->exitTimer));
        }
        return;
    }

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
                        // Handled above
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
            if(down)
            {
                switch(button)
                {
                    case UP:
                    {
                        modifyBpm(1);
                        tunernome->lastBpmButton = button;
                        tunernome->bpmButtonTimerUs = 0;
                        timerArm(&(tunernome->bpmButtonTimer), BPM_CHANGE_REPEAT_MS, true);
                        break;
                    }
                    case DOWN:
                    {
                        modifyBpm(-1);
                        tunernome->lastBpmButton = button;
                        tunernome->bpmButtonTimerUs = 0;
                        timerArm(&(tunernome->bpmButtonTimer), BPM_CHANGE_REPEAT_MS, true);
                        break;
                    }
                    case ACTION:
                    {
                        // Cycle the time signature
                        tunernome->tSigIdx = (tunernome->tSigIdx + 1) % (lengthof(tSigs));
                        break;
                    }
                    case RIGHT:
                    {
                        switchToSubmode(TN_TUNER);
                        break;
                    }
                    case LEFT:
                    {
                        // Handled above
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
                            tunernome->bpmButtonTimerUs = 0;
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
 * This timer function is called periodically when the button to exit is held down
 * When the accumulated time hits US_TO_QUIT, the mode is quit
 *
 * @param arg unused
 */
void ICACHE_FLASH_ATTR tnExitTimerFn(void* arg __attribute__((unused)))
{
    if(0 == tunernome->tLastCallUs)
    {
        tunernome->tLastCallUs = system_get_time();
    }
    else
    {
        uint32_t tNowUs = system_get_time();
        tunernome->exitTimeAccumulatedUs += (tNowUs - tunernome->tLastCallUs);
        tunernome->tLastCallUs = tNowUs;

        if(tunernome->exitTimeAccumulatedUs > US_TO_QUIT)
        {
            tunernome->shouldExit = true;
        }
    }
}

/**
 * Increases the bpm by bpmMod, may be negative
 *
 * @param bpmMod The amount to change the BPM
 */
void ICACHE_FLASH_ATTR modifyBpm(int16_t bpmMod)
{
    tunernome->bpm = CLAMP(tunernome->bpm + bpmMod, 1, MAX_BPM);
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

            led_t colors[NUM_LIN_LEDS] = {{0}};

            switch(tunernome->curTunerMode)
            {
                case GUITAR_TUNER:
                {
                    instrumentTunerMagic(freqBinIdxsGuitar, NUM_GUITAR_STRINGS, colors, NULL);
                    break;
                }
                case VIOLIN_TUNER:
                {
                    instrumentTunerMagic(freqBinIdxsViolin, NUM_VIOLIN_STRINGS, colors, fourNoteStringIdxToLedIdx);
                    break;
                }
                case UKULELE_TUNER:
                {
                    instrumentTunerMagic(freqBinIdxsUkulele, NUM_UKULELE_STRINGS, colors, fourNoteStringIdxToLedIdx);
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
                case LISTENING:
                default:
                {
                    for(uint8_t semitone = 0; semitone < NUM_SEMITONES; semitone++)
                    {
                        // uint8_t semitoneIdx = (tunernome->curTunerMode - SEMITONE_0) * 2;
                        uint8_t semitoneIdx = semitone * 2;
                        // Pick out the current magnitude and filter it
                        tunernome->semitone_intensitiy_filt[semitone] = (getSemiMagnitude(semitoneIdx + CHROMATIC_OFFSET) +
                                tunernome->semitone_intensitiy_filt[semitone]) -
                                (tunernome->semitone_intensitiy_filt[semitone] >> 5);

                        // Pick out the difference around current magnitude and filter it too
                        tunernome->semitone_diff_filt[semitone] = (getSemiDiffAround(semitoneIdx + CHROMATIC_OFFSET) +
                                tunernome->semitone_diff_filt[semitone]) -
                                (tunernome->semitone_diff_filt[semitone] >> 5);


                        // This is the magnitude of the target frequency bin, cleaned up
                        tunernome->intensity[semitone] = (tunernome->semitone_intensitiy_filt[semitone] >> SENSITIVITY) -
                                                         40; // drop a baseline.
                        tunernome->intensity[semitone] = CLAMP(tunernome->intensity[semitone], 0, 255);

                        //This is the tonal difference. You "calibrate" out the intensity.
                        tunernome->tonalDiff[semitone] = (tunernome->semitone_diff_filt[semitone] >> SENSITIVITY) * 200 /
                                                         (tunernome->intensity[semitone] + 1);
                    }

                    // tonal diff is -32768 to 32767. if its within -10 to 10 (now defined as TONAL_DIFF_IN_TUNE_DEVIATION), it's in tune.
                    // positive means too sharp, negative means too flat
                    // intensity is how 'loud' that frequency is, 0 to 255. you'll have to play around with values
                    int32_t red, grn, blu;
                    // Is the note in tune, i.e. is the magnitude difference in surrounding bins small?
                    if( (ABS(tunernome->tonalDiff[tunernome->curTunerMode - SEMITONE_0]) < TONAL_DIFF_IN_TUNE_DEVIATION) )
                    {
                        // Note is in tune, make it white
                        red = 255;
                        grn = 255;
                        blu = 255;
                    }
                    else
                    {
                        // Check if the note is sharp or flat
                        if( tunernome->tonalDiff[tunernome->curTunerMode - SEMITONE_0] > 0 )
                        {
                            // Note too sharp, make it red
                            red = 255;
                            grn = blu = 255 - (tunernome->tonalDiff[tunernome->curTunerMode - SEMITONE_0] - TONAL_DIFF_IN_TUNE_DEVIATION) * 15;
                        }
                        else
                        {
                            // Note too flat, make it blue
                            blu = 255;
                            grn = red = 255 - (-(tunernome->tonalDiff[tunernome->curTunerMode - SEMITONE_0] + TONAL_DIFF_IN_TUNE_DEVIATION)) * 15;
                        }

                        // Make sure LED output isn't more than 255
                        red = CLAMP(red, INT_MIN, 255);
                        grn = CLAMP(grn, INT_MIN, 255);
                        blu = CLAMP(blu, INT_MIN, 255);
                    }

                    // Scale each LED's brightness by the filtered intensity for that bin
                    red = (red >> 3 ) * ( tunernome->intensity[tunernome->curTunerMode - SEMITONE_0] >> 3);
                    grn = (grn >> 3 ) * ( tunernome->intensity[tunernome->curTunerMode - SEMITONE_0] >> 3);
                    blu = (blu >> 3 ) * ( tunernome->intensity[tunernome->curTunerMode - SEMITONE_0] >> 3);

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
            } // switch(tunernome->curTunerMode)

            if(LISTENING != tunernome->curTunerMode)
            {
                // Draw the LEDs
                setLeds( colors, sizeof(colors) );
            }
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
 * Called every 50ms (BPM_CHANGE_REPEAT_MS)
 *
 * @param timer_arg unused
 */
void ICACHE_FLASH_ATTR fasterBpmChange(void* timer_arg __attribute__((unused)))
{
    tunernome->bpmButtonTimerUs += BPM_CHANGE_REPEAT_MS;

    if(tunernome->bpmButtonTimerUs >= BPM_CHANGE_FIRST_MS)
    {
        int16_t mod = 1;
        if(tunernome->bpmButtonTimerUs >= BPM_CHANGE_FAST_MS)
        {
            mod = 3;
        }
        switch(tunernome->lastBpmButton)
        {
            case UP:
            {
                modifyBpm(mod);
                break;
            }
            case DOWN:
            {
                modifyBpm(-mod);
                break;
            }
            default:
            {
                break;
            }
        }
    }
}
