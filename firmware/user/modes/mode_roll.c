/*
 * mode_roll.c
 *
 *  Created on: 4 Aug 2019
 *      Author: bbkiwi
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
#include "bresenham.h"
#include "buttons.h"
#include "math.h"
#include "buzzer.h" // music notes
#include "hpatimer.h" //buzzer functions
#include "mode_music.h"
#include "mode_color_movement.h"


/*============================================================================
 * Defines
 *==========================================================================*/
//NOTE in ode_solvers.h is #define of FLOATING float    or double to test

// If want to simulate 6 LED barrel on dev-kit or bbkiwi or other with more than 6 leds
#define USE_6_LEDS
#ifdef USE_6_LEDS
    #define USE_NUM_LEDS 6
#else
    #define USE_NUM_LEDS NUM_LIN_LEDS
#endif
// LEDs relation to screen
#define LED_UPPER_LEFT LED_1
#define LED_UPPER_MID LED_2
#define LED_UPPER_RIGHT LED_3
#define LED_LOWER_RIGHT LED_4
#define LED_LOWER_MID LED_5
#define LED_LOWER_LEFT LED_6

#define LEN_PENDULUMR sqrt(2.0)
#define LEN_PENDULUMG sqrt(3.0)
#define LEN_PENDULUMB sqrt(5.0)
#define SPRINGCONSTR 5.0
#define SPRINGCONSTG 2.0
#define SPRINGCONSTB 45.0

#define MAX_NUM_NOTES 30

// update task (16 would give 60 fps like ipad, need read accel that fast too?)
#define UPDATE_TIME_MS 16
/*============================================================================
 * Prototypes
 *==========================================================================*/
void ICACHE_FLASH_ATTR rollInitMode(void);
void ICACHE_FLASH_ATTR rollExitMode(void);
void ICACHE_FLASH_ATTR initializeConditionsForODE(uint8_t Method);
void ICACHE_FLASH_ATTR roll_updateDisplay(void);
void ICACHE_FLASH_ATTR rollButtonCallback(uint8_t state __attribute__((unused)),
        int button, int down);
void ICACHE_FLASH_ATTR rollAccelerometerHandler(accel_t* accel);


//uint16_t ICACHE_FLASH_ATTR norm(int16_t xc, int16_t yc);
void ICACHE_FLASH_ATTR setRollLeds(led_t* ledData, uint8_t ledDataLen);
// other protypes in mode_roll.h and ode_solvers.h

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
    .fnEnterMode = rollInitMode,
    .fnExitMode = rollExitMode,
    .fnButtonCallback = rollButtonCallback,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = rollAccelerometerHandler,
    .menuImageData = mnu_colorshake_0,
    .menuImageLen = sizeof(mnu_colorshake_0)
};

struct
{
    uint8_t currentMethod;
    uint8_t numMethods;
    accel_t Accel;
    accel_t AccelHighPass;
    uint8_t ButtonState;
    uint8_t Brightnessidx;
    led_t leds[NUM_LIN_LEDS];
    uint8_t ledOrderInd[6];
    int LedCount;
    notePeriod_t midiNote;
    uint8_t numNotes;
    uint8_t midiScale[MAX_NUM_NOTES];
    FLOATING scxc;
    FLOATING scyc;
    FLOATING scxchold;
    FLOATING scychold;
    FLOATING scxcr;
    FLOATING scycr;
    FLOATING scxcg;
    FLOATING scycg;
    FLOATING scxcb;
    FLOATING scycb;

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
    FLOATING xir[MAX_EQNS], xfr[MAX_EQNS];
    FLOATING xig[MAX_EQNS], xfg[MAX_EQNS];
    FLOATING xib[MAX_EQNS], xfb[MAX_EQNS];
    FLOATING v0;
    FLOATING a0;
    bool useHighPassAccel;
    bool useSmooth;
    FLOATING xAccel;
    FLOATING yAccel;
    FLOATING zAccel;
    FLOATING alphaSlow;
    FLOATING alphaSmooth;
    FLOATING xAccelSlowAve;
    FLOATING yAccelSlowAve;
    FLOATING zAccelSlowAve;
    FLOATING xAccelHighPassSmoothed;
    FLOATING yAccelHighPassSmoothed;
    FLOATING zAccelHighPassSmoothed;
    FLOATING len;
    FLOATING lenr;
    FLOATING leng;
    FLOATING lenb;
    FLOATING totalenergyr;
    FLOATING totalenergyg;
    FLOATING totalenergyb;
    int16_t countframes;
    pendParam pendulumParametersRed;
    pendParam pendulumParametersGreen;
    pendParam pendulumParametersBlue;
    springParam springParametersRed;
    springParam springParametersGreen;
    springParam springParametersBlue;
    pendParam pendulumParameters;
    velParam velocityParameters;
    void (*rhs_fun_ptr)(FLOATING, FLOATING*, FLOATING*, int, FLOATING*);
    void (*adjustment_fun_ptr)(FLOATING, FLOATING*, FLOATING*, int, FLOATING*);

} roll;

