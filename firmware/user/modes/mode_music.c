/*==============================================================================
 * Includes
 *============================================================================*/

#include <osapi.h>
#include <math.h>
#include <user_interface.h>

#include "user_main.h"
#include "mode_music.h"

#include "hpatimer.h"
#include "buzzer.h"
#include "custom_commands.h"

#include "oled.h"
#include "bresenham.h"
#include "font.h"

#include "embeddedout.h"

#include "buttons.h"

#include "mode_tiltrads.h"

/*==============================================================================
 * Defines
 *============================================================================*/

#define TICK_HEIGHT 2
#define CURSOR_HEIGHT 4
#define BAR_X_MARGIN 0
#define BAR_Y_MARGIN (FONT_HEIGHT_TOMTHUMB + CURSOR_HEIGHT + 1)

#define PITCH_THRESHOLD 32

#define lengthof(x) (sizeof(x) / sizeof(x[0]))

#define REST_BIT 0x10000 // Largest note is 144, which is 0b10010000

#define BPM_MULTIPLIER 21

/*==============================================================================
 * Enums
 *============================================================================*/

typedef enum
{
    THIRTYSECOND_NOTE = 3,
    THIRTYSECOND_REST = 3 | REST_BIT,
    SIXTEENTH_NOTE    = 6,
    SIXTEENTH_REST    = 6 | REST_BIT,
    EIGHTH_NOTE       = 12,
    EIGHTH_REST       = 12 | REST_BIT,
    QUARTER_NOTE      = 24,
    QUARTER_REST      = 24 | REST_BIT,
    HALF_NOTE         = 48,
    HALF_REST         = 48 | REST_BIT,
    WHOLE_NOTE        = 96,
    WHOLE_REST        = 96 | REST_BIT,

    TRIPLET_SIXTYFOURTH_NOTE  = 1,
    TRIPLET_SIXTYFOURTH_REST  = 1 | REST_BIT,
    TRIPLET_THIRTYSECOND_NOTE = 2,
    TRIPLET_THIRTYSECOND_REST = 2 | REST_BIT,
    TRIPLET_SIXTEENTH_NOTE    = 4,
    TRIPLET_SIXTEENTH_REST    = 4 | REST_BIT,
    TRIPLET_EIGHTH_NOTE       = 8,
    TRIPLET_EIGHTH_REST       = 8 | REST_BIT,
    TRIPLET_QUARTER_NOTE      = 16,
    TRIPLET_QUARTER_REST      = 16 | REST_BIT,
    TRIPLET_HALF_NOTE         = 32,
    TRIPLET_HALF_REST         = 32 | REST_BIT,
    TRIPLET_WHOLE_NOTE        = 64,
    TRIPLET_WHOLE_REST        = 64 | REST_BIT,

    DOTTED_SIXTEENTH_NOTE = 9,
    DOTTED_SIXTEENTH_REST = 9 | REST_BIT,
    DOTTED_EIGHTH_NOTE    = 18,
    DOTTED_EIGHTH_REST    = 18 | REST_BIT,
    DOTTED_QUARTER_NOTE   = 36,
    DOTTED_QUARTER_REST   = 36 | REST_BIT,
    DOTTED_HALF_NOTE      = 72,
    DOTTED_HALF_REST      = 72 | REST_BIT,
    DOTTED_WHOLE_NOTE     = 144,
    DOTTED_WHOLE_REST     = 144 | REST_BIT,
} rhythmNote_t;

/*==============================================================================
 * Prototypes
 *============================================================================*/

void ICACHE_FLASH_ATTR musicEnterMode(void);
void ICACHE_FLASH_ATTR musicExitMode(void);
void ICACHE_FLASH_ATTR musicButtonCallback(uint8_t state __attribute__((unused)),
        int button, int down);
