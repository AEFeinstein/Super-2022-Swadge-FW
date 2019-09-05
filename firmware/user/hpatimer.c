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

#include "hpatimer.h"
#include "adc.h"
#include "missingEspFnPrototypes.h"
#include "gpio_user.h"

/*============================================================================
 * Defines
 *==========================================================================*/

#define FRC1_ENABLE_TIMER  BIT7
#define FRC1_AUTO_RELOAD 64

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

noteFreq_t mNote = SILENCE;
const song_t* mSong = NULL;
uint32_t mNoteTime = 0;
uint32_t mNoteIdx = 0;
os_timer_t songTimer = {0};

/*============================================================================
 * Prototypes
 *==========================================================================*/

static void timerhandle( void* v );

void ICACHE_FLASH_ATTR setBuzzerOn(bool on);
void ICACHE_FLASH_ATTR songTimerCb(void* arg __attribute__((unused)));

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
    if(SILENCE != mNote)
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
    if(SILENCE != mNote)
    {
        RTC_REG_WRITE(FRC1_CTRL_ADDRESS,  FRC1_AUTO_RELOAD |
                      DIVDED_BY_16 | //5MHz main clock.
                      FRC1_ENABLE_TIMER |
                      TM_EDGE_INT );

        RTC_REG_WRITE(FRC1_LOAD_ADDRESS,  mNote);
        RTC_REG_WRITE(FRC1_COUNT_ADDRESS, mNote);

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
    stopBuzzerSong();
    os_timer_setfn(&songTimer, songTimerCb, NULL);
}

/**
 * Set the note currently played by the buzzer
 *
 * @param note The musical note to be played. The numerical value is
 *             5000000/(2*freq). It's written to registers in StartHPATimer()
 */
void ICACHE_FLASH_ATTR setBuzzerNote(noteFreq_t note)
{
    // Set the frequency
    mNote = note;

    // Stop the timer
    PauseHPATimer();
    // Start the timer if we're not playing silence
    if(SILENCE != mNote)
    {
        StartHPATimer();
    }
}

/**
 * Set the song currently played by the buzzer. The pointer will be saved, but
 * no memory will be copied, so don't modify it!
 *
 * @param song A pointer to the song_t struct to be played
 */
void ICACHE_FLASH_ATTR startBuzzerSong(const song_t* song)
{
    // Stop everything
    stopBuzzerSong();

    // Save the song pointer
    mSong = song;

    // Set the timer to call every 1ms
    os_timer_arm(&songTimer, 1, true);

    // Start playing the first note
    setBuzzerNote(mSong->notes[mNoteIdx].note);
}

/**
 * Stops the song currently being played
 */
void ICACHE_FLASH_ATTR stopBuzzerSong(void)
{
    mNote = SILENCE;
    mSong = NULL;
    mNoteTime = 0;
    mNoteIdx = 0;
    os_timer_disarm(&songTimer);
    setBuzzerNote(SILENCE);
}

/**
 * A function called every millisecond. It advances through the song_t struct
 * and plays notes. It will loop the song if shouldLoop is set
 *
 * @param arg unused
 */
void ICACHE_FLASH_ATTR songTimerCb(void* arg __attribute__((unused)))
{
    // Increment the time
    mNoteTime++;

    // Check if it's time for a new note
    if(mNoteTime >= mSong->notes[mNoteIdx].timeMs)
    {
        // This note's time elapsed, try playing the next one
        mNoteIdx++;
        mNoteTime = 0;
        if(mNoteIdx < mSong->numNotes)
        {
            // There's another note to play, so play it
            setBuzzerNote(mSong->notes[mNoteIdx].note);
        }
        else
        {
            // No more notes
            if(mSong->shouldLoop)
            {
                // Song over, but should loop, so start again
                mNoteIdx = 0;
                setBuzzerNote(mSong->notes[mNoteIdx].note);
            }
            else
            {
                // Song over, not looping, stop the timer and the note
                setBuzzerNote(SILENCE);
                os_timer_disarm(&songTimer);
            }
        }
    }
}
