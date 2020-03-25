//Copyright 2015 <>< Charles Lohr Under the MIT/x11 License, NewBSD License or
// ColorChord License.  You Choose.

/*============================================================================
 * Includes
 *==========================================================================*/

#include <osapi.h>
#include <mem.h>
#include <eagle_soc.h>
#include <user_interface.h>
#include <ets_sys.h>
#include <math.h>

#include "hpatimer.h"
#include "adc.h"
#include "esp_niceness.h"
#include "gpio_user.h"
#include "nvm_interface.h"
#include "synced_timer.h"

/*============================================================================
 * Defines
 *==========================================================================*/

#define FRC1_ENABLE_TIMER  BIT7
#define FRC1_AUTO_RELOAD 64

// #define BZR_DBG(...) os_printf(__VA_ARGS__)
#define BZR_DBG(...)

/*============================================================================
 * Enums
 *==========================================================================*/

typedef enum
{
    DIVDED_BY_1 = 0,
    DIVDED_BY_16 = 4,
    DIVDED_BY_256 = 8,
} TIMER_PREDIVED_MODE;

typedef enum
{
    TM_LEVEL_INT = 1,
    TM_EDGE_INT   = 0,
} TIMER_INT_MODE;

/*============================================================================
 * Variables
 *==========================================================================*/

volatile bool hpaRunning = false;

struct
{
    uint16_t currNote; // Actually a clock divisor
    uint16_t currDuration;
    const song_t* song;
    bool songShouldLoop;
    uint32_t noteTime;
    uint32_t noteIdx;
    syncedTimer_t songTimer;
} bzr = {0};

/*============================================================================
 * Prototypes
 *==========================================================================*/

static void timerhandle( void* v );

void ICACHE_FLASH_ATTR setBuzzerOn(bool on);
void ICACHE_FLASH_ATTR songTimerCb(void* arg __attribute__((unused)));
void ICACHE_FLASH_ATTR loadNextNote(void);

/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * Timer callback function, registered by ETS_FRC_TIMER1_INTR_ATTACH() in StartHPATimer()
 * Calls hs_adc_read() to read a sample off the ADC
 *
 * This timer is attached to an interrupt, so it shouldn't be ICACHE_FLASH_ATTR
 *
 * @param v unused
 */
static void timerhandle( void* v __attribute__((unused)))
{
    RTC_CLR_REG_MASK(FRC1_INT_ADDRESS, FRC1_INT_CLR_MASK);
    if(SILENCE != bzr.currNote)
    {
        setBuzzerGpio(!getBuzzerGpio());
    }
}

/**
 * Initialize RTC Timer 1 to  run at 16KHz (DFREQ) and call timerhandle()
 * This timer is also used for PWM, so it can't do both PWM and ADC reading at the same time
 *
 * Calls ContinueHPATimer() to fully enable to timer and start an ADC reading with hs_adc_start()
 */
void ICACHE_FLASH_ATTR StartHPATimer(void)
{
    if(SILENCE != bzr.currNote)
    {
        RTC_REG_WRITE(FRC1_CTRL_ADDRESS,  FRC1_AUTO_RELOAD |
                      DIVDED_BY_16 | //5MHz main clock.
                      FRC1_ENABLE_TIMER |
                      TM_EDGE_INT );

        RTC_REG_WRITE(FRC1_LOAD_ADDRESS,  bzr.currNote);
        RTC_REG_WRITE(FRC1_COUNT_ADDRESS, bzr.currNote);

        ETS_FRC_TIMER1_INTR_ATTACH(timerhandle, NULL);

        ContinueHPATimer();
    }
}

/**
 * Pause the hardware timer
 */
void PauseHPATimer(void)
{
    TM1_EDGE_INT_DISABLE();
    ETS_FRC1_INTR_DISABLE();
    hpaRunning = false;
}

/**
 * Start the hardware timer
 */
void ContinueHPATimer(void)
{
    TM1_EDGE_INT_ENABLE();
    ETS_FRC1_INTR_ENABLE();
    hpaRunning = true;
}

/**
 * @return true if the hpa timer is running, false otherwise
 */
bool ICACHE_FLASH_ATTR isHpaRunning(void)
{
    return hpaRunning;
}

/*============================================================================
 * Buzzer Functions
 *==========================================================================*/

/**
 * Initialize the buzzer's state variables and timer
 */