static os_timer_t timerHandleUpdate = {0};
const FLOATING pi = 3.15159;

/*============================================================================
 * Functions
 *==========================================================================*/
/**
 * Initializer for roll
 */
void ICACHE_FLASH_ATTR rollInitMode(void)
{

    // Start the update loop.
    os_timer_disarm(&timerHandleUpdate);
    os_timer_setfn(&timerHandleUpdate, (os_timer_func_t*)roll_updateDisplay, NULL);
    os_timer_arm(&timerHandleUpdate, UPDATE_TIME_MS, 1);
    enableDebounce(false);
    rollEnterMode(0);
}

/**
 * Initializer for roll
 */
void ICACHE_FLASH_ATTR rollEnterMode(uint8_t method)
{
    roll.currentMethod = method;
    roll.numMethods = 12;
    //roll.Accel = {0};
    roll.ButtonState = 0;
    roll.Brightnessidx = 2;
    roll.LedCount = 0;
    roll.scxc = 0;
    roll.scyc = 0;
    roll.scxcr = 0;
    roll.scycr = 0;
    roll.scxcg = 0;
    roll.scycg = 0;
    roll.scxcb = 0;
    roll.scycb = 0;
    roll.alphaSlow = 0.02; // for finding long term moving average
    roll.alphaSmooth = 0.3; // for slight smoothing of High Pass Accel
    roll.useSmooth = true;
    roll.ledOrderInd[0] = LED_UPPER_LEFT;
    roll.ledOrderInd[1] = LED_LOWER_LEFT;
    roll.ledOrderInd[2] = LED_LOWER_MID;
    roll.ledOrderInd[3] = LED_LOWER_RIGHT;
    roll.ledOrderInd[4] = LED_UPPER_RIGHT;
    roll.ledOrderInd[5] = LED_UPPER_MID;


    roll.numNotes = 9;
    // uint8_t intervals[] = {2, 3, 2, 2, 3}; // pentatonic
    // generateScale(roll.midiScale, roll.numNotes, intervals, sizeof(intervals) );
    initializeConditionsForODE(roll.currentMethod);

}

/**
 * Called when roll is exited
 */
