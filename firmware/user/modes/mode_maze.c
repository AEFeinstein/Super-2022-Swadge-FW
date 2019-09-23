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

//#define MAZE_DEBUG_PRINT
#ifdef MAZE_DEBUG_PRINT
#include <stdlib.h>
    #define maze_printf(...) os_printf(__VA_ARGS__)
#else
    #define maze_printf(...)
#endif

#ifndef max
    #define max(a,b) ((a) > (b) ? (a) : (b))
#endif
#ifndef min
    #define min(a,b) ((a) < (b) ? (a) : (b))
#endif
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
void dnx(float, float [], float [], int );
int16_t ICACHE_FLASH_ATTR get_maze(uint8_t width, uint8_t height, uint8_t xleft[], uint8_t xright[], uint8_t ybot[], uint8_t ytop[]);
uint8_t ICACHE_FLASH_ATTR intervalsmeet(float a,float c,float b,float d,float e,float f, float param[]);
uint8_t ICACHE_FLASH_ATTR  gonethru(float b_prev[], float b_now[], float p_1[], float p_2[], float rball, float b_nowadjusted[], float param[]);
int16_t ICACHE_FLASH_ATTR  incrementifnewvert(int16_t nwi, int16_t startind, int16_t endind);
int16_t ICACHE_FLASH_ATTR  incrementifnewhoriz(int16_t nwi, int16_t startind, int16_t endind);

/*============================================================================
 * Static Const Variables
 *==========================================================================*/

static const uint8_t mazeBrightnesses[] =
{
    0x01,
    0x08,
    0x40,
    0x80,
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
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = mazeAccelerometerHandler
};

accel_t mazeAccel = {0};
uint8_t mazeButtonState = 0;
uint8_t mazeBrightnessIdx = 2;
int maze_ledCount = 0;

float xAccel;
float yAccel;
float zAccel;
float len;
float scxc;
float scyc;
float scxcprev = 2.0;
float scycprev = 2.0;
float scxcexit;
float scycexit;
float rballused = 5.0;
uint16_t totalcyclestilldone;
uint16_t totalhitstilldone;
bool gameover;

uint8_t width = 7;
uint8_t height = 3; //Maze dimensions must be odd>1 probably for OLED use 31 15
uint8_t mazescalex = 1;
uint8_t mazescaley = 1;
int16_t numwalls;
int16_t numwallstodraw;
uint8_t xleft[MAXNUMWALLS];
uint8_t xright[MAXNUMWALLS];
uint8_t ytop[MAXNUMWALLS];
uint8_t ybot[MAXNUMWALLS];
uint8_t flashcount = 0;
uint8_t flashmax = 4;

float wxleft[MAXNUMWALLS];
float wxright[MAXNUMWALLS];
float wytop[MAXNUMWALLS];
float wybot[MAXNUMWALLS];

float swxleft[MAXNUMWALLS];
float swxright[MAXNUMWALLS];
float swytop[MAXNUMWALLS];
float swybot[MAXNUMWALLS];
/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * Initializer for maze
 */