void ICACHE_FLASH_ATTR musicAccelerometerHandler(accel_t* accel);
void ICACHE_FLASH_ATTR musicUpdateDisplay(void);
void ICACHE_FLASH_ATTR musicBeatTimerFunc(void* arg __attribute__((unused)));
notePeriod_t ICACHE_FLASH_ATTR arpModify(notePeriod_t note, uint8_t arpInterval);
notePeriod_t ICACHE_FLASH_ATTR getCurrentNote(void);
char* ICACHE_FLASH_ATTR noteToStr(notePeriod_t note);
void ICACHE_FLASH_ATTR plotBar(uint8_t yOffset);
void ICACHE_FLASH_ATTR noteToColor( led_t* led, notePeriod_t note, uint8_t brightness);

/*==============================================================================
 * Structs
 *============================================================================*/

// A collection of parameters
typedef struct
{
    // The parameter's name
    char* name;
    // The notes
    const notePeriod_t* notes;
    const uint16_t notesLen;
    // The rhythm
    const rhythmNote_t* rhythm;
    const uint16_t rhythmLen;
    // The arpeggios
    const uint8_t* arpIntervals;
    const uint16_t arpLen;
    // Inter-note pause
    const uint16_t interNotePauseMs;
} swynthParam_t;

/*==============================================================================
 * Variables
 *============================================================================*/

// The swadge mode
swadgeMode musicMode =
{
    .modeName = "music",
    .fnEnterMode = musicEnterMode,
    .fnExitMode = musicExitMode,
    .fnButtonCallback = musicButtonCallback,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = musicAccelerometerHandler,
    .menuImageData = mnu_music_0,
    .menuImageLen = sizeof(mnu_music_0)
};

// The state data
struct
{
    // Track motion
    int16_t roll;
    int16_t pitch;

    // Track rhythm
    os_timer_t beatTimer;
    uint32_t timeUs;
    uint16_t rhythmIdx;
    uint8_t arpIdx;

    // Track the button
    bool shouldPlay;
    uint8_t paramIdx;
} music;

/*==============================================================================
 * Const Variables
 *============================================================================*/

// All the rhythms
const rhythmNote_t triplet64[] = {TRIPLET_SIXTYFOURTH_NOTE};
const rhythmNote_t quarterNotes[] = {QUARTER_NOTE, QUARTER_REST};
const rhythmNote_t triplets[] = {TRIPLET_EIGHTH_NOTE, TRIPLET_EIGHTH_NOTE, TRIPLET_EIGHTH_NOTE, TRIPLET_EIGHTH_REST};

