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
    int16_t roll;
    int16_t pitch;
    os_timer_t beatTimer;
    bool shouldPlay;
    int32_t timeMs;
    int16_t rhythmIdx;
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

notePeriod_t M_penta[] = {C_4, D_4, E_4, G_4, A_4, C_5};

swynthParam_t swynthParams[] =
{
    {
        .name = "Test",
        .notes = M_penta,
        .notesLen = lengthof(M_penta),
        .rhythm = quarterNotes,
        .rhythmLen = lengthof(quarterNotes),
        .arpIntervals = NULL
    }
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

    memset(&music, 0, sizeof(music));

    // Set a timer to tick every 1ms, forever
    os_timer_disarm(&music.beatTimer);
    os_timer_setfn(&music.beatTimer, musicBeatTimerFunc, NULL);
    os_timer_arm(&music.beatTimer, 1, true);

    musicUpdateDisplay();
}

/**
 * Called when music is exited
 */
void ICACHE_FLASH_ATTR musicExitMode(void)
{

}

/**
 * @brief Button callback function, plays notes and switches parameters
 *
 * @param state  A bitmask of all button states, unused
 * @param button The button which triggered this event
 * @param down   true if the button was pressed, false if it was released
 */
void ICACHE_FLASH_ATTR musicButtonCallback( uint8_t state,
        int button __attribute__((unused)), int down __attribute__((unused)))
{
    switch(button)
    {
        case 1:
        {
            // Left
            if(down)
            {
                // TODO cycle params
            }
            break;
        }
        case 2:
        {
            // Right
            music.shouldPlay = down;
            break;
        }
    }
}

/**
 * @brief Callback function for accelerometer values
 * Use the current vector to find pitch and roll, then update the display
 *
 * @param accel
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
    for(tick = 0; tick < swynthParams[0].notesLen + 1; tick++)
    {
        uint8_t x = BAR_X_MARGIN + ( (tick * ((OLED_WIDTH - 1) - (BAR_X_MARGIN * 2))) / swynthParams[0].notesLen) ;
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
 * @brief Called every 10ms to handle the rhythm
 *
 * @param arg
 */
void ICACHE_FLASH_ATTR musicBeatTimerFunc(void* arg __attribute__((unused)))
{
    music.timeMs++;

    if(music.timeMs >= swynthParams[0].rhythm[music.rhythmIdx].timeMs)
    {
        music.timeMs = 0;
        music.rhythmIdx = (music.rhythmIdx + 1) % swynthParams[0].rhythmLen;

        if(!music.shouldPlay || swynthParams[0].rhythm[music.rhythmIdx].isRest)
        {
            setBuzzerNote(SILENCE);
        }
        else
        {
            uint8_t noteIdx = (music.roll * swynthParams[0].notesLen) / OLED_WIDTH;
            setBuzzerNote(swynthParams[0].notes[noteIdx]);
        }
    }
}