void ICACHE_FLASH_ATTR mazeEnterMode(void)
{
    int16_t i;
    int16_t startvert = 0;
    totalcyclestilldone = 0;
    totalhitstilldone = 0;
    gameover = false;
    // initial position upper left corner
    scxcprev = rballused;
    scycprev = rballused;
    // set these accelerometer readings consistant with starting at scxcprev and scycprev
    xAccel = rballused - 64;
    yAccel = -2 * (rballused - 30);

    enableDebounce(true);
    system_print_meminfo();
    os_printf("Free Heap %d\n", system_get_free_heap_size());
    numwalls = get_maze(width, height, xleft, xright, ybot, ytop);
    // xleft, xright, ybot, ytop are lists of boundary intervals making maze
    numwallstodraw = numwalls;
    mazescalex = 127/width; //63,31,15,7
    mazescaley = 63/height; //31,15,7,3
    // for width 63 height 31 wx vary from 0, 4, 8, ..., 124  and wy vary from 0, 4, ... 60   radius 1 ball
    //           31        15         from 0, 8, 16, ..., 120                  0, 8, ..., 56  radius 2 ball
    //           15         7 wx      from 0, 16, ... , 112       wy           0, 16, ..., 48 radius 3 ball
    //            7         3         from 0, 32, ..., 96                      0, 32, ...  radius 4 ball
    maze_printf("width:%d, height:%d mscx:%d mscy:%d\n", width, height, mazescalex, mazescaley);
    if (numwalls > MAXNUMWALLS) os_printf("numwalls = %d exceeds MAXNUMWALLS = %d", numwalls, MAXNUMWALLS);

    // exit is bottom right corner

    scxcexit = mazescalex * (width - 1) - rballused;
    scycexit = mazescaley * (height - 1) - rballused;
    os_printf("exit (%d, %d)\n", (int)scxcexit, (int)scycexit);

    // produce scaled walls
    for (i = 0; i < numwalls; i++)
    {
        swxleft[i]  = mazescalex * xleft[i];
	    swybot[i]   = mazescaley * ybot[i];
        swxright[i] = mazescalex * xright[i];
	    swytop[i]   = mazescaley * ytop[i];
        maze_printf("i %d (%d, %d) to (%d, %d)\n", i, mazescalex*xleft[i], mazescaley*ybot[i], mazescalex*xright[i], mazescaley*ytop[i]);
    }

    // extend the scaled walls (could possibly use same storage for swxleft and wxleft etc, but need to be careful
    //   that use original scaled values for making wxleft etc)
    // extend walls by 0.99*rball and compute possible extra stopper walls
    // ONLY for horizontal and vertical walls. Could do for arbitrary but
    // would first need to compute perpendicular vectors and use them. 
    // extending by rball I think guarantees no passing thru corners, but also
    // causes sticking above T junctions in maze.
    // NOTE using 0.99*rball prevents sticking but has very small probability
    // to telport at corners extend walls
    for (i = 0; i < numwalls; i++)
    {
        if (swybot[i] == swytop[i]) // horizontal wall
        {
            wybot[i] = swybot[i];
            wytop[i] = swytop[i];
            wxleft[i] = swxleft[i] - 0.99*rballused;
            wxright[i] = swxright[i] + 0.99*rballused;
        } else {
            if ((swybot[i] < swytop[i]) && (startvert==0))
            {
                startvert = i;
            }
            wxleft[i] = swxleft[i];
            wxright[i] = swxright[i];
            wybot[i] = swybot[i] - 0.99*rballused;
            wytop[i] = swytop[i] + 0.99*rballused;
        }
    }
    int16_t nwi = numwalls; //new wall index starts here
    // find and keep only stopper walls that are not contained in any of the extended walls
    maze_printf("startvert = %d\n", startvert);

    for (i = 0; i < numwalls; i++)
    {
        if (swybot[i] == swytop[i]) // horizontal wall
        {
            // possible extra vertical walls crossing either end
            wxleft[nwi] = swxleft[i];
            wybot[nwi] = swybot[i] - 0.99*rballused;
            wxright[nwi] = swxleft[i];
            wytop[nwi] = swybot[i] + 0.99*rballused;
            nwi = incrementifnewvert(nwi, startvert, numwalls); //, wxleft, wybot, wxright, wytop);
            wxleft[nwi] = swxright[i];
            wybot[nwi] = swybot[i] - 0.99*rballused;
            wxright[nwi] =swxright[i];
            wytop[nwi] = swybot[i] + 0.99*rballused;
            nwi = incrementifnewvert(nwi, startvert, numwalls); //, wxleft, wybot, wxright, wytop);
        } else {
            // possible extra horizontal walls crossing either end
            wxleft[nwi] = swxleft[i]- 0.99*rballused;
            wybot[nwi] = swybot[i];
            wxright[nwi] = swxleft[i] + 0.99*rballused;
            wytop[nwi] = swybot[i];
            nwi = incrementifnewhoriz(nwi, 0, startvert); //, wxleft, wybot, wxright, wytop);
            wxleft[nwi] = swxleft[i]- 0.99*rballused;
            wybot[nwi] = swytop[i];
            wxright[nwi] = swxleft[i] + 0.99*rballused;
            wytop[nwi] = swytop[i];
            nwi = incrementifnewhoriz(nwi, 0, startvert); //, wxleft, wybot, wxright, wytop);
        }
    }
    if (nwi > MAXNUMWALLS) os_printf("nwi = %d exceeds MAXNUMWALLS = %d", nwi, MAXNUMWALLS);
    maze_printf("orginal numwalls = %d, with stoppers have %d\n", numwalls, nwi);
    // update numwalls
    numwalls = nwi;
}

