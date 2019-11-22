/*==============================================================================
 * Includes
 *============================================================================*/

#include <osapi.h>
#include <math.h>

#include "user_main.h"
#include "mode_music.h"

#include "hpatimer.h"
#include "buzzer.h"
#include "custom_commands.h"

#include "oled.h"
#include "bresenham.h"

/*==============================================================================
 * Defines
 *============================================================================*/

#define TICK_HEIGHT 2
#define CURSOR_HEIGHT 4
#define BAR_X_MARGIN 0
#define BAR_Y_MARGIN 4

#define lengthof(x) (sizeof(x) / sizeof(x[0]))

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

/*==============================================================================
 * Structs
 *============================================================================*/

// A single rhythm element, either a note or a rest
typedef struct
{
    uint16_t timeMs;
    bool isRest;
} rhythm_t;

// A collection of parameters
typedef struct
{
    // The parameter's name
    char* name;
    // The notes
    notePeriod_t* notes;
    uint16_t notesLen;
    // The rhythm
    rhythm_t* rhythm;
    uint16_t rhythmLen;
    // The arpeggios
    uint8_t* arpIntervals;
    uint16_t arpLen;
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
    int32_t timeMs;
    int16_t rhythmIdx;
    uint8_t arpIdx;

    // Track the button
    bool shouldPlay;
    uint8_t paramIdx;
} music;

rhythm_t quarterNotes[] =
{
    {
        .timeMs = 250,
        .isRest = false
    },
    {
        .timeMs = 250,
        .isRest = true
    },
};

rhythm_t triplets[] =
{
    {
        .timeMs = 200,
        .isRest = false
    },
    {
        .timeMs = 200,
        .isRest = false
    },
    {
        .timeMs = 200,
        .isRest = false
    },
    {
        .timeMs = 200,
        .isRest = true
    },
};

notePeriod_t M_penta[] = {C_4, D_4, E_4, G_4, A_4, C_5};
notePeriod_t chromatic[] = {C_4, C_SHARP_4, D_4, D_SHARP_4, E_4, F_4, F_SHARP_4, G_4, G_SHARP_4, A_4, A_SHARP_4, B_4, C_5};

uint8_t arp_M_triad[] = {1, 5, 8};

swynthParam_t swynthParams[] =
{
    {
        .name = "Test",
        .notes = M_penta,
        .notesLen = lengthof(M_penta),
        .rhythm = quarterNotes,
        .rhythmLen = lengthof(quarterNotes),
        .arpIntervals = NULL,
        .arpLen = 0
    },
    {
        .name = "ArpTest",
        .notes = M_penta,
        .notesLen = lengthof(M_penta),
        .rhythm = triplets,
        .rhythmLen = lengthof(triplets),
        .arpIntervals = arp_M_triad,
        .arpLen = lengthof(arp_M_triad)
    },
    {
        .name = "Chroma",
        .notes = chromatic,
        .notesLen = lengthof(chromatic),
        .rhythm = quarterNotes,
        .rhythmLen = lengthof(quarterNotes),
        .arpIntervals = NULL,
        .arpLen = 0
    },
};

notePeriod_t allNotes[] =
{
    C_0,
    C_SHARP_0,
    D_0,
    D_SHARP_0,
    E_0,
    F_0,
    F_SHARP_0,
    G_0,
    G_SHARP_0,
    A_0,
    A_SHARP_0,
    B_0,
    C_1,
    C_SHARP_1,
    D_1,
    D_SHARP_1,
    E_1,
    F_1,
    F_SHARP_1,
    G_1,
    G_SHARP_1,
    A_1,
    A_SHARP_1,
    B_1,
    C_2,
    C_SHARP_2,
    D_2,
    D_SHARP_2,
    E_2,
    F_2,
    F_SHARP_2,
    G_2,
    G_SHARP_2,
    A_2,
    A_SHARP_2,
    B_2,
    C_3,
    C_SHARP_3,
    D_3,
    D_SHARP_3,
    E_3,
    F_3,
    F_SHARP_3,
    G_3,
    G_SHARP_3,
    A_3,
    A_SHARP_3,
    B_3,
    C_4,
    C_SHARP_4,
    D_4,
    D_SHARP_4,
    E_4,
    F_4,
    F_SHARP_4,
    G_4,
    G_SHARP_4,
    A_4,
    A_SHARP_4,
    B_4,
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
    uint8_t state  __attribute__((unused)), int button, int down)
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
                music.timeMs = 0;
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
    //           music.roll, music.pitch,
    //           accel->x, accel->y, accel->z);

    musicUpdateDisplay();
}

