/*
 * mode_maze.c
 *
 *  Created on: 4 Aug 2019
 *      Author: bbkiwi
 */

//MAZE Version


/*============================================================================
 * Includes
 *==========================================================================*/

#include <osapi.h>

#include "user_main.h"
#include "mode_maze.h"
#include "DFT32.h"
#include "mazegen.h"
//#include "embeddedout.h"
#include "oled.h"
#include "font.h"
#include "MMA8452Q.h"
#include "bresenham.h"
#include "buttons.h"
#include "math.h"
#include "ode_solvers.h"

/*============================================================================
 * Defines
 *==========================================================================*/
//NOTE in ode_solvers.h is #define of FLOATING float    or double to test
//#define LEN_PENDULUM 1
/*============================================================================
 * Prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR mazeEnterMode(void);
void ICACHE_FLASH_ATTR mazeExitMode(void);
void ICACHE_FLASH_ATTR mazeSampleHandler(int32_t samp);
void ICACHE_FLASH_ATTR mazeButtonCallback(uint8_t state __attribute__((unused)),
        int button, int down);
void ICACHE_FLASH_ATTR mazeAccelerometerHandler(accel_t* accel);

void ICACHE_FLASH_ATTR maze_updateDisplay(void);
uint16_t ICACHE_FLASH_ATTR norm(int16_t xc, int16_t yc);
void ICACHE_FLASH_ATTR setmazeLeds(led_t* ledData, uint8_t ledDataLen);
void dnx(FLOATING, FLOATING [], FLOATING [], int );
int16_t ICACHE_FLASH_ATTR get_maze(uint8_t width, uint8_t height, uint8_t xleft[], uint8_t xright[], uint8_t ybot[], uint8_t ytop[]);

/*============================================================================
 * Static Const Variables
 *==========================================================================*/

static const uint8_t mazeBrightnesses[] =
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

swadgeMode mazeMode =
{
    .modeName = "maze",
    .fnEnterMode = mazeEnterMode,
    .fnExitMode = mazeExitMode,
    .fnButtonCallback = mazeButtonCallback,
    .fnAudioCallback = NULL,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = mazeAccelerometerHandler
};

accel_t mazeAccel = {0};
uint8_t mazeButtonState = 0;
uint8_t mazeBrightnessIdx = 0;
int maze_ledCount = 0;

/* global variables for ODE */
//const FLOATING gravity;               // free fall acceleration in m/s^2
//const FLOATING mass;                // mass of a projectile in kg
//const FLOATING radconversion; = 3.1415926/180.0;  // radians
//uint8_t numberoffirstordereqn;                          // number of first-order equations

#define MAX_EQNS 4                     // MAX number of first-order equations
FLOATING ti, tf, dt;
FLOATING xi[MAX_EQNS], xf[MAX_EQNS];
FLOATING v0, a0;

FLOATING xAccel;
FLOATING yAccel;
FLOATING zAccel;
FLOATING len;

uint8_t width = 7;
uint8_t height = 3; //Maze dimensions must be odd>1 probably for OLED use 31 15
uint8_t mazescalex = 1;
uint8_t mazescaley = 1;
int16_t indwall;
uint8_t xleft[MAXNUMWALLS];
uint8_t xright[MAXNUMWALLS];
uint8_t ytop[MAXNUMWALLS];
uint8_t ybot[MAXNUMWALLS];


/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * Initializer for maze
 */
void ICACHE_FLASH_ATTR mazeEnterMode(void)
{
    enableDebounce(false);
    indwall = get_maze(width, height, xleft, xright, ybot, ytop);
    mazescalex = 127/width;
    mazescaley = 63/height;
    for (uint8_t i = 0; i < indwall; i++)
    {
        os_printf("(%d, %d) to (%d, %d)\n", mazescalex*xleft[i], mazescaley*ybot[i], mazescalex*xright[i], mazescaley*ytop[i]);
    }
    
}

/**
 * Called when maze is exited
 */
void ICACHE_FLASH_ATTR mazeExitMode(void)
{

}