int16_t ICACHE_FLASH_ATTR  incrementifnewvert(int16_t nwi, int16_t startind, int16_t endind)
{
// nwi is new vertical wall index
// increment nwi only if no extended vertical walls contain it.
    for (int16_t i = startind; i < endind; i++)
    {
        if ((wxright[nwi] == wxright[i]) && (wybot[i] <= wybot[nwi]) && (wytop[i] >= wytop[nwi]))
        {
            // found containing extended vertical wall
            return nwi;
        }
    }
    return nwi + 1;
}

int16_t ICACHE_FLASH_ATTR  incrementifnewhoriz(int16_t nwi, int16_t startind, int16_t endind)
{
// nwi is  new horizontal wall index
// increment nwi only if no extended horizontal walls contain it.
    for (int16_t i = startind; i < endind; i++)
    {
        if ((wytop[nwi] == wytop[i]) && (wxleft[i] <= wxleft[nwi]) && (wxright[i] >= wxright[nwi]))
        {
            // found containing extended horizontal wall
            return nwi;
        }
    }
    return nwi + 1;
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

uint8_t ICACHE_FLASH_ATTR intervalsmeet(float a,float c,float b,float d,float e,float f, float param[])
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
    // param = [t, s]
    // returns True

    float det = -a*d + b*c;
    if (det == 0) return false;
    float t = (-e*d + f*b) / det; // t is param of interval
    float s = (a*f - c*e) / det; //s is param of interval from b_prev to b_now
    param[0] = t;
    param[1] = s;
    //if ((t < -0.05) || (t > 1.05)) return false;
    //if ((s < -0.05) || (s > 1.05)) return false;
    if ((t < 0) || (t > 1)) return false;
    if ((s < 0) || (s > 1)) return false;
    //maze_printf("t = %d, s = %d\n", (int8_t) (100*t), (int8_t) (100*s));
    return true;
}

