/*
 * mode_roll.c
 *
 *  Created on: 4 Aug 2019
 *      Author: bbkiwi
 */

//TODO lots of repeated code from mode_dance and mode_demo and had to change
//     names. Wasting space. More common routines needed.


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
uint16_t ICACHE_FLASH_ATTR norm(int16_t xc, int16_t yc);
/*============================================================================
 * Static Const Variables
 *==========================================================================*/

static const uint8_t rollBrightnesses[] =
{
    0x01,
    0x08,
    0x40,
    0x80,
    0xC0,
};



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
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = rollAccelerometerHandler
};

accel_t rollAccel = {0};
uint8_t rollButtonState = 0;
uint8_t rollBrightnessIdx = 0;
int roll_ledCount = 0;
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

/**
 * Approximates sqrt of sum of squares
 */
uint16_t ICACHE_FLASH_ATTR norm(int16_t xc, int16_t yc)
{
    xc = xc<0? -xc : xc;
    yc = yc<0? -yc : yc;
    return xc>yc? xc + (yc>>1) : yc + (xc>>1);
}

void ICACHE_FLASH_ATTR roll_updateDisplay(void)
{
    int16_t scxc, scyc;
    // Clear the display
    clearDisplay();

    // Using center of screen as orgin, position ball  proportional to x,y component of rollAccel
    //plotCircle(64 + (rollAccel.x>>2), 32 - (rollAccel.y>>3), BTN_RAD);

    // Using center of screen as orgin, position ball on circle of radius 32 with direction x,y component of rollAccel
    int16_t xc = rollAccel.x;
    int16_t yc = rollAccel.y;
    uint16_t len = norm(xc, yc);
    if (len>0) {
        // scale normalized vector to length 28 to keep ball within bounds of screen
        scxc = ((xc*28) / len);
        scyc = ((yc*28) / len);
	//os_printf("xc %d, yc %d, len %d scxc %d scyc %d\n", xc, yc, len, scxc, scyc);
        plotCircle(64 + scxc, 32 - scyc, 5);
        plotCircle(64 + scxc, 32 - scyc, 3);
        plotCircle(64 + scxc, 32 - scyc, 1);
    }

    // Declare some LEDs, all off
    led_t leds[16] = {{0}};

/*  Python
    for led in self.leds:
        led.alpha = (1 - abs(self.ball.position - led.position)/(30+2*self.size.h/2.5))**3
        if self.framecount % 1 == 0:
            led.color = colorsys.hsv_to_rgb(self.ball.meanspeed,1,1)
*/

    for (uint8_t indLed=0; indLed<8; indLed++)
    {
        int16_t ledy = Ssinonlytable[((indLed<<5) + 0x80) % 256]*28/1500; // from -1500 to 1500
        int16_t ledx = Ssinonlytable[((indLed<<5) + 0xC0) % 256]*28/1500;
        len = norm(scxc - ledx, scyc - ledy);
        //os_printf("%d %d %d %d %d %d %d \n",indLed, ledx, ledy, scxc, scyc, len, 255 - len * 4);
        leds[2*indLed].r = 255 - len * 4;
    }
    setRollLeds(leds, sizeof(leds));

}


/**
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

    if(down)
    {
        if(1 == button)
        {
            // Cycle brightnesses
            rollBrightnessIdx = (rollBrightnessIdx + 1) %
                                 (sizeof(rollBrightnesses) / sizeof(rollBrightnesses[0]));
        }
    }
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

/**
 * Intermediate function which adjusts brightness and sets the LEDs
 *
 * @param ledData    The LEDs to be scaled, then set
 * @param ledDataLen The length of the LEDs to set
 */
void ICACHE_FLASH_ATTR setRollLeds(led_t* ledData, uint8_t ledDataLen)
{
    uint8_t i;
    for(i = 0; i < ledDataLen / sizeof(led_t); i++)
    {
        ledData[i].r = ledData[i].r / rollBrightnesses[rollBrightnessIdx];
        ledData[i].g = ledData[i].g / rollBrightnesses[rollBrightnessIdx];
        ledData[i].b = ledData[i].b / rollBrightnesses[rollBrightnessIdx];
    }
    setLeds(ledData, ledDataLen);
}