void ICACHE_FLASH_ATTR maze_updateDisplay(void)
{
    // Clear the display
    clearDisplay();

    for (uint8_t i = 0; i < indwall; i++)
    {
        plotLine(mazescalex*xleft[i], mazescaley*ybot[i], mazescalex*xright[i], mazescaley*ytop[i]);
    }

    //NOTE bug in Expressif OS can't print floating point! Must cast as int
    //debug print for thrown ball
    //os_printf("100t = %d, x = %d, y = %d, vx = %d, vy = %d\n", (int)(100*ti), (int)xi[0], (int)xi[1], (int)xi[2], (int)xi[3]);

    //Save accelerometer reading in global storage
//TODO can get values bigger than 1. here, my accelerometer has 14 bits
    xAccel = mazeAccel.x / 256.0;
    yAccel = mazeAccel.y / 256.0;
    zAccel = mazeAccel.z / 256.0;

 
    int16_t scxc = mazeAccel.x>>2;
    int16_t scyc = mazeAccel.y>>3;

    
    //plotCircle(64 + scxc, 32 - scyc, 5);
    plotCircle(64 + scxc, 32 - scyc, 3);
    plotCircle(64 + scxc, 32 - scyc, 1);


    // Declare some LEDs, all off
    led_t leds[NUM_LIN_LEDS] = {{0}};
#define GAP 1

/*  Python
    for led in self.leds:
        led.alpha = (1 - abs(self.ball.position - led.position)/(30+2*self.size.h/2.5))**3
        if self.framecount % 1 == 0:
            led.color = colorsys.hsv_to_rgb(self.ball.meanspeed,1,1)
*/

    for (uint8_t indLed=0; indLed<NUM_LIN_LEDS/GAP; indLed++)
    {
        int16_t ledy = Ssinonlytable[((indLed<<8)*GAP/NUM_LIN_LEDS + 0x80) % 256]*28/1500; // from -1500 to 1500
        int16_t ledx = Ssinonlytable[((indLed<<8)*GAP/NUM_LIN_LEDS + 0xC0) % 256]*28/1500;
        len = sqrt((scxc - ledx)*(scxc - ledx) + (scyc - ledy)*(scyc - ledy));
        //len = norm(scxc - ledx, scyc - ledy);
        uint8_t glow = 255 * pow(1.0 - (len / 56.0), 3);
        //os_printf("%d %d %d %d %d %d %d \n",indLed, ledx, ledy, scxc, scyc, len, 255 - len * 4);
        //leds[GAP*indLed].r = 255 - len * 4;
        leds[GAP*indLed].r = glow;
    }
    setmazeLeds(leds, sizeof(leds));

}


/**
 *
 * @param state  A bitmask of all button states, unused
 * @param button The button which triggered this event
 * @param down   true if the button was pressed, false if it was released
 */
void ICACHE_FLASH_ATTR mazeButtonCallback( uint8_t state,
        int button __attribute__((unused)), int down __attribute__((unused)))
{
    mazeButtonState = state;
    maze_updateDisplay();

    if(down)
    {
        if(3 == button)
        {
            // Cycle brightnesses
            mazeBrightnessIdx = (mazeBrightnessIdx + 1) %
                                 (sizeof(mazeBrightnesses) / sizeof(mazeBrightnesses[0]));
        }
	if(1 == button)
	{
		// get new maze
		mazeEnterMode( );
	}
	if(2 == button)
	{
		// get new maze cycling thru size
		if (width == 63)
		{
			width = 7;
			height = 3;
		} else {
			height = width;
			width = 2*height + 1;
		}
		mazeEnterMode( );
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
void ICACHE_FLASH_ATTR mazeAccelerometerHandler(accel_t* accel)
{
    mazeAccel.x = accel->x;
    mazeAccel.y = accel->y;
    mazeAccel.z = accel->z;
    maze_updateDisplay();
}

/**
 * Intermediate function which adjusts brightness and sets the LEDs
 *
 * @param ledData    The LEDs to be scaled, then set
 * @param ledDataLen The length of the LEDs to set
 */
void ICACHE_FLASH_ATTR setmazeLeds(led_t* ledData, uint8_t ledDataLen)
{
    uint8_t i;
    for(i = 0; i < ledDataLen / sizeof(led_t); i++)
    {
        ledData[i].r = ledData[i].r / mazeBrightnesses[mazeBrightnessIdx];
        ledData[i].g = ledData[i].g / mazeBrightnesses[mazeBrightnessIdx];
        ledData[i].b = ledData[i].b / mazeBrightnesses[mazeBrightnessIdx];
    }
    setLeds(ledData, ledDataLen);
}