uint8_t ICACHE_FLASH_ATTR  gonethru(float b_prev[], float b_now[], float p_1[], float p_2[], float rball, float b_nowadjusted[], float param[])
{
    // given two points p_1, p_2 in the (x,y) plane specifying a line interval from p_1 to p_2
    // a moving object (ball of radius rball, or point if rball is None)
    // whos center was at b_prev and is currently at b_now
    // returns true if the balls leading (in direction of travel) boundary crossed the line interval
    // this can also be useful if want the line interval to be a barrier by
    //    reverting to b_prev
    // b_nowadjusted is mutable list which is the point moved back to inside boundary
    float pperp[2];
    // param vector if an intersection {t, s} where t parameterizes from p_1 to p_2 and s b_prev to b_now 
    //float param[2];
    uint8_t didgothru;

    b_nowadjusted[0] = b_now[0];
    b_nowadjusted[1] = b_now[1];
    //TODO could compute ppperp in mazeEnterMode to save time but take more space
    pperp[0] = p_2[1]-p_1[1];
    pperp[1] = p_1[0]-p_2[0];

    float pperplen = sqrt(pperp[0] * pperp[0] + pperp[1] * pperp[1]);

    if (pperplen == 0.0) return false;
    //if (pperplen == 0.0) {os_printf("P"); return false;} // should never happen here as using walls which all have pos length

    pperp[0] = pperp[0] / pperplen; // make unit vector
    pperp[1] = pperp[1] / pperplen; // make unit vector


    float testdir = pperp[0] * (b_now[0] - b_prev[0]) + pperp[1] * (b_now[1] - b_prev[1]);
    if (testdir == 0.0) return false;
    //if (testdir == 0.0) {os_printf("T"); return false;} // happens when touching boundary but not for largest maze

    //b_nowadjusted[0] = b_now[0] - testdir * pperp[0];
    //b_nowadjusted[1] = b_now[1] - testdir * pperp[1];

    //os_printf("%d ", (int)(1000*testdir));

    if (testdir > 0) // > for leading edge , < for trailing edge
        didgothru =  intervalsmeet(p_2[0]-p_1[0], p_2[1]-p_1[1], b_now[0]-b_prev[0], b_now[1]-b_prev[1], b_prev[0] + rball*pperp[0] - p_1[0], b_prev[1] + rball*pperp[1] - p_1[1], param);
    else
        didgothru = intervalsmeet(p_2[0]-p_1[0], p_2[1]-p_1[1], b_now[0]-b_prev[0], b_now[1]-b_prev[1], b_prev[0] - rball*pperp[0] - p_1[0], b_prev[1] - rball*pperp[1] - p_1[1], param);

    
    if (didgothru)
    {
         // Adjust back to previous point ie ignore movement - TOO choppy
         //b_nowadjusted[0] = b_prev[0];
         //b_nowadjusted[1] = b_prev[1];
         // Could adjust to point of contact with interval but still choppy

         // Adjust to roll along interval is would have gone thru
         b_nowadjusted[0] = b_now[0] + (1.0 - param[1]) * (- testdir * pperp[0]);
         b_nowadjusted[1] = b_now[1] + (1.0 - param[1]) * (- testdir * pperp[1]);
   }
    return didgothru;
}

led_t leds[NUM_LIN_LEDS] = {{0}};

