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
void ICACHE_FLASH_ATTR initializeConditionsForODE(uint8_t Method);
void ICACHE_FLASH_ATTR rollSampleHandler(int32_t samp);
void ICACHE_FLASH_ATTR rollButtonCallback(uint8_t state __attribute__((unused)),
        int button, int down);
void ICACHE_FLASH_ATTR rollAccelerometerHandler(accel_t* accel);

void ICACHE_FLASH_ATTR roll_updateDisplay(void);
//uint16_t ICACHE_FLASH_ATTR norm(int16_t xc, int16_t yc);
void ICACHE_FLASH_ATTR setRollLeds(led_t* ledData, uint8_t ledDataLen);
void dnxdampedpendulum(FLOATING, FLOATING [], FLOATING [], int );
void dnx2dvelocity(FLOATING, FLOATING [], FLOATING [], int );
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
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = rollAccelerometerHandler
};

struct
{
    uint8_t currentMethod;
    uint8_t numMethods;
    accel_t Accel;
    uint8_t ButtonState;
    uint8_t Brightnessidx;
    led_t leds[NUM_LIN_LEDS];
    int LedCount;
    FLOATING scxc;
    FLOATING scyc;


    /* global variables for ODE */
    //const FLOATING gravity = 9.81;               // free fall acceleration in m/s^2
    //const FLOATING mass = 1.0;                // mass of a projectile in kg
    //const FLOATING radconversion = 3.1415926/180.0;  // radians
    uint8_t numberoffirstordereqn;                          // number of first-order equations
#define MAX_EQNS 4                     // MAX number of first-order equations
    FLOATING ti;
    FLOATING tf;
    FLOATING dt;
    FLOATING xi[MAX_EQNS];
    FLOATING xf[MAX_EQNS];
    FLOATING v0;
    FLOATING a0;
    FLOATING xAccel;
    FLOATING yAccel;
    FLOATING zAccel;
    FLOATING len;

    void (*rhs_fun_ptr)(FLOATING, FLOATING*, FLOATING*, int);
    void (*adjustment_fun_ptr)(FLOATING, FLOATING*, FLOATING*, int);

} roll;

/*============================================================================
 * Functions
 *==========================================================================*/


/**
 * Initializer for roll
 */
void ICACHE_FLASH_ATTR rollEnterMode(void)
{
    roll.currentMethod = 0;
    roll.numMethods = 4;
    //roll.Accel = {0};
    roll.ButtonState = 0;
    roll.Brightnessidx = 2;
    roll.LedCount = 0;
    roll.scxc = 0;
    roll.scyc = 0;
    enableDebounce(false);
    initializeConditionsForODE(roll.currentMethod);

}

/**
 * Called when roll is exited
 */
void ICACHE_FLASH_ATTR rollExitMode(void)
{

}

/**
 * initial information for ODEs
*/
void ICACHE_FLASH_ATTR initializeConditionsForODE(uint8_t Method)
{

    /* for thrown ball
        roll.numberoffirstordereqn = 4;
        roll.ti = 0.0;                // initial value for variable t
        roll.v0 = 180.0;              // initial speed (m/s)
        roll.a0 =  45.0;              // initial angle (degrees)
        roll.xi[0] = 0.0;             // initial position in x (m)
        roll.xi[1] = 0.0;             // initial position in y (m)
        roll.xi[2] = v0*cos(a0*rad);  // initial speed in x direction (m.s)
        roll.xi[3] = v0*sin(a0*rad);  // initial speed in y direction (m/s)
        roll.dt = 0.1;                // step size for integration (s)
    */
    switch(Method)
    {
        case 0:
            // For damped pendulum
            roll.numberoffirstordereqn = 2;
            roll.ti = 0.0;                // initial value for variable t
            roll.xi[0] = 0.0;             // initial angle position in (radians)
            roll.xi[1] = 0.0;             // initial angular speed in radians/ sec
            roll.dt = 0.1;                // step size for integration (s)
            roll.rhs_fun_ptr = &dnxdampedpendulum;
            roll.adjustment_fun_ptr = NULL;
            break;
        case 1:
            // For velocity controlled ball in plane
            roll.numberoffirstordereqn = 2;
            roll.ti = 0.0;                // initial value for variable t
            roll.xi[0] = 0.5;             // initial xcoor position
            roll.xi[1] = 0.5;             // initial ycoor
            roll.dt = 0.1;                // step size for integration (s)
            roll.rhs_fun_ptr = &dnx2dvelocity;
            break;
        default:
            (void)0;
    }
}