// All the scales
const notePeriod_t scl_M_Penta[] = {C_5, D_5, E_5, G_5, A_5, C_6, C_6, D_6, E_6, G_6, A_6, C_7, };
const notePeriod_t scl_m_Penta[] = {C_5, D_SHARP_5, F_5, G_5, A_SHARP_5, C_6, C_6, D_SHARP_6, F_6, G_6, A_SHARP_6, C_7, };
const notePeriod_t scl_m_Blues[] = {C_5, D_SHARP_5, F_5, F_SHARP_5, G_5, A_SHARP_5, C_6, C_6, D_SHARP_6, F_6, F_SHARP_6, G_6, A_SHARP_6, C_7, };
const notePeriod_t scl_M_Blues[] = {C_5, D_5, D_SHARP_5, E_5, G_5, A_5, C_6, C_6, D_6, D_SHARP_6, E_6, G_6, A_6, C_7, };
const notePeriod_t scl_Major[] = {C_5, D_5, E_5, F_5, G_5, A_5, B_5, C_6, C_6, D_6, E_6, F_6, G_6, A_6, B_6, C_7, };
const notePeriod_t scl_Minor_Aeolian[] = {C_5, D_5, D_SHARP_5, F_5, G_5, G_SHARP_5, A_SHARP_5, C_6, C_6, D_6, D_SHARP_6, F_6, G_6, G_SHARP_6, A_SHARP_6, C_7, };
const notePeriod_t scl_Harm_Minor[] = {C_5, D_5, D_SHARP_5, F_5, G_5, G_SHARP_5, B_5, C_6, C_6, D_6, D_SHARP_6, F_6, G_6, G_SHARP_6, B_6, C_7, };
const notePeriod_t scl_Dorian[] = {C_5, D_5, D_SHARP_5, F_5, G_5, A_5, A_SHARP_5, C_6, C_6, D_6, D_SHARP_6, F_6, G_6, A_6, A_SHARP_6, C_7, };
const notePeriod_t scl_Phrygian[] = {C_5, C_SHARP_5, D_SHARP_5, F_5, G_5, G_SHARP_5, A_SHARP_5, C_6, C_6, C_SHARP_6, D_SHARP_6, F_6, G_6, G_SHARP_6, A_SHARP_6, C_7, };
const notePeriod_t scl_Lydian[] = {C_5, D_5, E_5, F_SHARP_5, G_5, A_5, B_5, C_6, C_6, D_6, E_6, F_SHARP_6, G_6, A_6, B_6, C_7, };
const notePeriod_t scl_Mixolydian[] = {C_5, D_5, E_5, F_5, G_5, A_5, A_SHARP_5, C_6, C_6, D_6, E_6, F_6, G_6, A_6, A_SHARP_6, C_7, };
const notePeriod_t scl_Locrian[] = {C_5, C_SHARP_5, D_SHARP_5, F_5, F_SHARP_5, G_SHARP_5, A_SHARP_5, C_6, C_6, C_SHARP_6, D_SHARP_6, F_6, F_SHARP_6, G_SHARP_6, A_SHARP_6, C_7, };
const notePeriod_t scl_Dom_Bebop[] = {C_5, D_5, E_5, F_5, G_5, A_5, A_SHARP_5, B_5, C_6, C_6, D_6, E_6, F_6, G_6, A_6, A_SHARP_6, B_6, C_7, };
const notePeriod_t scl_M_Bebop[] = {C_5, D_5, E_5, F_5, G_5, G_SHARP_5, A_SHARP_5, B_5, C_6, C_6, D_6, E_6, F_6, G_6, G_SHARP_6, A_SHARP_6, B_6, C_7, };
const notePeriod_t scl_Whole_Tone[] = {C_5, D_5, E_5, F_SHARP_5, G_SHARP_5, A_SHARP_5, C_6, C_6, D_6, E_6, F_SHARP_6, G_SHARP_6, A_SHARP_6, C_7, };
const notePeriod_t scl_Chromatic[] = {C_5, C_SHARP_5, D_5, D_SHARP_5, E_5, F_5, F_SHARP_5, G_5, G_SHARP_5, A_5, A_SHARP_5, B_5, C_6, C_6, C_SHARP_6, D_6, D_SHARP_6, E_6, F_6, F_SHARP_6, G_6, G_SHARP_6, A_6, A_SHARP_6, B_6, C_7, };

// All the arpeggios
const uint8_t arp_M_Triad[] = {1, 5, 8};
const uint8_t arp_m_Triad[] = {1, 4, 8};
const uint8_t arp_M7[] = {1, 5, 8, 11};
const uint8_t arp_m7[] = {1, 4, 8, 10};
const uint8_t arp_Dom7[] = {1, 5, 8, 10};
const uint8_t arp_Octave[] = {1, 13};
const uint8_t arp_Fifth[] = {1, 8};
const uint8_t arp_Dim[] = {1, 4, 7};
const uint8_t arp_Dim7[] = {1, 4, 7, 10};
const uint8_t arp_M7_add_9[] = {1, 5, 8, 12, 15};
const uint8_t arp_Sans[] = {1, 1, 13, 8, 7, 6, 4, 1, 4, 6};

const swynthParam_t swynthParams[] =
{
    {
        .name = "Test",
        .notes = scl_M_Penta,
        .notesLen = lengthof(scl_M_Penta),
        .rhythm = quarterNotes,
        .rhythmLen = lengthof(quarterNotes),
        .arpIntervals = NULL,
        .arpLen = 0,
        .interNotePauseMs = 5
    },
    {
        .name = "ArpTest",
        .notes = scl_M_Penta,
        .notesLen = lengthof(scl_M_Penta),
        .rhythm = triplets,
        .rhythmLen = lengthof(triplets),
        .arpIntervals = arp_M_Triad,
        .arpLen = lengthof(arp_M_Triad),
        .interNotePauseMs = 5
    },
    {
        .name = "Slide Whistl",
        .notes = scl_Chromatic,
        .notesLen = lengthof(scl_Chromatic),
        .rhythm = triplet64,
        .rhythmLen = lengthof(triplet64),
        .arpIntervals = NULL,
        .arpLen = 0,
        .interNotePauseMs = 0
    },
};

