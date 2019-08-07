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
#include "math.h"
#include "ode_solvers.h"

/*============================================================================
 * Defines
 *==========================================================================*/
//NOTE in ode_solvers.h is #define of FLOATING float    or double to test
#define LEN_PENDULUM 1
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
void ICACHE_FLASH_ATTR setRollLeds(led_t* ledData, uint8_t ledDataLen);
void dnx(FLOATING, FLOATING [], FLOATING [], int );
//brought in from ode_solvers.h
//void rk4_dn1(void(*)(FLOATING, FLOATING [], FLOATING [], int ),
//               FLOATING, FLOATING, FLOATING [], FLOATING [], int);
//void euler_dn1(void(*)(FLOATING, FLOATING [], FLOATING [], int ),
//               FLOATING, FLOATING, FLOATING [], FLOATING [], int);

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

/* global variables for ODE */
const FLOATING g = 9.81;               // free fall acceleration in m/s^2
const FLOATING m = 1.0;                // mass of a projectile in kg
const FLOATING rad = 3.1415926/180.0;  // radians
const int neqn = 2;                      // number of first-order equations
#define MAX_EQNS 4                     // MAX number of first-order equations
FLOATING ti, tf, dt;
FLOATING xi[MAX_EQNS], xf[MAX_EQNS];
FLOATING v0, a0;

FLOATING xAccel;
FLOATING yAccel;
FLOATING zAccel;
FLOATING len;

/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * Initializer for roll
 */
