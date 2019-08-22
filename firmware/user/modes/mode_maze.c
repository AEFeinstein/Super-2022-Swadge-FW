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
uint8_t ICACHE_FLASH_ATTR intervalsmeet(FLOATING a,FLOATING c,FLOATING b,FLOATING d,FLOATING e,FLOATING f);
uint8_t ICACHE_FLASH_ATTR  gonethru(FLOATING b_prev[], FLOATING b_now[], FLOATING p_1[], FLOATING p_2[], FLOATING rball, FLOATING b_nowadjusted[]);
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
FLOATING scxc;
FLOATING scyc;
FLOATING scxcprev = 2.0;
FLOATING scycprev = 2.0;

uint8_t width = 7;
uint8_t height = 3; //Maze dimensions must be odd>1 probably for OLED use 31 15
uint8_t mazescalex = 1;
uint8_t mazescaley = 1;
int16_t indwall;
uint8_t xleft[MAXNUMWALLS];
uint8_t xright[MAXNUMWALLS];
uint8_t ytop[MAXNUMWALLS];
uint8_t ybot[MAXNUMWALLS];

FLOATING wxleft[MAXNUMWALLS];
FLOATING wxright[MAXNUMWALLS];
FLOATING wytop[MAXNUMWALLS];
FLOATING wybot[MAXNUMWALLS];
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
        wxleft[i]  = mazescalex * xleft[i];
	wybot[i]   = mazescaley * ybot[i];
        wxright[i] = mazescalex * xright[i];
	wytop[i]   = mazescaley * ytop[i];
	// for width 63 height 31 wx vary from 0, 4, 8, ..., 124  and wy vary from 0, 4, ... 60   radius 1 ball
	//           31        15         from 0, 8, 16, ..., 120                  0, 8, ..., 56  radius 2 ball
        //           15         7 wx      from 0, 16, ... , 112       wy           0, 16, ..., 48 radius 3 ball
        //            7         3         from 0, 32, ..., 96                      0, 32, ...  radius 4 ball
        os_printf("(%d, %d) to (%d, %d)\n", mazescalex*xleft[i], mazescaley*ybot[i], mazescalex*xright[i], mazescaley*ytop[i]);
    }
    
}

/**
 * Called when maze is exited
 */
void ICACHE_FLASH_ATTR mazeExitMode(void)
{

}

/**
 * Linear Alg Find Intersection of line segments
 */

uint8_t ICACHE_FLASH_ATTR intervalsmeet(FLOATING a,FLOATING c,FLOATING b,FLOATING d,FLOATING e,FLOATING f)
{
    // given two points p_1, p_2 in the (x,y) plane specifying a line interval from p_1 to p_2
    //.    parameterized by t
    // a moving object which was at b_prev and is currently at b_now
    // returns true if the object crossed the line interval
    // this can also be useful if want the line interval to be a barrier by
    //    reverting to b_prev
    // the column vector [a,c] is p_2 - p_1
    // the column vector [b,d] is b_now - b_prev
    // the column vector [e,f] is vector from p_1 to b_prev =  b_prev - p_1
    // looking for parametric solution to
    // p_1 + t(p_2 - p_1) = b_prev + s(b_now - b_prev) with 0<= t,s <= 1
    // if (a -b)(t)  (e)
    //    (c -d)(s)  (f) has unique solution with 0<= t,s <= 1
    // returns True

    FLOATING det = -a*d + b*c;
    if (det == 0) return false;
    FLOATING t = (-e*d + f*b) / det; // t is param of interval
    if ((t < 0) || (t > 1)) return false;
    FLOATING s = (a*f - c*e) / det; //s is param of interval from b_prev to b_now
    if ((s < 0) || (s > 1)) return false;
    return true;
}

uint8_t ICACHE_FLASH_ATTR  gonethru(FLOATING b_prev[], FLOATING b_now[], FLOATING p_1[], FLOATING p_2[], FLOATING rball, FLOATING b_nowadjusted[])
{
    // given two points p_1, p_2 in the (x,y) plane specifying a line interval from p_1 to p_2
    // a moving object (ball of radius rball, or point if rball is None)
    // whos center was at b_prev and is currently at b_now
    // returns true if the balls leading (in direction of travel) boundary crossed the line interval
    // this can also be useful if want the line interval to be a barrier by
    //    reverting to b_prev
    // b_nowadjusted is mutable list which is the point moved back to inside boundary
    FLOATING pperp[2];
    uint8_t didgothru;

    b_nowadjusted[0] = b_now[0];
    b_nowadjusted[1] = b_now[1];
    pperp[0] = p_2[1]-p_1[1];
    pperp[1] = p_1[0]-p_2[0];

    FLOATING pperplen = sqrt(pperp[0] * pperp[0] + pperp[1] * pperp[1]);

    if (pperplen == 0.0) return false;

    pperp[0] = pperp[0] / pperplen; // make unit vector
    pperp[1] = pperp[1] / pperplen; // make unit vector


    FLOATING testdir = pperp[0] * (b_now[0] - b_prev[0]) + pperp[1] * (b_now[1] - b_prev[1]);
    if (testdir == 0.0) return false;

    b_nowadjusted[0] = b_now[0] - testdir * pperp[0];
    b_nowadjusted[1] = b_now[1] - testdir * pperp[1];

    if (testdir > 0) // > for leading edge , < for trailing edge
        didgothru =  intervalsmeet(p_2[0]-p_1[0], p_2[1]-p_1[1], b_now[0]-b_prev[0], b_now[1]-b_prev[1], b_prev[0] + rball*pperp[0] - p_1[0], b_prev[1] + rball*pperp[1] - p_1[1]);
    else
        didgothru = intervalsmeet(p_2[0]-p_1[0], p_2[1]-p_1[1], b_now[0]-b_prev[0], b_now[1]-b_prev[1], b_prev[0] - rball*pperp[0] - p_1[0], b_prev[1] - rball*pperp[1] - p_1[1]);
    return didgothru;
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
//  but these are usually between +- 255
    xAccel = mazeAccel.x;
    yAccel = mazeAccel.y;
    zAccel = mazeAccel.z;

    // want -63 to 63 to go approx from 0 to 124 for scxc and 60 to 0 for scyc
    scxc = xAccel + 62; //xAccel/63 * 62 + 62
    scyc = -yAccel/2 + 30; //yAccel/63  + 30


    for (uint8_t i = 0; i < indwall; i++)
    {
	FLOATING p_1[2] = {wxleft[i], wybot[i]};
	FLOATING p_2[2] = {wxright[i], wytop[i]};
	FLOATING b_prev[2] = {scxcprev, scycprev};
	FLOATING b_now[2] = {scxc, scyc};
	FLOATING b_nowadjusted[2];

	if ( gonethru(b_prev, b_now, p_1, p_2, 1, b_nowadjusted) )
	{
              scxc = b_nowadjusted[0];
              scyc = b_nowadjusted[1];
	//} else {
        //    self.x = self.state[0] // update
         //   self.y = self.state[1]
	}
    }

    scxcprev = scxc;
    scycprev = scyc;
    
    //plotCircle(scxc, scyc, 5);
    plotCircle(scxc, scyc, 3);
    plotCircle(scxc, scyc, 1);


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