void ICACHE_FLASH_ATTR rollExitMode(void)
{
    os_timer_disarm(&timerHandleUpdate);
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
        case 1:
            // For damped pendulum
            roll.numberoffirstordereqn = 2;
            roll.ti = 0.0;                // initial value for variable t
            roll.xi[0] = 0.0;             // initial angle position in (radians)
            roll.xi[1] = 0.0;             // initial angular speed in radians/ sec
            roll.dt = 0.1;                // step size for integration (s)
            roll.rhs_fun_ptr = &dnxdampedpendulum;
            roll.pendulumParameters = (pendParam)
            {
                .damping = 0.9,
                .lenPendulum = 1,
                .gravity  = 9.81,
                .force = 0
            };
            roll.adjustment_fun_ptr = NULL;
            break;
        case 2:
        case 3:
            // For velocity controlled ball in plane
            roll.numberoffirstordereqn = 2;
            roll.ti = 0.0;                // initial value for variable t
            roll.xi[0] = 0.5;             // initial xcoor position
            roll.xi[1] = 0.5;             // initial ycoor
            roll.dt = 0.1;                // step size for integration (s)
            roll.rhs_fun_ptr = &dnx2dvelocity;
            roll.velocityParameters = (velParam)
            {
                .gmult = 200,
                .force = 0
            };
            break;
        case 8:
        case 9:
            // For damped spring
            roll.numberoffirstordereqn = 2;
            roll.ti = 0.0;                // initial value for variable t
            // Choose initial position so all have same max potential energy
            roll.xir[0] = sqrt(200 / SPRINGCONSTR) - pi / 2; // initial position in (m)
            roll.xir[1] = 0.0;                              // initial speed in m/ sec
            roll.xig[0] = sqrt(200 / SPRINGCONSTG) - pi / 2; // initial position in (m)
            roll.xig[1] = 0.0;
            roll.xib[0] = sqrt(200 / SPRINGCONSTB) - pi / 2; // initial position in (m)
            roll.xib[1] = 0.0;
            roll.dt = 0.1;                // step size for integration (s)
            roll.rhs_fun_ptr = &dnxdampedspring;
            roll.springParametersRed = (springParam)
            {
                .damping = 0.2,
                .springConstant = SPRINGCONSTR,
                .force = 0
            };
            roll.springParametersGreen = (springParam)
            {
                .damping = 0.2,
                .springConstant = SPRINGCONSTG,
                .force = 0
            };
            roll.springParametersBlue = (springParam)
            {
                .damping = 0.1,
                .springConstant = SPRINGCONSTB,
                .force = 0
            };
            roll.adjustment_fun_ptr = NULL;
            break;
        case 10:
        case 11:
            // For damped pendulum
            roll.numberoffirstordereqn = 2;
            roll.ti = 0.0;                // initial value for variable t
            roll.xir[0] = 0.0;             // initial angle position in (radians)
            roll.xir[1] = 0.0;             // initial angular speed in radians/ sec
            roll.xig[0] = 0.0;             // initial angle position in (radians)
            roll.xig[1] = 0.0;             // initial angular speed in radians/ sec
            roll.xib[0] = 0.0;             // initial angle position in (radians)
            roll.xib[1] = 0.0;             // initial angular speed in radians/ sec
            roll.dt = 0.1;                // step size for integration (s)
            roll.rhs_fun_ptr = &dnxdampedpendulum;
            roll.pendulumParametersRed = (pendParam)
            {
                .damping = 0.9,
                .lenPendulum = LEN_PENDULUMR,
                .gravity  = 9.81,
                .force = 0
            };
            roll.pendulumParametersGreen = (pendParam)
            {
                .damping = 0.9,
                .lenPendulum = LEN_PENDULUMG,
                .gravity  = 9.81,
                .force = 0
            };
            roll.pendulumParametersBlue = (pendParam)
            {
                .damping = 0.9,
                .lenPendulum = LEN_PENDULUMB,
                .gravity  = 9.81,
                .force = 0
            };
            roll.adjustment_fun_ptr = NULL;
            break;
        default:
            (void)0;
    }
}



/*==== RHS of ODE ===============================================*/

