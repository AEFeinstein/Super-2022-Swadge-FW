/*
 * mode_roll.c
 *
 *  Created on: 4 Aug 2019
 *      Author: bbkiw
 */


/*============================================================================
 * Includes
 *==========================================================================*/

#include <osapi.h>

#include "user_main.h"
#include "mode_roll.h"
#include "DFT32.h"
//#include "embeddedout.h"
#include "oled.h"
#include "font.h"
#include "MMA8452Q.h"
#include "bresenham.h"
#include "buttons.h"

/*============================================================================
 * Defines
 *==========================================================================*/

//#define BTN_CTR_X 96
//#define BTN_CTR_Y 40
#define BTN_RAD    8
//#define BTN_OFF   12

/*============================================================================
 * Prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR rollEnterMode(void);
void ICACHE_FLASH_ATTR rollExitMode(void);
void ICACHE_FLASH_ATTR rollSampleHandler(int32_t samp);
void ICACHE_FLASH_ATTR rollButtonCallback(uint8_t state __attribute__((unused)),
        int button, int down);
void ICACHE_FLASH_ATTR rollAccelerometerHandler(accel_t* accel);

void ICACHE_FLASH_ATTR roll_updateDisplay(void);

/*============================================================================
 * Variables
 *==========================================================================*/

swadgeMode rollMode =
{
    .modeName = "roll",
    .fnEnterMode = rollEnterMode,
    .fnExitMode = rollExitMode,
    .fnButtonCallback = rollButtonCallback,
    .fnAudioCallback = NULL,
    .wifiMode = SOFT_AP,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = rollAccelerometerHandler
};

accel_t rollAccel = {0};
uint8_t rollButtonState = 0;

/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * Initializer for roll
 */
void ICACHE_FLASH_ATTR rollEnterMode(void)
{
//    InitColorChord();
//    samplesProcessed = 0;
    enableDebounce(false);
}

/**
 * Called when roll is exited
 */
void ICACHE_FLASH_ATTR rollExitMode(void)
{

}


void ICACHE_FLASH_ATTR roll_updateDisplay(void)
{
    // Clear the display
    clearDisplay();

    //const int16_t Ssinonlytable[256]
    os_printf("%d\n", Ssinonlytable[1]);

    // position ball on screen so will move from center outwards in 
    // direction of tilt
    plotCircle(64 + (rollAccel.x>>2), 32 - (rollAccel.y>>3), BTN_RAD);
}


/**
 * TODO
 *
 * @param state  A bitmask of all button states, unused
 * @param button The button which triggered this event
 * @param down   true if the button was pressed, false if it was released
 */
void ICACHE_FLASH_ATTR rollButtonCallback( uint8_t state,
        int button __attribute__((unused)), int down __attribute__((unused)))
{
    rollButtonState = state;
    roll_updateDisplay();
}

/**
 * Store the acceleration data to be displayed later
 *
 * @param x
 * @param x
 * @param z
 */
void ICACHE_FLASH_ATTR rollAccelerometerHandler(accel_t* accel)
{
    rollAccel.x = accel->x;
    rollAccel.y = accel->y;
    rollAccel.z = accel->z;
    roll_updateDisplay();
}
