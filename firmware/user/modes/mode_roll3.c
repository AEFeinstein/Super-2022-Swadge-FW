/*
 * mode_roll3.c
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
#include "MMA8452Q.h"
#include "bresenham.h"
#include "buttons.h"
#include "math.h"
#include "ode_solvers.h"
#include "ccconfig.h"

/*============================================================================
 * Defines
 *==========================================================================*/
//NOTE in ode_solvers.h is #define of FLOATING float    or double to test
#define LEN_PENDULUMR sqrt(2.0)
#define LEN_PENDULUMG sqrt(3.0)
#define LEN_PENDULUMB sqrt(5.0)
#define SPRINGCONSTR 5.0
#define SPRINGCONSTG 20.0
#define SPRINGCONSTB 45.0

// LEDs relation to screen
#define LED_UPPER_LEFT LED_1
#define LED_UPPER_MID LED_2
#define LED_UPPER_RIGHT LED_3
#define LED_LOWER_RIGHT LED_4
#define LED_LOWER_MID LED_5
#define LED_LOWER_LEFT LED_6

/*============================================================================
 * Prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR roll3EnterMode(void);
void ICACHE_FLASH_ATTR roll3ExitMode(void);
void ICACHE_FLASH_ATTR initializeConditionsForODERoll3(uint8_t currentMethod);
void ICACHE_FLASH_ATTR roll3SampleHandler(int32_t samp);
void ICACHE_FLASH_ATTR roll3ButtonCallback(uint8_t state __attribute__((unused)),
        int button, int down);
void ICACHE_FLASH_ATTR roll3AccelerometerHandler(accel_t* accel);

void ICACHE_FLASH_ATTR roll3_updateDisplay(void);
//uint16_t ICACHE_FLASH_ATTR norm(int16_t xc, int16_t yc);
void ICACHE_FLASH_ATTR setRoll3Leds(led_t* ledData, uint8_t ledDataLen);
void dnxdampedspring(FLOATING, FLOATING [], FLOATING [], int, FLOATING []);

/*============================================================================
 * Static Const Variables
 *==========================================================================*/

static const uint8_t roll3Brightnesses[] =
{
    0x01,
    0x08,
    0x40,
    0x80,
};



/*============================================================================
 * Variables
 *==========================================================================*/

swadgeMode roll3Mode =
{
    .modeName = "roll3",
    .fnEnterMode = roll3EnterMode,
    .fnExitMode = roll3ExitMode,
    .fnButtonCallback = roll3ButtonCallback,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = roll3AccelerometerHandler
};

uint8_t ledOrderIndroll3[] = {LED_UPPER_LEFT, LED_LOWER_LEFT, LED_LOWER_MID, LED_LOWER_RIGHT, LED_UPPER_RIGHT, LED_UPPER_MID};

uint8_t currentMethod = 0;
uint8_t numMethods = 4;
accel_t roll3Accel = {0};
uint8_t roll3ButtonState = 0;
uint8_t roll3BrightnessIdx = 0;
int roll3_ledCount = 0;
FLOATING scxcr = 0;
FLOATING scycr = 0;
FLOATING scxcg = 0;
FLOATING scycg = 0;
FLOATING scxcb = 0;
FLOATING scycb = 0;



/* global variables for ODE */
//const FLOATING gravity = 9.81;               // free fall acceleration in m/s^2
//const FLOATING mass = 1.0;                // mass of a projectile in kg
//const FLOATING radconversion = 3.1415926/180.0;  // radians
//uint8_t numberoffirstordereqn = 2;                          // number of first-order equations
#define MAX_EQNS 4                     // MAX number of first-order equations
const FLOATING pi = 3.15159;
FLOATING ti, tf, dt;
FLOATING xi[MAX_EQNS], xf[MAX_EQNS];
FLOATING xir[MAX_EQNS], xfr[MAX_EQNS];
FLOATING xig[MAX_EQNS], xfg[MAX_EQNS];
FLOATING xib[MAX_EQNS], xfb[MAX_EQNS];
FLOATING v0, a0;