const notePeriod_t allNotes[] =
{
    C_5,
    C_SHARP_5,
    D_5,
    D_SHARP_5,
    E_5,
    F_5,
    F_SHARP_5,
    G_5,
    G_SHARP_5,
    A_5,
    A_SHARP_5,
    B_5,

    C_6,
    C_SHARP_6,
    D_6,
    D_SHARP_6,
    E_6,
    F_6,
    F_SHARP_6,
    G_6,
    G_SHARP_6,
    A_6,
    A_SHARP_6,
    B_6,

    C_7,
    C_SHARP_7,
    D_7,
    D_SHARP_7,
    E_7,
    F_7,
    F_SHARP_7,
    G_7,
    G_SHARP_7,
    A_7,
    A_SHARP_7,
    B_7,

    C_8,
    C_SHARP_8,
    D_8,
    D_SHARP_8,
    E_8,
    F_8,
    F_SHARP_8,
    G_8,
    G_SHARP_8,
    A_8,
    A_SHARP_8,
    B_8,
};

/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * Initializer for music
 */
void ICACHE_FLASH_ATTR musicEnterMode(void)
{
    // If the swadge is muted
    if(getIsMutedOption())
    {
        // Unmute it and init the buzzer
        setMuteOverride(true);
        initBuzzer();
        setBuzzerNote(SILENCE);
    }

    // Clear everything
    memset(&music, 0, sizeof(music));

    // Set a timer to tick every 1ms, forever
    os_timer_disarm(&music.beatTimer);
    os_timer_setfn(&music.beatTimer, musicBeatTimerFunc, NULL);
    os_timer_arm(&music.beatTimer, 1, true);

    // Draw an initial display
    musicUpdateDisplay();

    // Request to do everything faster
    setAccelPollTime(50);
    setOledDrawTime(50);
    enableDebounce(false);
}

/**
 * Called when music is exited
 */
void ICACHE_FLASH_ATTR musicExitMode(void)
{
    os_timer_disarm(&music.beatTimer);
}

/**
 * @brief Button callback function, plays notes and switches parameters
 *
 * @param state  A bitmask of all button states, unused
 * @param button The button which triggered this event
 * @param down   true if the button was pressed, false if it was released
 */
void ICACHE_FLASH_ATTR musicButtonCallback(
    uint8_t state __attribute__((unused)), int button, int down)
{
    switch(button)
    {
        case 1:
        {
            // Left
            if(down)
            {
                // cycle params
                music.paramIdx = (music.paramIdx + 1) % lengthof(swynthParams);
                music.timeUs = 0;
                music.rhythmIdx = 0;
                music.arpIdx = 0;
                musicUpdateDisplay();
            }
            break;
        }
        case 2:
        {
            // Right, track whether a note should be played or not
            music.shouldPlay = down;
            break;
        }
        default:
        {
            break;
        }
    }
}

/**
 * @brief Callback function for accelerometer values
 * Use the current vector to find pitch and roll, then update the display
 *
 * @param accel The freshly read accelerometer values
 */
void ICACHE_FLASH_ATTR musicAccelerometerHandler(accel_t* accel)
{
    // Only find values when the swadge is pointed up
    if(accel-> x < 0)
    {
        return;
    }

    // Find the roll and pitch in radians
    float rollF = atanf(accel->y / (float)accel->x);
    float pitchF = atanf((-1 * accel->z) / sqrtf((accel->y * accel->y) + (accel->x * accel->x)));

    // Normalize the values to [0,1]
    rollF = ((rollF) / M_PI) + 0.5f;
    pitchF = ((pitchF) / M_PI) + 0.5f;

    // Round and scale to OLED_WIDTH
    music.roll = roundf(rollF * OLED_WIDTH);
    if(music.roll >= OLED_WIDTH)
    {
        music.roll = OLED_WIDTH - 1;
    }
    music.pitch = roundf(pitchF * OLED_WIDTH);
    if(music.pitch >= OLED_WIDTH)
    {
        music.pitch = OLED_WIDTH - 1;
    }

    // os_printf("roll %6d pitch %6d, x %4d, y %4d, z %4d, \n",
    // music.roll, music.pitch,
    // accel->x, accel->y, accel->z);

    musicUpdateDisplay();
}