/*
// Motion of thrown ball roll.numberoffirstordereqn=4, x = [xcoor, ycoor, xdot, ydot]
void ICACHE_FLASH_ATTR dnx(FLOATING t, FLOATING x[], FLOATING dx[], int n, FLOATING parameters[])
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
void ICACHE_FLASH_ATTR dnxdampedpendulum(FLOATING t, FLOATING x[], FLOATING dx[], int n, FLOATING parameters[])
{

    // cast p1 to point to structure and assign to p
    pendParam* p = (pendParam*)parameters;
    // can now refer to the paramters p->a, p->b rather than p1[0], p1[1]   // to stop warning that t and n not used
    (void)t;
    (void)n;
    //first order
    dx[0] = x[1];
    // second order
    FLOATING down = atan2(-p->yAccel, -p->xAccel);
    //NOTE as scaled accelerations can exceed 1 this computation sometime got error and then got negative overflow and all stopped working
    //dx[1] = force + -gravity * sqrt(1.0 - pow(roll.zAccel, 2)) / LEN_PENDULUM * sin(x[0] - down) - .05 * x[1];
    //USE safer calculation that won't get sqrt of negative number
    dx[1] = p->force + -p->gravity * sqrt(pow(p->xAccel, 2) + pow(p->yAccel,
                                          2)) / p->lenPendulum * sin(x[0] - down) - p->damping * x[1];
}


// Motion of damped spring with gravity the downward component
// of the accelerometer
// Here numberoffirstordereqn = 2, x is [th, thdot] position in radian, speed in radians/sec
void ICACHE_FLASH_ATTR dnxdampedspring(FLOATING t, FLOATING x[], FLOATING dx[], int n, FLOATING parameters[])
{
    // cast p1 to point to structure and assign to p
    springParam* p = (springParam*)parameters;
    // can now refer to the paramters p->a, p->b rather than p1[0], p1[1]

    // to stop warning that t and n not used
    (void)t;
    (void)n;
    //os_printf("IN  t %d x[0] %d x[1] %d dx[0] %d dx[1] %d n %d\n", (int)(100 * t), (int)(100 * x[0]), (int)(100 * x[1]),
    //          (int)(100 * dx[0]), (int)(100 * dx[1]), n);
    //first order
    dx[0] = x[1];
    // second order
    FLOATING down = atan2(-p->yAccel, -p->xAccel);
    //os_printf("100down = %d\n", (int)(100*down)); // down 0 is with positve x on accel pointing down
    dx[1] = p->force - p->springConstant * (x[0] + pi / 2)  - p->damping * x[1] - p->springConstant * sqrt(pow(p->xAccel,
            2) + pow(p->yAccel,
                     2))  * sin(x[0] - down);
    //os_printf("OUT t %d x[0] %d x[1] %d dx[0] %d dx[1] %d n %d\n\n", (int)(100 * t), (int)(100 * x[0]), (int)(100 * x[1]),
    //          (int)(100 * dx[0]), (int)(100 * dx[1]), n);

}

// Here 1st Order motion with velocity from accelerometer
void ICACHE_FLASH_ATTR dnx2dvelocity(FLOATING t, FLOATING x[], FLOATING dx[], int n, FLOATING parameters[])
{
    velParam* p = (velParam*)parameters;
    // to stop warning that t and n not used
    (void)t;
    (void)n;
    (void)x;
    //first order
    dx[0] = p->force + p->gmult * p->xAccel;
    dx[1] = p->force + p->gmult * p->yAccel;
}


led_t* ICACHE_FLASH_ATTR roll_updateDisplayComputations(int16_t xAccel, int16_t yAccel, int16_t zAccel)
{
    //NOTE bug in Expressif OS can't print floating point! Must cast as int
    //Save accelerometer reading in global storage
    //TODO can get values bigger than 1. here, my accelerometer has 14 bits


    roll.xAccel = xAccel / 256.0;
    roll.yAccel = yAccel / 256.0;
    roll.zAccel = zAccel / 256.0;


    //os_printf("%d %d %d\n", (int)(100 * roll.xAccel), (int)(100 * roll.yAccel), (int)(100 * roll.zAccel));

    switch (roll.currentMethod)
    {
        case 0:
        case 1:
            roll.tf = roll.ti + roll.dt;
            // Do one step of ODE solver assigned to rhs_fun_pointer
            //euler_dn1(dnx, roll.ti, dt, roll.xi, roll.xf, roll.numberoffirstordereqn);
            //rk4_dn1(dnx, roll.ti, roll.dt, roll.xi, roll.xf, roll.numberoffirstordereqn);
            roll.pendulumParameters.xAccel = roll.xAccel;
            roll.pendulumParameters.yAccel = roll.yAccel;
            rk4_dn1((*roll.rhs_fun_ptr), roll.ti, roll.dt, roll.xi, roll.xf, roll.numberoffirstordereqn,
                    (FLOATING*)&roll.pendulumParameters);
            break;
        case 2:
        case 3:
            roll.tf = roll.ti + roll.dt;
            roll.velocityParameters.xAccel = roll.xAccel;
            roll.velocityParameters.yAccel = roll.yAccel;
            euler_dn1((*roll.rhs_fun_ptr), roll.ti, roll.dt, roll.xi, roll.xf, roll.numberoffirstordereqn,
                      (FLOATING*)&roll.velocityParameters);
            break;

        case 8:
        case 9:
            // Damped Spring
            roll.tf = roll.ti + roll.dt;
            // Do one step of ODE solver to dnx assigned to rhs_fun_pointer
            // Need to update accel values in parameters
            roll.springParametersRed.xAccel = roll.xAccel;
            roll.springParametersRed.yAccel = roll.yAccel;
            roll.springParametersGreen.xAccel = roll.xAccel;
            roll.springParametersGreen.yAccel = roll.yAccel;
            roll.springParametersBlue.xAccel = roll.xAccel;
            roll.springParametersBlue.yAccel = roll.yAccel;
            rk4_dn1((*roll.rhs_fun_ptr), roll.ti, roll.dt, roll.xir, roll.xfr, roll.numberoffirstordereqn,
                    (FLOATING*)&roll.springParametersRed);
            rk4_dn1((*roll.rhs_fun_ptr), roll.ti, roll.dt, roll.xig, roll.xfg, roll.numberoffirstordereqn,
                    (FLOATING*)&roll.springParametersGreen);
            rk4_dn1((*roll.rhs_fun_ptr), roll.ti, roll.dt, roll.xib, roll.xfb, roll.numberoffirstordereqn,
                    (FLOATING*)&roll.springParametersBlue);
            break;
        case 10:
        case 11:
            // Damped Pendulum
            roll.tf = roll.ti + roll.dt;
            // Do one step of ODE solver to dnx assigned to rhs_fun_pointer
            // Need to update accel values in parameters
            roll.pendulumParametersRed.xAccel = roll.xAccel;
            roll.pendulumParametersRed.yAccel = roll.yAccel;
            roll.pendulumParametersGreen.xAccel = roll.xAccel;
            roll.pendulumParametersGreen.yAccel = roll.yAccel;
            roll.pendulumParametersBlue.xAccel = roll.xAccel;
            roll.pendulumParametersBlue.yAccel = roll.yAccel;
            //Old but don't need pointer
            //rk4_dn1((*rhs_fun_ptrr), ti, dt, xir, xfr, numberoffirstordereqn, (FLOATING*)&pendulumParametersRed);
            //rk4_dn1((*rhs_fun_ptrg), ti, dt, xig, xfg, numberoffirstordereqn, (FLOATING*)&pendulumParametersGreen);
            //rk4_dn1((*rhs_fun_ptrb), ti, dt, xib, xfb, numberoffirstordereqn, (FLOATING*)&pendulumParametersBlue);
            //TODO thought should be &dandampedpendulum but either works but mode starts with spring wrong
            rk4_dn1(dnxdampedpendulum, roll.ti, roll.dt, roll.xir, roll.xfr, roll.numberoffirstordereqn,
                    (FLOATING*)&roll.pendulumParametersRed);
            rk4_dn1(dnxdampedpendulum, roll.ti, roll.dt, roll.xig, roll.xfg, roll.numberoffirstordereqn,
                    (FLOATING*)&roll.pendulumParametersGreen);
            rk4_dn1(dnxdampedpendulum, roll.ti, roll.dt, roll.xib, roll.xfb, roll.numberoffirstordereqn,
                    (FLOATING*)&roll.pendulumParametersBlue);
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
        case 1:
            roll.ti = roll.tf;
            for (uint8_t i = 0; i < roll.numberoffirstordereqn; i++)
            {
                roll.xi[i] = roll.xf[i];
                //os_printf("%d ", (int)(100 * roll.xi[i]));
            }
            //os_printf("\n");

            break;
        case 8:
        case 9:
        case 10:
        case 11:
            roll.ti = roll.tf;
            for (uint8_t i = 0; i < roll.numberoffirstordereqn; i++)
            {
                roll.xir[i] = roll.xfr[i];
                roll.xig[i] = roll.xfg[i];
                roll.xib[i] = roll.xfb[i];
            }
            break;
        case 2:
        case 3:
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
    FLOATING down = atan2(-roll.yAccel, -roll.xAccel);

    switch (roll.currentMethod)
    {
        case 0:
        case 1:
            roll.scxc = 64.0 - 28.0 * cos(roll.xi[0]);
            roll.scyc = 32.0 - 28.0 * sin(roll.xi[0]);
            break;
        case 2:
        case 3:
            roll.scxc = roll.xi[0];
            roll.scyc = roll.xi[1];
            break;

        case 4:
        case 5:
            // Using center of screen as orgin, position ball  proportional to x,y component of Accel
            roll.scxc = 64 + (roll.Accel.x >> 2);
            roll.scyc = 32 + (roll.Accel.y >> 3);
            break;

        case 6:
        // flat corresponds to the average postion and level measured as deviation from this
        case 7:
            // Here holding screen flat the ball seeks the lowest level
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
                roll.scxchold = roll.scxc;
                roll.scychold = roll.scyc;
            }
            else
            {
                roll.scxc = roll.scxchold;
                roll.scyc = roll.scychold;
            }
            break;

        case 8:
        case 9:
            roll.scxcr = 64.0 - 16 * (roll.xfr[0] + pi / 2);
            roll.scycr = 10.0;
            roll.scxcg = 64.0 - 16 * (roll.xfg[0] + pi / 2);
            roll.scycg = 32.0;
            roll.scxcb = 64.0 - 16 * (roll.xfb[0] + pi / 2);
            roll.scycb = 54.0;
            roll.totalenergyr = SPRINGCONSTR * (roll.xfr[0] + pi / 2) * (roll.xfr[0] + pi / 2) + roll.xfr[1] *
                                roll.xfr[1];
            roll.totalenergyg = SPRINGCONSTG * (roll.xfg[0] + pi / 2) * (roll.xfg[0] + pi / 2) + roll.xfg[1] *
                                roll.xfg[1];
            roll.totalenergyb = SPRINGCONSTB * (roll.xfb[0] + pi / 2) * (roll.xfb[0] + pi / 2) + roll.xfb[1] *
                                roll.xfb[1];
            break;
        case 10:
        case 11:
            roll.scxcr = 64.0 - 28.0 * cos(roll.xfr[0]);
            roll.scycr = 32.0 - 28.0 * sin(roll.xfr[0]);
            roll.scxcg = 64.0 - 28.0 * cos(roll.xfg[0]);
            roll.scycg = 32.0 - 28.0 * sin(roll.xfg[0]);
            roll.scxcb = 64.0 - 28.0 * cos(roll.xfb[0]);
            roll.scycb = 32.0 - 28.0 * sin(roll.xfb[0]);
            // Using this for pendulum
            roll.totalenergyr = gravity * LEN_PENDULUMR * (1 - cos(roll.xfr[0] - down))  + 0.5 * LEN_PENDULUMR * LEN_PENDULUMR *
                                roll.xfr[1] * roll.xfr[1];
            roll.totalenergyg = gravity * LEN_PENDULUMG * (1 - cos(roll.xfg[0] - down))  + 0.5 * LEN_PENDULUMG * LEN_PENDULUMG *
                                roll.xfg[1] * roll.xfg[1];
            roll.totalenergyb = gravity * LEN_PENDULUMB * (1 - cos(roll.xfb[0] - down))  + 0.5 * LEN_PENDULUMB * LEN_PENDULUMB *
                                roll.xfb[1] * roll.xfb[1];
            break;


        default:
            (void)0;
    }
    //TODO for development
    // char uiStr[32] = {0};
    // ets_snprintf(uiStr, sizeof(uiStr), "%d", roll.currentMethod);
    // plotText(57, 31, uiStr, IBM_VGA_8, WHITE);

    switch (roll.currentMethod)
    {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
            // Draw virtual ball
            plotCircle(roll.scxc, roll.scyc, 5, WHITE);
            plotCircle(roll.scxc, roll.scyc, 3, WHITE);
            plotCircle(roll.scxc, roll.scyc, 1, WHITE);
            //os_printf("(%d, %d)\n", (int)roll.scxc, (int)roll.scyc);
            // LEDs, all off
            ets_memset(roll.leds, 0, sizeof(roll.leds));
#define GAP 1

            /*  Python
                for led in self.leds:
                    led.alpha = (1 - abs(self.ball.position - led.position)/(30+2*self.size.h/2.5))**3
                    if self.framecount % 1 == 0:
                        led.color = colorsys.hsv_to_rgb(self.ball.meanspeed,1,1)
            */

            for (uint8_t indLed = 0; indLed < USE_NUM_LEDS / GAP; indLed++)
            {
                int16_t ledy = Ssinonlytable[((indLed << 8) * GAP / USE_NUM_LEDS + 0x80) % 256] * 28 / 1500; // from -1500 to 1500
                int16_t ledx = Ssinonlytable[((indLed << 8) * GAP / USE_NUM_LEDS + 0xC0) % 256] * 28 / 1500;
                roll.len = sqrt((roll.scxc - 64 - ledx) * (roll.scxc - 64 - ledx) + (-roll.scyc + 32 - ledy) *
                                (-roll.scyc + 32 - ledy));
                //roll.len = norm(roll.scxc - ledx, roll.scyc - ledy);
                uint8_t glow = 255 * pow(1.0 - (roll.len / 56.0), 3);
                //os_printf("%d %d %d %d %d %d %d \n",indLed, ledx, ledy, scxc, roll.scyc, roll.len, 255 - roll.len * 4);

#ifdef USE_6_LEDS
                roll.leds[roll.ledOrderInd[GAP * indLed]].r = glow;
#else
                roll.leds[GAP * indLed].r = glow;
#endif
            }
            break;
        case 8:
        case 9:
            plotText(roll.scxcr, roll.scycr, "R", IBM_VGA_8, WHITE);
            plotText(roll.scxcg, roll.scycg, "G", IBM_VGA_8, WHITE);
            plotText(roll.scxcb, roll.scycb, "B", IBM_VGA_8, WHITE);
            break;
        case 10:
        case 11:
            plotText(.95 * roll.scxcr, .95 * roll.scycr, "R", IBM_VGA_8, WHITE);
            plotText(.9 * roll.scxcg, .9 * roll.scycg, "G", IBM_VGA_8, WHITE);
            plotText(.85 * roll.scxcb, .85 * roll.scycb, "B", IBM_VGA_8, WHITE);
            //plotCircle(scxcr, scycr, 5, WHITE);
            //plotCircle(scxcr, scycr, 3, WHITE);
            //plotCircle(scxcr, scycr, 1, WHITE);
            //plotCircle(scxcg, scycg, 5, WHITE);
            //plotCircle(scxcg, scycg, 3, WHITE);
            //plotCircle(scxcg, scycg, 1, WHITE);
            //plotCircle(scxcb, scycb, 5, WHITE);
            //plotCircle(scxcb, scycb, 3, WHITE);
            //plotCircle(scxcb, scycb, 1, WHITE);
            break;
        default:
            (void)0;
    }

    switch (roll.currentMethod)
    {
        case 8:
        case 9:
#ifndef USE_6_LEDS
            for (uint8_t indLed = 0; indLed < NUM_LIN_LEDS ; indLed++)
            {
                roll.leds[indLed].r = roll.totalenergyr;
                roll.leds[indLed].g = roll.totalenergyg;
                roll.leds[indLed].b = roll.totalenergyb;
            }
#else
            for (uint8_t indLed = 0; indLed < 6 ; indLed++)
            {
                roll.leds[roll.ledOrderInd[indLed]].r = roll.totalenergyr;
                roll.leds[roll.ledOrderInd[indLed]].g = roll.totalenergyg;
                roll.leds[roll.ledOrderInd[indLed]].b = roll.totalenergyb;
            }
#endif
            break;
        case 10:
        case 11:
#define GAP 1
            for (uint8_t indLed = 0; indLed < USE_NUM_LEDS / GAP; indLed++)
            {
                int16_t ledy = Ssinonlytable[((indLed << 8) * GAP / USE_NUM_LEDS + 0x80) % 256] * 28 / 1500; // from -1500 to 1500
                int16_t ledx = Ssinonlytable[((indLed << 8) * GAP / USE_NUM_LEDS + 0xC0) % 256] * 28 / 1500;
                roll.lenr = sqrt((roll.scxcr - 64 - ledx) * (roll.scxcr - 64 - ledx) + (-roll.scycr + 32 - ledy) *
                                 (-roll.scycr + 32 - ledy));
                roll.leng = sqrt((roll.scxcg - 64 - ledx) * (roll.scxcg - 64 - ledx) + (-roll.scycg + 32 - ledy) *
                                 (-roll.scycg + 32 - ledy));
                roll.lenb = sqrt((roll.scxcb - 64 - ledx) * (roll.scxcb - 64 - ledx) + (-roll.scycb + 32 - ledy) *
                                 (-roll.scycb + 32 - ledy));
                //len = norm(roll.scxc - ledx, roll.scyc - ledy);
                uint8_t glowr = 255 * pow(1.0 - (roll.lenr / 56.0), 3);
                uint8_t glowg = 255 * pow(1.0 - (roll.leng / 56.0), 3);
                uint8_t glowb = 255 * pow(1.0 - (roll.lenb / 56.0), 3);
                //os_printf("%d %d %d %d %d %d %d \n",indLed, ledx, ledy, scxc, scyc, len, 255 - len * 4);
                //roll.leds[GAP*indLed].r = 255 - len * 4;
#ifdef USE_6_LEDS
                roll.leds[roll.ledOrderInd[GAP * indLed]].r = glowr;
                roll.leds[roll.ledOrderInd[GAP * indLed]].g = glowg;
                roll.leds[roll.ledOrderInd[GAP * indLed]].b = glowb;
#else
                roll.leds[GAP * indLed].r = glowr;
                roll.leds[GAP * indLed].g = glowg;
                roll.leds[GAP * indLed].b = glowb;
#endif
            }
            break;
        default:
            (void)0;
    }
    return roll.leds;
}