FLOATING xAccel;
FLOATING yAccel;
FLOATING zAccel;
FLOATING lenr;
FLOATING leng;
FLOATING lenb;
FLOATING totalenergyr;
FLOATING totalenergyg;
FLOATING totalenergyb;
pendParam pendulumParametersRed;
pendParam pendulumParametersGreen;
pendParam pendulumParametersBlue;
springParam springParametersRed;
springParam springParametersGreen;
springParam springParametersBlue;

int16_t countframes;

void (*rhs_fun_ptr)(FLOATING, FLOATING*, FLOATING*, int, FLOATING*);
void (*rhs_fun_ptrr)(FLOATING, FLOATING*, FLOATING*, int, FLOATING*);
void (*rhs_fun_ptrg)(FLOATING, FLOATING*, FLOATING*, int, FLOATING*);
void (*rhs_fun_ptrb)(FLOATING, FLOATING*, FLOATING*, int, FLOATING*);
void (*adjustment_fun_ptr)(FLOATING, FLOATING*, FLOATING*, int, FLOATING*);


/*============================================================================
 * Functions
 *==========================================================================*/


/**
 * Initializer for roll3
 */
void ICACHE_FLASH_ATTR roll3EnterMode(void)
{
    enableDebounce(false);
    initializeConditionsForODERoll3(currentMethod);

}

/**
 * Called when roll3 is exited
 */
void ICACHE_FLASH_ATTR roll3ExitMode(void)
{

}

/**
 * initial information for ODEs
*/
void ICACHE_FLASH_ATTR initializeConditionsForODERoll3(uint8_t Method)
{
    os_printf("freq R %d / 100, G %d / 100, B %d / 100\n", (int)(200 * pi * sqrt(SPRINGCONSTR)),
              (int)(200 * pi * sqrt(SPRINGCONSTG)), (int)(200 * pi * sqrt(SPRINGCONSTB)));
    countframes = 0;
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
    switch(Method)
    {
        case 0:
        case 1:
            // For damped spring
            numberoffirstordereqn = 2;
            ti = 0.0;                // initial value for variable t
            // Choose initial position so all have same max potential energy
            xir[0] = sqrt(200 / SPRINGCONSTR) - pi / 2; // initial position in (m)
            xir[1] = 0.0;                              // initial speed in m/ sec
            xig[0] = sqrt(200 / SPRINGCONSTG) - pi / 2; // initial position in (m)
            xig[1] = 0.0;
            xib[0] = sqrt(200 / SPRINGCONSTB) - pi / 2; // initial position in (m)
            xib[1] = 0.0;
            dt = 0.1;                // step size for integration (s)
            rhs_fun_ptrr = &dnxdampedspring;
            rhs_fun_ptrg = &dnxdampedspring;
            rhs_fun_ptrb = &dnxdampedspring;
            springParametersRed = (springParam)
            {
                .damping = 0.2,
                .springConstant = SPRINGCONSTR,
                .force = 0
            };
            springParametersGreen = (springParam)
            {
                .damping = 0.2,
                .springConstant = SPRINGCONSTG,
                .force = 0
            };
            springParametersBlue = (springParam)
            {
                .damping = 0.1,
                .springConstant = SPRINGCONSTB,
                .force = 0
            };
            adjustment_fun_ptr = NULL;
            break;
        case 2:
        case 3:
            // For damped pendulum
            numberoffirstordereqn = 2;
            ti = 0.0;                // initial value for variable t
            xir[0] = 0.0;             // initial angle position in (radians)
            xir[1] = 0.0;             // initial angular speed in radians/ sec
            xig[0] = 0.0;             // initial angle position in (radians)
            xig[1] = 0.0;             // initial angular speed in radians/ sec
            xib[0] = 0.0;             // initial angle position in (radians)
            xib[1] = 0.0;             // initial angular speed in radians/ sec
            dt = 0.1;                // step size for integration (s)
            rhs_fun_ptrr = &dnxdampedpendulum;
            rhs_fun_ptrg = &dnxdampedpendulum;
            rhs_fun_ptrb = &dnxdampedpendulum;
            pendulumParametersRed = (pendParam)
            {
                .damping = 0.05,
                .lenPendulum = LEN_PENDULUMR,
                .gravity  = 9.81,
                .force = 0
            };
            pendulumParametersGreen = (pendParam)
            {
                .damping = 0.05,
                .lenPendulum = LEN_PENDULUMG,
                .gravity  = 9.81,
                .force = 0
            };
            pendulumParametersBlue = (pendParam)
            {
                .damping = 0.05,
                .lenPendulum = LEN_PENDULUMB,
                .gravity  = 9.81,
                .force = 0
            };
            adjustment_fun_ptr = NULL;
            break;
        default:
            (void)0;
    }
}