/**
 * Update the display by drawing the current state of affairs
 */
void ICACHE_FLASH_ATTR musicUpdateDisplay(void)
{
    clearDisplay();

    // Plot the bars
    plotBar(OLED_HEIGHT - BAR_Y_MARGIN - 1);
    plotBar(OLED_HEIGHT - BAR_Y_MARGIN - (2 * CURSOR_HEIGHT + 5));

    if(music.pitch < PITCH_THRESHOLD)
    {
        // Plot the cursor
        plotLine(music.roll, OLED_HEIGHT - BAR_Y_MARGIN - (2 * CURSOR_HEIGHT + 5) - CURSOR_HEIGHT,
                 music.roll, OLED_HEIGHT - BAR_Y_MARGIN - (2 * CURSOR_HEIGHT + 5) + CURSOR_HEIGHT,
                 WHITE);
    }
    else
    {
        // Plot the cursor
        plotLine(music.roll, OLED_HEIGHT - BAR_Y_MARGIN - 1 - CURSOR_HEIGHT,
                 music.roll, OLED_HEIGHT - BAR_Y_MARGIN - 1 + CURSOR_HEIGHT,
                 WHITE);
    }

    // Plot the title
    plotText(0, 0, swynthParams[music.paramIdx].name, RADIOSTARS, WHITE);

    // Plot the note
    plotCenteredText(0, 20, OLED_WIDTH - 1, noteToStr(getCurrentNote()), RADIOSTARS, WHITE);

    // Plot the button funcs
    plotText(0, OLED_HEIGHT - FONT_HEIGHT_TOMTHUMB, "Choose", TOM_THUMB, WHITE);
    plotText(OLED_WIDTH - 15, OLED_HEIGHT - FONT_HEIGHT_TOMTHUMB, "Play", TOM_THUMB, WHITE);
}

/**
 * @brief Plot a horizontal bar indicating where the note boundaries are
 *
 * @param yOffset The Y Offset of the middle of the bar, not the ticks
 */
void ICACHE_FLASH_ATTR plotBar(uint8_t yOffset)
{
    // Plot the main bar
    plotLine(
        BAR_X_MARGIN,
        yOffset,
        OLED_WIDTH - BAR_X_MARGIN,
        yOffset,
        WHITE);

    // Plot tick marks at each of the note boundaries
    uint8_t tick;
    for(tick = 0; tick < (swynthParams[music.paramIdx].notesLen / 2) + 1; tick++)
    {
        uint8_t x = BAR_X_MARGIN + ( (tick * ((OLED_WIDTH - 1) - (BAR_X_MARGIN * 2))) /
                                     (swynthParams[music.paramIdx].notesLen / 2)) ;
        plotLine(x, yOffset - TICK_HEIGHT,
                 x, yOffset + TICK_HEIGHT,
                 WHITE);
    }
}

/**
 * @brief Called every 1ms to handle the rhythm, arpeggios, and setting the buzzer
 *
 * @param arg unused
 */