/**
 * Update the display by drawing the current state of affairs
 */
void ICACHE_FLASH_ATTR musicUpdateDisplay(void)
{
    clearDisplay();

    // Plot the main bar
    plotLine(
        BAR_X_MARGIN,
        OLED_HEIGHT - BAR_Y_MARGIN,
        OLED_WIDTH - BAR_X_MARGIN,
        OLED_HEIGHT - BAR_Y_MARGIN,
        WHITE);

    // Plot tick marks at each of the note boundaries
    uint8_t tick;
    for(tick = 0; tick < swynthParams[music.paramIdx].notesLen + 1; tick++)
    {
        uint8_t x = BAR_X_MARGIN + ( (tick * ((OLED_WIDTH - 1) - (BAR_X_MARGIN * 2))) / swynthParams[music.paramIdx].notesLen) ;
        plotLine(x, OLED_HEIGHT - BAR_Y_MARGIN - TICK_HEIGHT,
                 x, OLED_HEIGHT - BAR_Y_MARGIN + TICK_HEIGHT,
                 WHITE);
    }

    // Plot the cursor
    plotLine(music.roll, OLED_HEIGHT - BAR_Y_MARGIN - CURSOR_HEIGHT,
             music.roll, OLED_HEIGHT - BAR_Y_MARGIN + CURSOR_HEIGHT,
             WHITE);
}

/**
 * @brief Called every 1ms to handle the rhythm, arpeggios, and setting the buzzer
 *
 * @param arg unused
 */
void ICACHE_FLASH_ATTR musicBeatTimerFunc(void* arg __attribute__((unused)))
{
    // Increment time
    music.timeMs++;
    // If time crossed a rhythm boundary, do something different
    if(music.timeMs >= swynthParams[music.paramIdx].rhythm[music.rhythmIdx].timeMs)
    {
        // Reset the time
        music.timeMs = 0;
        // Move to the next rhythm element
        music.rhythmIdx = (music.rhythmIdx + 1) % swynthParams[music.paramIdx].rhythmLen;

        // See if the note should be arpeggiated. Do this even for unplayed notes
        if(NULL != swynthParams[music.paramIdx].arpIntervals &&
                !swynthParams[music.paramIdx].rhythm[music.rhythmIdx].isRest)
        {
            // Track the current arpeggio index
            music.arpIdx = (music.arpIdx + 1) % swynthParams[music.paramIdx].arpLen;
        }

        // See if we should actually play the note or not
        if(!music.shouldPlay || swynthParams[music.paramIdx].rhythm[music.rhythmIdx].isRest)
        {
            setBuzzerNote(SILENCE);
        }
        else
        {
            // Get the index of the note to play based on roll
            uint8_t noteIdx = (music.roll * swynthParams[music.paramIdx].notesLen) / OLED_WIDTH;

            notePeriod_t noteToPlay;
            if(NULL != swynthParams[music.paramIdx].arpIntervals)
            {
                // This mode has an arpeggio, so modify the current note
                noteToPlay = arpModify(
                                 swynthParams[music.paramIdx].notes[noteIdx],
                                 swynthParams[music.paramIdx].arpIntervals[music.arpIdx]);
            }
            else
            {
                // This mode has no arpeggio, so play it straight
                noteToPlay = swynthParams[music.paramIdx].notes[noteIdx];
            }

            setBuzzerNote(noteToPlay);
        }
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