/*==== RHS of ODE ===============================================*/

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
    dx[1] = p->force - p->springConstant * (x[0] + pi / 2)  - p->damping * x[1] - p->springConstant * sqrt(pow(xAccel, 2) + pow(yAccel,
            2))  * sin(x[0] - down);
    //os_printf("OUT t %d x[0] %d x[1] %d dx[0] %d dx[1] %d n %d\n\n", (int)(100 * t), (int)(100 * x[0]), (int)(100 * x[1]),
    //          (int)(100 * dx[0]), (int)(100 * dx[1]), n);

}

// // Here 1st Order motion with velocity from accelerometer
// void ICACHE_FLASH_ATTR dnx2dvelocityRoll3(FLOATING t, FLOATING x[], FLOATING dx[], int n, FLOATING parameters[])
// {
//     // to stop warning that t and n not used
//     (void)t;
//     (void)n;
//     (void)x;
//     FLOATING force = 0.0;
//     //FLOATING friction = 1.0;
//     FLOATING gmult = 2;
//     //first order
//     dx[0] = force + gmult * xAccel;
//     dx[1] = force + gmult * yAccel;
// }



void ICACHE_FLASH_ATTR roll3_updateDisplay(void)
{
    // Clear the display
    clearDisplay();

    //NOTE bug in Expressif OS can't print floating point! Must cast as int
    //debug print for thrown ball
    //os_printf("100t = %d, x = %d, y = %d, vx = %d, vy = %d\n", (int)(100*ti), (int)xi[0], (int)xi[1], (int)xi[2], (int)xi[3]);

    //Save accelerometer reading in global storage
    //TODO can get values bigger than 1. here, my accelerometer has 14 bits

    xAccel = roll3Accel.x / 256.0;
    yAccel = roll3Accel.y / 256.0;
    zAccel = roll3Accel.z / 256.0;
    //os_printf("%d %d %d\n", (int)(100 * xAccel), (int)(100 * yAccel), (int)(100 * zAccel));

    switch (currentMethod)
    {
        case 0:
        case 1:
            // Damped Spring
            tf = ti + dt;
            // Do one step of ODE solver to dnx assigned to rhs_fun_pointer
            // Need to update accel values in parameters
            springParametersRed.xAccel = xAccel;
            springParametersRed.yAccel = yAccel;
            springParametersGreen.xAccel = xAccel;
            springParametersGreen.yAccel = yAccel;
            springParametersBlue.xAccel = xAccel;
            springParametersBlue.yAccel = yAccel;
            rk4_dn1((*rhs_fun_ptrr), ti, dt, xir, xfr, numberoffirstordereqn, (FLOATING*)&springParametersRed);
            rk4_dn1((*rhs_fun_ptrg), ti, dt, xig, xfg, numberoffirstordereqn, (FLOATING*)&springParametersGreen);
            rk4_dn1((*rhs_fun_ptrb), ti, dt, xib, xfb, numberoffirstordereqn, (FLOATING*)&springParametersBlue);
            break;
        case 2:
        case 3:
            // Damped Pendulum
            tf = ti + dt;
            // Do one step of ODE solver to dnx assigned to rhs_fun_pointer
            // Need to update accel values in parameters
            pendulumParametersRed.xAccel = xAccel;
            pendulumParametersRed.yAccel = yAccel;
            pendulumParametersGreen.xAccel = xAccel;
            pendulumParametersGreen.yAccel = yAccel;
            pendulumParametersBlue.xAccel = xAccel;
            pendulumParametersBlue.yAccel = yAccel;
            //Old but don't need pointer
            //rk4_dn1((*rhs_fun_ptrr), ti, dt, xir, xfr, numberoffirstordereqn, (FLOATING*)&pendulumParametersRed);
            //rk4_dn1((*rhs_fun_ptrg), ti, dt, xig, xfg, numberoffirstordereqn, (FLOATING*)&pendulumParametersGreen);
            //rk4_dn1((*rhs_fun_ptrb), ti, dt, xib, xfb, numberoffirstordereqn, (FLOATING*)&pendulumParametersBlue);
            //TODO thought should be &dandampedpendulum but either works but mode starts with spring wrong
            rk4_dn1(dnxdampedpendulum, ti, dt, xir, xfr, numberoffirstordereqn, (FLOATING*)&pendulumParametersRed);
            rk4_dn1(dnxdampedpendulum, ti, dt, xig, xfg, numberoffirstordereqn, (FLOATING*)&pendulumParametersGreen);
            rk4_dn1(dnxdampedpendulum, ti, dt, xib, xfb, numberoffirstordereqn, (FLOATING*)&pendulumParametersBlue);
            break;
        default:
            (void)0;
    }
    // prepare for the next step
    //*adjustment_fun_ptr(xi, xf);

    // possibly prevent hitting walls by stopping or bouncing
    // simple case 4 walls bounding the screen

    switch (currentMethod)
    {
        case 0:
        case 1:
        case 2:
        case 3:
            ti = tf;
            for (uint8_t i = 0; i < numberoffirstordereqn; i++)
            {
                xir[i] = xfr[i];
                xig[i] = xfg[i];
                xib[i] = xfb[i];
            }
            break;
        default:
            (void)0;
    }
    FLOATING down = atan2(-yAccel, -xAccel);
    switch (currentMethod)
    {
        case 0:
        case 1:
            scxcr = 64.0 - 16 * (xfr[0] + pi / 2);
            scycr = 10.0;
            scxcg = 64.0 - 16 * (xfg[0] + pi / 2);
            scycg = 32.0;
            scxcb = 64.0 - 16 * (xfb[0] + pi / 2);
            scycb = 54.0;
            totalenergyr = SPRINGCONSTR * (xfr[0] + pi / 2) * (xfr[0] + pi / 2) + xfr[1] * xfr[1];
            totalenergyg = SPRINGCONSTG * (xfg[0] + pi / 2) * (xfg[0] + pi / 2) + xfg[1] * xfg[1];
            totalenergyb = SPRINGCONSTB * (xfb[0] + pi / 2) * (xfb[0] + pi / 2) + xfb[1] * xfb[1];
            break;
        case 2:
        case 3:
            scxcr = 64.0 - 28.0 * cos(xfr[0]);
            scycr = 32.0 - 28.0 * sin(xfr[0]);
            scxcg = 64.0 - 28.0 * cos(xfg[0]);
            scycg = 32.0 - 28.0 * sin(xfg[0]);
            scxcb = 64.0 - 28.0 * cos(xfb[0]);
            scycb = 32.0 - 28.0 * sin(xfb[0]);
            // Using this for both spring and pendulum
            totalenergyr = gravity * LEN_PENDULUMR * (1 - cos(xfr[0] - down))  + 0.5 * LEN_PENDULUMR * LEN_PENDULUMR * xfr[1] *
                           xfr[1];
            totalenergyg = gravity * LEN_PENDULUMG * (1 - cos(xfg[0] - down))  + 0.5 * LEN_PENDULUMG * LEN_PENDULUMG * xfg[1] *
                           xfg[1];
            totalenergyb = gravity * LEN_PENDULUMB * (1 - cos(xfb[0] - down))  + 0.5 * LEN_PENDULUMB * LEN_PENDULUMB * xfb[1] *
                           xfb[1];

            // totalenergyr = SPRINGCONSTR * (xfr[0] + pi / 2) * (xfr[0] + pi / 2) + xfr[1] * xfr[1];
            // totalenergyg = SPRINGCONSTG * (xfg[0] + pi / 2) * (xfg[0] + pi / 2) + xfg[1] * xfg[1];
            // totalenergyb = SPRINGCONSTB * (xfb[0] + pi / 2) * (xfb[0] + pi / 2) + xfb[1] * xfb[1];
            //os_printf("Total Energy at frame %d: R %d G %d B %d\n", countframes++, (int)totalenergyr, (int)totalenergyg, (int)totalenergyb);
            //os_printf("%d, %d, %d, %d, %d, %d\n", (int)totalenergyr, (int)totalenergyg, (int)totalenergyb, (int)(xfr[1] * xfr[1]),
            //          (int)(xfg[1] * xfg[1]), (int)(xfb[1] * xfb[1]));
            break;
        default:
            (void)0;
    }



    //len = 1;

    // Convert Output of ODE solution to coordinates on OLED for the moving ball
    // OLED xcoord from 0 (left) to 127, ycoord from 0(top) to 63

    /*
        // Using center of screen as orgin, position ball  proportional to x,y component of roll3Accel
        //plotCircle(64 + (roll3Accel.x>>2), 32 - (roll3Accel.y>>3), BTN_RAD, WHITE);

        // Using center of screen as orgin, position ball on circle of radius 32 with direction x,y component of roll3Accel
        //int16_t xc = roll3Accel.x;
        //int16_t yc = roll3Accel.y;


        len = sqrt(xc*xc + yc*yc);
        //uint16_t len = sqrt(xc*xc + yc*yc);
        //uint16_t len = norm(xc, yc);
    */

    if (true)
    {
        // scale normalized vector to length 28 to keep ball within bounds of screen
        //scxc = ((xc*28) / len); // for rolling ball
        //scyc = ((yc*28) / len); // for rolling ball
        //scxc = 28.0 * xc / len; // for rolling ball using Floating point
        //scyc = 28.0 * yc / len; // for rolling ball using Floating point
        //os_printf("100th %d, 100x %d, 100y %d\n", (int)(100*xf[0]), (int)(100*scxc), (int)(100*scyc));
        //os_printf("xc %d, yc %d, len %d scxc %d scyc %d\n", xc, yc, len, scxc, scyc);

        plotText(.95 * scxcr, .95 * scycr, "R", IBM_VGA_8, WHITE);
        plotText(.9 * scxcg, .9 * scycg, "G", IBM_VGA_8, WHITE);
        plotText(.85 * scxcb, .85 * scycb, "B", IBM_VGA_8, WHITE);

        //plotCircle(scxcr, scycr, 5, WHITE);
        //plotCircle(scxcr, scycr, 3, WHITE);
        //plotCircle(scxcr, scycr, 1, WHITE);
        //plotCircle(scxcg, scycg, 5, WHITE);
        //plotCircle(scxcg, scycg, 3, WHITE);
        //plotCircle(scxcg, scycg, 1, WHITE);
        //plotCircle(scxcb, scycb, 5, WHITE);
        //plotCircle(scxcb, scycb, 3, WHITE);
        //plotCircle(scxcb, scycb, 1, WHITE);
    }

    // Declare some LEDs, all off
    led_t leds[NUM_LIN_LEDS] = {{0}};

    /*  Python
        for led in self.leds:
            led.alpha = (1 - abs(self.ball.position - led.position)/(30+2*self.size.h/2.5))**3
            if self.framecount % 1 == 0:
                led.color = colorsys.hsv_to_rgb(self.ball.meanspeed,1,1)
    */

#define USE_6_LEDS
    switch (currentMethod)
    {
        case 0:
        case 2:
            //NOTE set to  == if want to test on my mockup with 16 leds
#ifndef USE_6_LEDS
            for (uint8_t indLed = 0; indLed < NUM_LIN_LEDS ; indLed++)
            {
                leds[indLed].r = totalenergyr;
                leds[indLed].g = totalenergyg;
                leds[indLed].b = totalenergyb;
            }
#else
            for (uint8_t indLed = 0; indLed < 6 ; indLed++)
            {
                leds[ledOrderIndroll3[indLed]].r = totalenergyr;
                leds[ledOrderIndroll3[indLed]].g = totalenergyg;
                leds[ledOrderIndroll3[indLed]].b = totalenergyb;
            }
#endif
            break;
        case 1:
        case 3:

#define GAP 1
#ifdef USE_6_LEDS
#define USE_NUM_LEDS 6
#else
#define USE_NUM_LEDS NUM_LIN_LEDS
#endif
            for (uint8_t indLed = 0; indLed < USE_NUM_LEDS / GAP; indLed++)
            {
                int16_t ledy = Ssinonlytable[((indLed << 8) * GAP / USE_NUM_LEDS + 0x80) % 256] * 28 / 1500; // from -1500 to 1500
                int16_t ledx = Ssinonlytable[((indLed << 8) * GAP / USE_NUM_LEDS + 0xC0) % 256] * 28 / 1500;
                lenr = sqrt((scxcr - 64 - ledx) * (scxcr - 64 - ledx) + (-scycr + 32 - ledy) * (-scycr + 32 - ledy));
                leng = sqrt((scxcg - 64 - ledx) * (scxcg - 64 - ledx) + (-scycg + 32 - ledy) * (-scycg + 32 - ledy));
                lenb = sqrt((scxcb - 64 - ledx) * (scxcb - 64 - ledx) + (-scycb + 32 - ledy) * (-scycb + 32 - ledy));
                //len = norm(scxc - ledx, scyc - ledy);
                uint8_t glowr = 255 * pow(1.0 - (lenr / 56.0), 3);
                uint8_t glowg = 255 * pow(1.0 - (leng / 56.0), 3);
                uint8_t glowb = 255 * pow(1.0 - (lenb / 56.0), 3);
                //os_printf("%d %d %d %d %d %d %d \n",indLed, ledx, ledy, scxc, scyc, len, 255 - len * 4);
                //leds[GAP*indLed].r = 255 - len * 4;
#ifdef USE_6_LEDS
                leds[ledOrderIndroll3[GAP * indLed]].r = glowr;
                leds[ledOrderIndroll3[GAP * indLed]].g = glowg;
                leds[ledOrderIndroll3[GAP * indLed]].b = glowb;
#else
                leds[GAP * indLed].r = glowr;
                leds[GAP * indLed].g = glowg;
                leds[GAP * indLed].b = glowb;
#endif
            }
            break;
        default:
            (void)0;
    }

    setRoll3Leds(leds, sizeof(leds));

}