void ICACHE_FLASH_ATTR musicBeatTimerFunc(void* arg __attribute__((unused)))
{
    // Keep track of time with microsecond precision
    static int32_t lastCallTimeUs = 0;
    if(0 == lastCallTimeUs)
    {
        // Just initialize lastCallTimeUs
        lastCallTimeUs = system_get_time();
        return;
    }

    // Figure out the delta between calls in microseconds, increment time
    int32_t currentCallTimeUs = system_get_time();
    music.timeUs += (currentCallTimeUs - lastCallTimeUs);
    lastCallTimeUs = currentCallTimeUs;

    // If time crossed a rhythm boundary, do something different
    uint32_t rhythmIntervalUs = (1000 * BPM_MULTIPLIER * ((~REST_BIT) &
                                 swynthParams[music.paramIdx].rhythm[music.rhythmIdx]));
    led_t leds[NUM_LIN_LEDS] = {{0}};
    if(music.timeUs >= rhythmIntervalUs)
    {
        // Reset the time
        music.timeUs -= rhythmIntervalUs;
        // Move to the next rhythm element
        music.rhythmIdx = (music.rhythmIdx + 1) % swynthParams[music.paramIdx].rhythmLen;

        // See if the note should be arpeggiated. Do this even for unplayed notes
        if(NULL != swynthParams[music.paramIdx].arpIntervals &&
                !(swynthParams[music.paramIdx].rhythm[music.rhythmIdx] & REST_BIT))
        {
            // Track the current arpeggio index
            music.arpIdx = (music.arpIdx + 1) % swynthParams[music.paramIdx].arpLen;
        }

        // See if we should actually play the note or not
        if(!music.shouldPlay || (swynthParams[music.paramIdx].rhythm[music.rhythmIdx] & REST_BIT))
        {
            setBuzzerNote(SILENCE);
            noteToColor(&leds[0], getCurrentNote(), 0x10);
        }
        else
        {
            notePeriod_t noteToPlay = getCurrentNote();
            if(NULL != swynthParams[music.paramIdx].arpIntervals)
            {
                // This mode has an arpeggio, so modify the current note
                noteToPlay = arpModify(
                                 noteToPlay,
                                 swynthParams[music.paramIdx].arpIntervals[music.arpIdx]);
            }
            setBuzzerNote(noteToPlay);
            noteToColor(&leds[0], getCurrentNote(), 0x40);
        }
    }
    else if(music.timeUs >= rhythmIntervalUs - (1000 * swynthParams[music.paramIdx].interNotePauseMs))
    {
        setBuzzerNote(SILENCE);
        noteToColor(&leds[0], getCurrentNote(), 0x10);
    }
    else
    {
        // Don't set LEDs if nothing changed
        return;
    }

    // Copy LED color from the first LED to all of them
    for(uint8_t ledIdx = 1; ledIdx < NUM_LIN_LEDS; ledIdx++)
    {
        leds[ledIdx].r = leds[0].r;
        leds[ledIdx].g = leds[0].g;
        leds[ledIdx].b = leds[0].b;
    }
    setLeds(leds, sizeof(leds));
}

/**
 * @return the current note the angle coresponds to. doesn't matter if it should
 * be played right now or not
 */
notePeriod_t ICACHE_FLASH_ATTR getCurrentNote(void)
{
    // Get the index of the note to play based on roll
    uint8_t noteIdx = (music.roll * (swynthParams[music.paramIdx].notesLen / 2)) / OLED_WIDTH;
    // See if we should play the higher note
    if(music.pitch < PITCH_THRESHOLD)
    {
        uint8_t offset = swynthParams[music.paramIdx].notesLen / 2;
        return swynthParams[music.paramIdx].notes[noteIdx + offset];
    }
    else
    {
        return swynthParams[music.paramIdx].notes[noteIdx];
    }
}

/**
 * @brief Arpeggiate a note
 *
 * @param note The root note to arpeggiate
 * @param arpInterval The interval to arpeggiate it by
 * @return notePeriod_t The arpeggiated note
 */
notePeriod_t ICACHE_FLASH_ATTR arpModify(notePeriod_t note, uint8_t arpInterval)
{
    // First find the note in the list of all notes
    for(uint16_t i = 0; i < lengthof(allNotes); i++)
    {
        if(note == allNotes[i])
        {
            // Then shift up by arpInterval
            while(--arpInterval)
            {
                i++;
            }
            // Then return the arpeggiated note
            return allNotes[i];
        }
    }
    return note;
}

/**
 * @brief Translate a musical note to a color
 *
 * @param led The led_t to write the color data to
 * @param note The note to translate to color
 * @param brightness The brightness of the LED
 */