/*==== RHS of ODE ===============================================*/

/*
// Motion of thrown ball roll.numberoffirstordereqn=4, x = [xcoor, ycoor, xdot, ydot]
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


// Motion of damped rigid pendulum with gravity the downward component
// of the accelerometer
// Here roll.numberoffirstordereqn = 2, x is [th, thdot] position in radian, speed in radians/sec
void ICACHE_FLASH_ATTR dnxdampedpendulum(FLOATING t, FLOATING x[], FLOATING dx[], int n)
{
    // to stop warning that t and n not used
    (void)t;
    (void)n;
    //first order
    dx[0] = x[1];
    // second order
    FLOATING down = atan2(-roll.yAccel, -roll.xAccel);
    //os_printf("100down = %d\n", (int)(100*down)); // down 0 is with positve x on accel pointing down
    FLOATING force = 0.0; // later used for button replulsion
    //os_printf("%d\n", (int)(100*roll.zAccel)); //roll.xAccel can exceed 1
    //NOTE as scaled accelerations can exceed 1 this computation sometime got error and then got negative overflow and all stopped working
    //dx[1] = force + -gravity * sqrt(1.0 - pow(roll.zAccel, 2)) / LEN_PENDULUM * sin(x[0] - down) - .05 * x[1];
    //USE safer calculation that won't get sqrt of negative number
    dx[1] = force + -gravity * sqrt(pow(roll.xAccel, 2) + pow(roll.yAccel,
                                    2)) / LEN_PENDULUM * sin(x[0] - down) - .05 * x[1];
}

// Here 1st Order motion with velocity from accelerometer
void ICACHE_FLASH_ATTR dnx2dvelocity(FLOATING t, FLOATING x[], FLOATING dx[], int n)
{
    // to stop warning that t and n not used
    (void)t;
    (void)n;
    (void)x;
    FLOATING force = 0.0;
    //FLOATING friction = 1.0;
    FLOATING gmult = 200;
    //first order
    dx[0] = force + gmult * roll.xAccel;
    dx[1] = force + gmult * roll.yAccel;
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
    roll.xAccel = roll.Accel.x / 256.0;
    roll.yAccel = roll.Accel.y / 256.0;
    roll.zAccel = roll.Accel.z / 256.0;
    //os_printf("%d %d %d\n", (int)(100 * roll.xAccel), (int)(100 * roll.yAccel), (int)(100 * roll.zAccel));

    switch (roll.currentMethod)
    {
        case 0:
            roll.tf = roll.ti + roll.dt;
            // Do one step of ODE solver assigned to rhs_fun_pointer
            //euler_dn1(dnx, roll.ti, dt, roll.xi, roll.xf, roll.numberoffirstordereqn);
            //rk4_dn1(dnx, roll.ti, roll.dt, roll.xi, roll.xf, roll.numberoffirstordereqn);
            rk4_dn1((*roll.rhs_fun_ptr), roll.ti, roll.dt, roll.xi, roll.xf, roll.numberoffirstordereqn);
            break;
        case 1:
            roll.tf = roll.ti + roll.dt;
            euler_dn1((*roll.rhs_fun_ptr), roll.ti, roll.dt, roll.xi, roll.xf, roll.numberoffirstordereqn);
            break;
        default:
            (void)0;
    }

    // prepare for the next step if are solving an ODE and
    // perform any adjustments such as
    // prevent hitting walls by stopping or bouncing
    // torus wrapping
    // simple case 4 walls bounding the screen
    // migt use *adjustment_fun_ptr(roll.xi, roll.xf);

    int wrap[] = {127, 63};
    switch (roll.currentMethod)
    {
        case 0:
            roll.ti = roll.tf;
            for (uint8_t i = 0; i < roll.numberoffirstordereqn; i++)
            {
                roll.xi[i] = roll.xf[i];
                //os_printf("%d ", (int)(100 * roll.xi[i]));
            }
            //os_printf("\n");
            break;
        case 1:
            roll.ti = roll.tf;
            for (uint8_t i = 0; i < roll.numberoffirstordereqn; i++)
            {
                if ((roll.xf[i] >= 0) && (roll.xf[i] <= wrap[i]))
                {
                    roll.xi[i] = roll.xf[i];
                }
                else if (roll.xf[i] < 0)
                {
                    roll.xi[i] = roll.xf[i] + wrap[i];
                }
                else
                {
                    roll.xi[i] = roll.xf[i] - wrap[i];
                }
            }
            break;
        default:
            (void)0;
    }

    // Insure solution coordinates on OLED for the moving ball
    // OLED xcoord from 0 (left) to 127, ycoord from 0(top) to 63

    switch (roll.currentMethod)
    {
        case 0:
            roll.scxc = 64.0 - 28.0 * cos(roll.xi[0]);
            roll.scyc = 32.0 - 28.0 * sin(roll.xi[0]);
            break;
        case 1:
            roll.scxc = roll.xi[0];
            roll.scyc = roll.xi[1];
            break;

        case 2:
            // Using center of screen as orgin, position ball  proportional to x,y component of Accel
            roll.scxc = 64 + (roll.Accel.x >> 2);
            roll.scyc = 32 + (roll.Accel.y >> 3);
            break;
        case 3:
            // Using center of screen as orgin, position ball on circle of radius 32 with direction x,y component of Accel
            // acts as level with ball at lowest spot
            roll.scxc = roll.Accel.x;
            roll.scyc = roll.Accel.y;
            roll.len = sqrt(roll.scxc * roll.scxc + roll.scyc * roll.scyc);
            if (roll.len > 0)
            {
                // scale normalized vector to length 28 to keep ball within bounds of screen
                roll.scxc = 64.0 + 28.0 * roll.scxc / roll.len;
                roll.scyc = 32.0 + 28.0 * roll.scyc / roll.len;
            }
            break;

        default:
            (void)0;
    }

    // Draw virtual ball
    plotCircle(roll.scxc, roll.scyc, 5, WHITE);
    plotCircle(roll.scxc, roll.scyc, 3, WHITE);
    plotCircle(roll.scxc, roll.scyc, 1, WHITE);

    //os_printf("(%d, %d\n", (int)roll.scxc, (int)roll.scyc);

    // LEDs, all off
    ets_memset(roll.leds, 0, sizeof(roll.leds));
    //roll.leds[NUM_LIN_LEDS] = {{0}};
#define GAP 1

    /*  Python
        for led in self.leds:
            led.alpha = (1 - abs(self.ball.position - led.position)/(30+2*self.size.h/2.5))**3
            if self.framecount % 1 == 0:
                led.color = colorsys.hsv_to_rgb(self.ball.meanspeed,1,1)
    */

    for (uint8_t indLed = 0; indLed < NUM_LIN_LEDS / GAP; indLed++)
    {
        int16_t ledy = Ssinonlytable[((indLed << 8) * GAP / NUM_LIN_LEDS + 0x80) % 256] * 28 / 1500; // from -1500 to 1500
        int16_t ledx = Ssinonlytable[((indLed << 8) * GAP / NUM_LIN_LEDS + 0xC0) % 256] * 28 / 1500;
        roll.len = sqrt((roll.scxc - 64 - ledx) * (roll.scxc - 64 - ledx) + (-roll.scyc + 32 - ledy) *
                        (-roll.scyc + 32 - ledy));
        //roll.len = norm(roll.scxc - ledx, roll.scyc - ledy);
        uint8_t glow = 255 * pow(1.0 - (roll.len / 56.0), 3);
        //os_printf("%d %d %d %d %d %d %d \n",indLed, ledx, ledy, scxc, roll.scyc, roll.len, 255 - roll.len * 4);
        //roll.leds[GAP*indLed].r = 255 - roll.len * 4;
        roll.leds[GAP * indLed].r = glow;
    }
    setRollLeds(roll.leds, sizeof(roll.leds));

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
    roll.ButtonState = state;
    roll_updateDisplay();

    if(down)
    {
        if(2 == button)
        {
            // Cycle movement methods
            roll.currentMethod = (roll.currentMethod + 1) % roll.numMethods;
            os_printf("roll.currentMethod = %d\n", roll.currentMethod);
            //reset init conditions for new method
            initializeConditionsForODE(roll.currentMethod);
        }
        if(1 == button)
        {
            // Cycle brightnesses
            roll.Brightnessidx = (roll.Brightnessidx + 1) %
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
    roll.Accel.x = accel->y;
    roll.Accel.y = accel->x;
    roll.Accel.z = accel->z;
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
        ledData[i].r = ledData[i].r / rollBrightnesses[roll.Brightnessidx];
        ledData[i].g = ledData[i].g / rollBrightnesses[roll.Brightnessidx];
        ledData[i].b = ledData[i].b / rollBrightnesses[roll.Brightnessidx];
    }
    setLeds(ledData, ledDataLen);
}