void ICACHE_FLASH_ATTR maze_updateDisplay(void)
{
    float param[2];
    bool gonethruany;
    if (gameover)
    {
       // Show score
       char scoreStr[32] = {0};
       ets_snprintf(scoreStr, sizeof(scoreStr), "Time: %d", totalcyclestilldone);
       plotText(20, OLED_HEIGHT - (5 * (FONT_HEIGHT_IBMVGA8 + 1)), scoreStr, IBM_VGA_8, WHITE);
       ets_snprintf(scoreStr, sizeof(scoreStr), "Wall: %d", totalhitstilldone);
       plotText(20, OLED_HEIGHT - (4 * (FONT_HEIGHT_IBMVGA8 + 1)), scoreStr, IBM_VGA_8, WHITE);
       return;
    }

    // Clear the display
    clearDisplay();

    for (int16_t i = 0; i < numwallstodraw; i++)
    {
        plotLine(mazescalex*xleft[i], mazescaley*ybot[i], mazescalex*xright[i], mazescaley*ytop[i], WHITE);
    }

    //NOTE bug in Expressif OS can't print floating point! Must cast as int
    //debug print for thrown ball
    //maze_printf("100t = %d, x = %d, y = %d, vx = %d, vy = %d\n", (int)(100*ti), (int)xi[0], (int)xi[1], (int)xi[2], (int)xi[3]);

    //Save accelerometer reading in global storage
    //TODO can get values bigger than 1. here, my accelerometer has 14 bits
    //  but these are usually between +- 255

    // Smooth accelerometer readings
    xAccel = 0.9*xAccel + 0.1*mazeAccel.x;
    yAccel = 0.9*yAccel + 0.1*mazeAccel.y;
    zAccel = 0.9*zAccel + 0.1*mazeAccel.z;

#define GETVELOCITY
#ifdef GETVELOCITY
    // Accelerometer determines velocity and does one euler step with dt = .1
    scxc = scxcprev + 0.1 * mazeAccel.x;
    scyc = scycprev + 0.1 * mazeAccel.y;
    // Smoothed accelerometer determining velocity puts inertia makes bit harder to control
    //scxc = scxcprev + 0.1 * xAccel;
    //scyc = scycprev + 0.1 * yAccel;
#else
    // Smoothed accelerometer determines position on screen
    // want -63 to 63 to go approx from 0 to 124 for scxc and 60 to 0 for scyc
    scxc = xAccel + 62; //xAccel/63 * 62 + 62
    scyc = yAccel/2 + 30; //yAccel/63  + 30
#endif

    // force values within screen range
    // boundary walls should handle this
    // but don't seem to always do so this hack
    // TODO will get different boundarys for various size mazes
    scxc = min(scxc, 127.);
    scxc = max(scxc, 0.0);
    scyc = min(scyc, 63.);
    scyc = max(scyc, 0.0);


    /**************************************
    //Keep ball within maze boundaries.
    **************************************/

    //maze_printf("Entry for (%d, %d) to (%d, %d)\n", (int)(100*scxcprev), (int)(100*scycprev), (int)(100*scxc), (int)(100*scyc));

    // Have at most two passes to find first hit of wall and adjust
    int16_t iused = -1;
    int16_t imin = -1;

    for (uint8_t k = 0; k < 2; k++)
    {
        float b_prev[2] = {scxcprev, scycprev};
        float b_now[2] = {scxc, scyc};
        float b_nowadjusteduse[2] = {scxc, scyc};
        float closestintersection[] = {0, 0};
        gonethruany = false;
        float smin = 2;
        for (int16_t i = 0; i < numwalls; i++)
        {
            if (i == iused)
            {
                continue;
            }

            float p_1[2] = {wxleft[i], wybot[i]};
            float p_2[2] = {wxright[i], wytop[i]};
            float b_nowadjusted[2];
            if ( gonethru(b_prev, b_now, p_1, p_2, rballused, b_nowadjusted, param) )
            {
                gonethruany = true;

        //DEBUG
                maze_printf("100x ******* i = %d \np(%d, %d), n(%d, %d)\nw1(%d, %d), w2(%d, %d)\nt=%d, s=%d, a(%d, %d)\n",i,  (int)(100*b_prev[0]), (int)(100*b_prev[1]), (int)(100*b_now[0]), (int)(100*b_now[1]), (int)(100*p_1[0]), (int)(100*p_1[1]), (int)(100*p_2[0]), (int)(100*p_2[1]), (int)(100*param[0]), (int)(100*param[1]), (int)(100*b_nowadjusted[0]), (int)(100*b_nowadjusted[1]));
        #ifdef MAZE_DEBUG_PRINT
                int xmeet1, xmeet2, ymeet1, ymeet2;
                xmeet1 = 100*(b_prev[0] + param[1] * (b_now[0] - b_prev[0]));
                xmeet2 = 100*(p_1[0] + param[0] * (p_2[0] - p_1[0]));
                ymeet2 = 100*(p_1[1] + param[0] * (p_2[1] - p_1[1]));
                maze_printf("on motion vector meet (%d, %d) =? (%d, %d) on boundary\n\n", xmeet1, ymeet1, xmeet2, ymeet2);
        #endif
                if (param[1] < smin)
                {
                    smin = param[1];
                    b_nowadjusteduse[0] = b_nowadjusted[0];
                    b_nowadjusteduse[1] = b_nowadjusted[1];
                    closestintersection[0] = b_prev[0] + param[1] * (b_now[0] - b_prev[0]);
                    closestintersection[1] = b_prev[1] + param[1] * (b_now[1] - b_prev[1]);
                    imin = i;
                }
            }
        } // end loop in i going thru walls checking to find closest intersection
        iused = imin;
        if (gonethruany)
        {
            //if (k==1)
            if (true)
            {
                scxc = b_nowadjusteduse[0];
                scyc = b_nowadjusteduse[1];
                scxcprev = closestintersection[0];
                scycprev = closestintersection[1];
            } else {
                scxc = closestintersection[0];
                scyc = closestintersection[1];
            }
        } else {
            break;
        }
        maze_printf("pass %d end at (%d, %d)\n", k, (int)(100*scxc), (int)(100*scyc));
        maze_printf("    closest intersection at wall %d with s = %d at (%d, %d)\n", iused, (int)(100*smin), (int)(100*closestintersection[0]), (int)(100*closestintersection[1]));
    
    } // end two try loop

    // balls new coordinates scxc, scyc were adjusted if gonethruany is true
    // can keep a score of totaltime and totaltimehitting
    // can flash LED when touch

    totalcyclestilldone++;
    if (gonethruany) totalhitstilldone++;

    // Update previous location for next cycle

    scxcprev = scxc;
    scycprev = scyc;

    // Test if at exit (bottom right corner) thus finished

    if ((scxc == scxcexit) && (scyc == scycexit))
    {
        leds[0].r = 255;
        // Compute score
        os_printf("Time to complete maze %d, time on walls %d\n",totalcyclestilldone, totalhitstilldone);
        gameover = true;
        //TODO How to stop and save score and start new game
    }

    // Draw the ball

    switch ((int)rballused - 1)
    {
        case 4:
            plotCircle(scxc + 0.5, scyc + 0.5, 4, WHITE);
        case 3:
            plotCircle(scxc + 0.5, scyc + 0.5, 3, WHITE);
        case 2:
            plotCircle(scxc + 0.5, scyc + 0.5, 2, WHITE);
        case 1:
            if (flashcount < flashmax/2) plotCircle(scxc + 0.5, scyc + 0.5, 1, WHITE);
            flashcount++;
            if (flashcount > flashmax) flashcount = 0;
    default:
        plotCircle(scxc + 0.5, scyc + 0.5, 0, WHITE);
    }

    // Light some LEDS


    /*  Old Python Code for setting alpha and color depending on balls position and speed
        for led in self.leds:
            led.alpha = (1 - abs(self.ball.position - led.position)/(30+2*self.size.h/2.5))**3
            if self.framecount % 1 == 0:
                led.color = colorsys.hsv_to_rgb(self.ball.meanspeed,1,1)
    */

    /*
    #define GAP 1

        for (uint8_t indLed=0; indLed<NUM_LIN_LEDS/GAP; indLed++)
        {
            // imagine led is positioned around OLED, use distance to ball as surogate for its brightness
            int16_t ledy = Ssinonlytable[((indLed<<8)*GAP/NUM_LIN_LEDS + 0x80) % 256]*28/1500; // from -1500 to 1500
            int16_t ledx = Ssinonlytable[((indLed<<8)*GAP/NUM_LIN_LEDS + 0xC0) % 256]*28/1500;
            len = sqrt((scxc - ledx)*(scxc - ledx) + (scyc - ledy)*(scyc - ledy));
            //len = norm(scxc - ledx, scyc - ledy);
            uint8_t glow = 255 * pow(1.0 - (len / 56.0), 3);
            //maze_printf("%d %d %d %d %d %d %d \n",indLed, ledx, ledy, scxc, scyc, len, 255 - len * 4);
            //leds[GAP*indLed].r = 255 - len * 4;
            leds[GAP*indLed].r = glow;
        }
    */
    setmazeLeds(leds, sizeof(leds));

} // end of maze_updateDisplay


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
                height = 7;
                rballused += 3.0;
            } else {
                height = width;
                width = 2*height + 1;
                rballused--;
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
    mazeAccel.x = accel->y;
    mazeAccel.y = accel->x;
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