void ICACHE_FLASH_ATTR noteToColor( led_t* led, notePeriod_t note, uint8_t brightness)
{
    // First find the note in the list of all notes
    for(uint16_t idx = 0; idx < lengthof(allNotes); idx++)
    {
        if(note == allNotes[idx])
        {
            idx = idx % 12;
            idx = (idx * 255) / 12;

            uint32_t col = EHSVtoHEX(idx, 0xFF, brightness);
            led->r = (col >> 16) & 0xFF;
            led->g = (col >>  8) & 0xFF;
            led->b = (col >>  0) & 0xFF;
            return;
        }
    }
}

/**
 * @brief Translate a musical note to a string. Only covers the notes we play
 *
 * @param note The note to translate to color
 * @return A null terminated string for this note
 */
char* ICACHE_FLASH_ATTR noteToStr(notePeriod_t note)
{
    switch(note)
    {
        case SILENCE:
        case C_0:
        case C_SHARP_0:
        case D_0:
        case D_SHARP_0:
        case E_0:
        case F_0:
        case F_SHARP_0:
        case G_0:
        case G_SHARP_0:
        case A_0:
        case A_SHARP_0:
        case B_0:
        case C_1:
        case C_SHARP_1:
        case D_1:
        case D_SHARP_1:
        case E_1:
        case F_1:
        case F_SHARP_1:
        case G_1:
        case G_SHARP_1:
        case A_1:
        case A_SHARP_1:
        case B_1:
        case C_2:
        case C_SHARP_2:
        case D_2:
        case D_SHARP_2:
        case E_2:
        case F_2:
        case F_SHARP_2:
        case G_2:
        case G_SHARP_2:
        case A_2:
        case A_SHARP_2:
        case B_2:
        case C_3:
        case C_SHARP_3:
        case D_3:
        case D_SHARP_3:
        case E_3:
        case F_3:
        case F_SHARP_3:
        case G_3:
        case G_SHARP_3:
        case A_3:
        case A_SHARP_3:
        case B_3:
        case C_4:
        case C_SHARP_4:
        case D_4:
        case D_SHARP_4:
        case E_4:
        case F_4:
        case F_SHARP_4:
        case G_4:
        case G_SHARP_4:
        case A_4:
        case A_SHARP_4:
        case B_4:
        case C_5:
        {
            return "C5";
        }
        case C_SHARP_5:
        {
            return "C#5";
        }
        case D_5:
        {
            return "D5";
        }
        case D_SHARP_5:
        {
            return "D#5";
        }
        case E_5:
        {
            return "E5";
        }
        case F_5:
        {
            return "F5";
        }
        case F_SHARP_5:
        {
            return "F#5";
        }
        case G_5:
        {
            return "G5";
        }
        case G_SHARP_5:
        {
            return "G#5";
        }
        case A_5:
        {
            return "A5";
        }
        case A_SHARP_5:
        {
            return "A#5";
        }
        case B_5:
        {
            return "B5";
        }
        case C_6:
        {
            return "C6";
        }
        case C_SHARP_6:
        {
            return "C#6";
        }
        case D_6:
        {
            return "D6";
        }
        case D_SHARP_6:
        {
            return "D#6";
        }
        case E_6:
        {
            return "E6";
        }
        case F_6:
        {
            return "F6";
        }
        case F_SHARP_6:
        {
            return "F#6";
        }
        case G_6:
        {
            return "G6";
        }
        case G_SHARP_6:
        {
            return "G#6";
        }
        case A_6:
        {
            return "A6";
        }
        case A_SHARP_6:
        {
            return "A#6";
        }
        case B_6:
        {
            return "B6";
        }
        case C_7:
        {
            return "C7";
        }
        case C_SHARP_7:
        case D_7:
        case D_SHARP_7:
        case E_7:
        case F_7:
        case F_SHARP_7:
        case G_7:
        case G_SHARP_7:
        case A_7:
        case A_SHARP_7:
        case B_7:
        case C_8:
        case C_SHARP_8:
        case D_8:
        case D_SHARP_8:
        case E_8:
        case F_8:
        case F_SHARP_8:
        case G_8:
        case G_SHARP_8:
        case A_8:
        case A_SHARP_8:
        case B_8:
        default:
        {
            return "";
        }
    }
}