/**
 *
 * @param state  A bitmask of all button states, unused
 * @param button The button which triggered this event
 * @param down   true if the button was pressed, false if it was released
 */
void ICACHE_FLASH_ATTR roll3ButtonCallback( uint8_t state,
        int button __attribute__((unused)), int down __attribute__((unused)))
{
    roll3ButtonState = state;
    roll3_updateDisplay();

    if(down)
    {
        if(2 == button)
        {
            // Cycle movement methods
            currentMethod = (currentMethod + 1) % numMethods;
            initializeConditionsForODERoll3(currentMethod);
            os_printf("currentMethod = %d\n", currentMethod);
        }
        if(1 == button)
        {
            // Cycle brightnesses
            roll3BrightnessIdx = (roll3BrightnessIdx + 1) %
                                 (sizeof(roll3Brightnesses) / sizeof(roll3Brightnesses[0]));
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
void ICACHE_FLASH_ATTR roll3AccelerometerHandler(accel_t* accel)
{
    roll3Accel.x = accel->y;
    roll3Accel.y = accel->x;
    roll3Accel.z = accel->z;
    roll3_updateDisplay();
}

/**
 * Intermediate function which adjusts brightness and sets the LEDs
 *
 * @param ledData    The LEDs to be scaled, then set
 * @param ledDataLen The length of the LEDs to set
 */
void ICACHE_FLASH_ATTR setRoll3Leds(led_t* ledData, uint8_t ledDataLen)
{
    uint8_t i;
    for(i = 0; i < ledDataLen / sizeof(led_t); i++)
    {
        ledData[i].r = ledData[i].r / roll3Brightnesses[roll3BrightnessIdx];
        ledData[i].g = ledData[i].g / roll3Brightnesses[roll3BrightnessIdx];
        ledData[i].b = ledData[i].b / roll3Brightnesses[roll3BrightnessIdx];
    }
    setLeds(ledData, ledDataLen);
}
