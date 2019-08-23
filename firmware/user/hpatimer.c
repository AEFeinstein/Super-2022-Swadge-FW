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
uint32_t mFrequency = 32000;
bool mBuzzerOn = false;

/*============================================================================
 * Prototypes
 *==========================================================================*/

static void timerhandle( void* v );

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
    if(mBuzzerOn)
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
    RTC_REG_WRITE(FRC1_CTRL_ADDRESS,  FRC1_AUTO_RELOAD |
                  DIVDED_BY_16 | //5MHz main clock.
                  FRC1_ENABLE_TIMER |
                  TM_EDGE_INT );

    RTC_REG_WRITE(FRC1_LOAD_ADDRESS,  5000000 / mFrequency);
    RTC_REG_WRITE(FRC1_COUNT_ADDRESS, 5000000 / mFrequency);

    ETS_FRC_TIMER1_INTR_ATTACH(timerhandle, NULL);

    ContinueHPATimer();
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

/**
 * @brief Set the Buzzer On object
 *
 * @param on true to play sound, false to not play sound
 */
void ICACHE_FLASH_ATTR setBuzzerOn(bool on)
{
    mBuzzerOn = on;
    if(false == on)
    {
        setBuzzerGpio(false);
    }
}

/**
 * @brief Set the current PWM output frequency
 *
 * @param frequency The frequency to call the timer. The square wave will have a
 *                  frequency of half this
 */
void ICACHE_FLASH_ATTR setBuzzerFrequency(uint32_t frequency)
{
    if(frequency > 32000)
    {
        frequency = 32000;
    }
    // Set the frequency
    mFrequency = frequency;

    // Stop and start the timer at the new frequency
    PauseHPATimer();
    StartHPATimer();
}