void ICACHE_FLASH_ATTR rollEnterMode(void)
{
    enableDebounce(false);
    /* initial information for ODE */
/* for thrown ball
    ti = 0.0;                // initial value for variable t
    v0 = 180.0;              // initial speed (m/s)
    a0 =  45.0;              // initial angle (degrees)
    xi[0] = 0.0;             // initial position in x (m)
    xi[1] = 0.0;             // initial position in y (m)
    xi[2] = v0*cos(a0*rad);  // initial speed in x direction (m.s)
    xi[3] = v0*sin(a0*rad);  // initial speed in y direction (m/s)
    dt = 0.1;                // step size for integration (s)
*/
// For damped pendulum
    ti = 0.0;                // initial value for variable t
    xi[0] = 0.0;             // initial angle position in (radians)
    xi[1] = 0.0;             // initial angular speed in radians/ sec
    dt = 0.1;                // step size for integration (s)
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
//uint16_t ICACHE_FLASH_ATTR norm(int16_t xc, int16_t yc)
//{
//    xc = xc<0? -xc : xc;
//    yc = yc<0? -yc : yc;
//    return xc>yc? xc + (yc>>1) : yc + (xc>>1);
//}




/*==== RHS of ODE ===============================================*/

/*
// Motion of thrown ball
void ICACHE_FLASH_ATTR dnx(FLOATING t, FLOATING x[], FLOATING dx[], int n)
{
// to stop warning that t and n not used
   t = t;
   n = n;
   //first order
    dx[0] = x[2];
    dx[1] = x[3];
   // second order
    dx[2] = 0.0;
    dx[3] = (-1.0)*g;
}
*/

/*
Python
def derivs(t, state, force):
    g = gravity()
    # seems to use portrait mode coor while scene uses landscape
    # holding ipad flat
    # gxy points from center of screen to lowest point on circle around the center
    #   magnitude is cos of angle with vertical
    gxy = Vector2(g.y, -g.x)
    down = math.atan2(gxy[0], -gxy[1])
    #print(g.z)
    # gravity in direction of down
    #return numpy.array([state[1], force + -9.8/L1 * sin(state[0] - down) - .05 * state[1]])
    # gravity scaled by g.z so max when nearly flat
    #return numpy.array([state[1], force + g.z * 9.8/L1 * sin(state[0] - down) - .05 * state[1]])
    # gravity is proportional to  downward component
    return numpy.array([state[1], force + -9.8 *sqrt(1.0 - g.z**2)/L1 * sin(state[0] - down) - .05 * state[1]])
*/

// Motion of damped rigid pendulum with gravity the downward component
// of the accelerometer
// Here x is [th, thdot] position in radian, speed in radians/sec
void ICACHE_FLASH_ATTR dnx(FLOATING t, FLOATING x[], FLOATING dx[], int n)
{
// to stop warning that t and n not used
   t = t;
   n = n;
   //first order
   dx[0] = x[1];
   // second order
   FLOATING down = atan2(yAccel, -xAccel);
   //os_printf("100down = %d\n", (int)(100*down)); // down 0 is with positve x on accel pointing down
   FLOATING force = 0.0; // later used for button replulsion
   //os_printf("%d\n", (int)(100*zAccel)); //xAccel can exceed 1
//TODO so this computation sometime got error and then got negative overflow and all stopped working
   //dx[1] = force + -g * sqrt(1.0 - pow(zAccel, 2)) / LEN_PENDULUM * sin(x[0] - down) - .05 * x[1];
   //safer calculation that won't get sqrt of negative number
   dx[1] = force + -g * sqrt(pow(xAccel,2) + pow(yAccel, 2)) / LEN_PENDULUM * sin(x[0] - down) - .05 * x[1];
}


void ICACHE_FLASH_ATTR roll_updateDisplay(void)
{
    // Clear the display
    clearDisplay();

    //NOTE bug in Expressif OS can't print floating point! Must cast as int
    //debug print for thrown ball
    //os_printf("100t = %d, x = %d, y = %d, vx = %d, vy = %d\n", (int)(100*ti), (int)xi[0], (int)xi[1], (int)xi[2], (int)xi[3]);

    //Save accelerometer reading in global storage
//TODO can get values bigger than 1. here, my accelerometer has 14 bits
    xAccel = rollAccel.x / 256.0;
    yAccel = rollAccel.y / 256.0;
    zAccel = rollAccel.z / 256.0;

    tf = ti + dt;
    // Do one step of ODE solver
    //euler_dn1(dnx, ti, dt, xi, xf, neqn);
    rk4_dn1(dnx, ti, dt, xi, xf, neqn);

    // prepare for the next step
    ti = tf;
    for (uint8_t i = 0; i<=neqn-1; i = i+1)
    {
        xi[i] = xf[i];
    }
    len = 1;


    int16_t scxc = 0;
    int16_t scyc = 0;
/*
    // Using center of screen as orgin, position ball  proportional to x,y component of rollAccel
    //plotCircle(64 + (rollAccel.x>>2), 32 - (rollAccel.y>>3), BTN_RAD);

    // Using center of screen as orgin, position ball on circle of radius 32 with direction x,y component of rollAccel
    //int16_t xc = rollAccel.x;
    //int16_t yc = rollAccel.y;


    len = sqrt(xc*xc + yc*yc);
    //uint16_t len = sqrt(xc*xc + yc*yc);
    //uint16_t len = norm(xc, yc);
*/

    if (len>0) {
        // scale normalized vector to length 28 to keep ball within bounds of screen
        //scxc = ((xc*28) / len); // for rolling ball
        //scyc = ((yc*28) / len); // for rolling ball
        //scxc = 28.0 * xc / len; // for rolling ball using Floating point
        //scyc = 28.0 * yc / len; // for rolling ball using Floating point
        scxc = -28.0 * cos(xf[0]);
        scyc =  28.0 * sin(xf[0]);
	//os_printf("100th %d, 100x %d, 100y %d\n", (int)(100*xf[0]), (int)(100*scxc), (int)(100*scyc));
	//os_printf("xc %d, yc %d, len %d scxc %d scyc %d\n", xc, yc, len, scxc, scyc);
        plotCircle(64 + scxc, 32 - scyc, 5);
        plotCircle(64 + scxc, 32 - scyc, 3);
        plotCircle(64 + scxc, 32 - scyc, 1);
    }

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