void ICACHE_FLASH_ATTR initBuzzer(void)
{
    // Keep it high in the idle state
    setBuzzerGpio(false);

    // If it's muted, don't set anything
    if(getIsMutedOption())
    {
        return;
    }

    ets_memset(&bzr, 0, sizeof(bzr));
    stopBuzzerSong();
    syncedTimerSetFn(&bzr.songTimer, songTimerCb, NULL);
}

/**
 * Set the note currently played by the buzzer
 *
 * @param note The musical note to be played.
 *             note==0 means silence, note=-1 means take from mode_music
 *             positive values are the counts to produce a pitch
 *             The numerical value is 5000000/(2*freq).
 *             It's written to registers in StartHPATimer()
 */
void ICACHE_FLASH_ATTR setBuzzerNote(uint16_t note)
{
    BZR_DBG("%s %d\n", __func__, noteClockDivisor);
    // If it's muted or not actually changing don't set anything
    if(getIsMutedOption() || (bzr.currNote == note))
    {
        return;
    }
    else
    {
        // Set the period count
        bzr.currNote = note;

        // Stop the timer
        PauseHPATimer();
        // Start the timer if we're not playing silence
        if(SILENCE != bzr.currNote)
        {
            StartHPATimer();
        }
        else
        {
            // Keep it high when SILENCED
            setBuzzerGpio(false);
        }
    }
}

/**
 * Set the song currently played by the buzzer. The pointer will be saved, but
 * no memory will be copied, so don't modify it!
 *
 * @param song A pointer to the song_t struct to be played
 * @param shouldLoop true to loop the song, false otherwise
 */
void ICACHE_FLASH_ATTR startBuzzerSong(const song_t* song, bool shouldLoop)
{
    BZR_DBG("%s, %d notes, %d internote pause\n", __func__, song->numNotes, song->interNotePause);
    // If it's muted, don't set anything
    if(getIsMutedOption())
    {
        return;
    }

    // Stop everything
    stopBuzzerSong();

    // Save the song pointer
    bzr.song = song;
    bzr.songShouldLoop = shouldLoop;

    // Set the timer to call every 1ms
    syncedTimerArm(&bzr.songTimer, 1, true);

    // Start playing the first note
    loadNextNote();
}

/**
 * @brief Load the next note from the song's ROM and play it
 */
void ICACHE_FLASH_ATTR loadNextNote(void)
{
    uint32_t noteAndDuration = bzr.song->notes[bzr.noteIdx];
    bzr.currDuration = (noteAndDuration >> 16) & 0xFFFF;
    setBuzzerNote(noteAndDuration & 0xFFFF);

    BZR_DBG("%s n:%5d d:%5d\n", __func__, bzr.currNote, bzr.currDuration);
}

/**
 * Stops the song currently being played
 */
void ICACHE_FLASH_ATTR stopBuzzerSong(void)
{
    BZR_DBG("%s\n", __func__);

    setBuzzerNote(SILENCE);
    bzr.song = NULL;
    bzr.noteTime = 0;
    bzr.noteIdx = 0;
    syncedTimerDisarm(&bzr.songTimer);
}

/**
 * A function called every millisecond. It advances through the song_t struct
 * and plays notes. It will loop the song if shouldLoop is set
 *
 * @param arg unused
 */
void ICACHE_FLASH_ATTR songTimerCb(void* arg __attribute__((unused)))
{
    // If it's muted, don't set anything
    if(getIsMutedOption())
    {
        return;
    }

    // Increment the time
    bzr.noteTime++;

    // Check if it's time for a new note
    if(bzr.noteTime >= bzr.currDuration)
    {
        // This note's time elapsed, try playing the next one
        bzr.noteIdx++;
        bzr.noteTime = 0;
        if(bzr.noteIdx < bzr.song->numNotes)
        {
            // There's another note to play, so play it
            loadNextNote();
        }
        else
        {
            // No more notes
            if(bzr.songShouldLoop)
            {
                BZR_DBG("Loop\n");
                // Song over, but should loop, so start again
                bzr.noteIdx = 0;
                loadNextNote();
            }
            else
            {
                BZR_DBG("Don't loop\n");
                // Song over, not looping, stop the timer and the note
                setBuzzerNote(SILENCE);
                syncedTimerDisarm(&bzr.songTimer);
            }
        }
    }
    else if ((bzr.currDuration > bzr.song->interNotePause) &&
             (bzr.noteTime >= bzr.currDuration - bzr.song->interNotePause))
    {
        // Pause a little between notes
        setBuzzerNote(SILENCE);
    }
}