void ICACHE_FLASH_ATTR roll_updateDisplay(void)
{
    // Clear the display
    clearDisplay();

    switch (roll.currentMethod)
    {
        case 1:
        case 3:
        case 5:
        case 7:
        case 9:
        case 11:
            roll.useHighPassAccel = true;
            break;
        default:
            roll.useHighPassAccel = false;
            break;
    }

    (void)roll_updateDisplayComputations(roll.Accel.x, roll.Accel.y, roll.Accel.z);
    setRollLeds(roll.leds, sizeof(roll.leds));
    // Set midiNote
    //TODO are notes spread equally around circle?
    // uint8_t notenum = (int)(0.5 + roll.numNotes * atan2(roll.scxc - 64, roll.scyc - 32) / 2.0 / pi) + (roll.numNotes >> 1);
    // roll.midiNote = midi2note(roll.midiScale[notenum]);
    // if want continous change at each frame
    //setBuzzerNote(roll.midiNote);
    //os_printf("notenum = %d,   midi = %d,  roll.midiNote = %d\n", notenum, roll.midiScale[notenum], roll.midiNote);
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
    //roll_updateDisplay();

    if(down)
    {
        if(2 == button)
        {
            setBuzzerNote(roll.midiNote);
        }
        if(1 == button)
        {
            // Cycle brightnesses
            roll.Brightnessidx = (roll.Brightnessidx + 1) %
                                 (sizeof(rollBrightnesses) / sizeof(rollBrightnesses[0]));
            if (roll.Brightnessidx == 0)
            {
                // Cycle movement methods
                roll.currentMethod = (roll.currentMethod + 1) % roll.numMethods;
                os_printf("roll.currentMethod = %d\n", roll.currentMethod);
                //reset init conditions for new method
                initializeConditionsForODE(roll.currentMethod);
            }
        }
    }
    else
    {
        //os_printf("up \n");
        stopBuzzerSong();
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

    if (roll.useHighPassAccel)
    {
        roll.xAccelSlowAve = (1.0 - roll.alphaSlow) * roll.xAccelSlowAve + roll.alphaSlow * (float)roll.Accel.x;
        roll.yAccelSlowAve = (1.0 - roll.alphaSlow) * roll.yAccelSlowAve + roll.alphaSlow * (float)roll.Accel.y;
        roll.zAccelSlowAve = (1.0 - roll.alphaSlow) * roll.zAccelSlowAve + roll.alphaSlow * (float)roll.Accel.z;

        roll.Accel.x = roll.Accel.x - roll.xAccelSlowAve;
        roll.Accel.y = roll.Accel.y - roll.yAccelSlowAve;
        roll.Accel.z = roll.Accel.z - roll.zAccelSlowAve;
    }
    if (roll.useSmooth)
    {
        roll.xAccelHighPassSmoothed = (1.0 - roll.alphaSmooth) * roll.xAccelHighPassSmoothed + roll.alphaSmooth *
                                      (float)roll.Accel.x;
        roll.yAccelHighPassSmoothed = (1.0 - roll.alphaSmooth) * roll.yAccelHighPassSmoothed + roll.alphaSmooth *
                                      (float)roll.Accel.y;
        roll.zAccelHighPassSmoothed = (1.0 - roll.alphaSmooth) * roll.zAccelHighPassSmoothed + roll.alphaSmooth *
                                      (float)roll.Accel.z;

        roll.Accel.x = roll.xAccelHighPassSmoothed;
        roll.Accel.y = roll.yAccelHighPassSmoothed;
        roll.Accel.z = roll.zAccelHighPassSmoothed;
    }



    //roll_updateDisplay();
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
