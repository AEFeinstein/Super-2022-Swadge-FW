/*
 * mode_flight.c
 *
 *  Created on: Sept 15, 2020
 *      Author: <>< CNLohr
 */
#include "user_config.h"

/*============================================================================
 * Includes
 *==========================================================================*/

#include <osapi.h>
#include <mem.h>
#include <stdint.h>

#include "user_main.h"
#include "embeddednf.h"
#include "oled.h"
#include "bresenham.h"
#include "assets.h"
#include "buttons.h"
#include "menu2d.h"
#include "linked_list.h"
#include "font.h"
#include "gpio.h"
#include "esp_niceness.h"

#include "embeddednf.h"
#include "embeddedout.h"

/*============================================================================
 * Defines, Structs, Enums
 *==========================================================================*/

#define FLIGHT_UPDATE_MS 33

typedef enum
{
    FL_EXPLORE,
    FL_TIMED
} flGameType;


typedef enum
{
    FLIGHT_MENU,
    FLIGHT_GAME
} flightModeScreen;

typedef struct
{
    flightModeScreen mode;

    timer_t updateTimer;
    int frames;
    uint8_t buttonState;
    flGameType type;

    int16_t planeloc[3];
    int16_t hpr[3];

    menu_t* menu;
} flight_t;

/*============================================================================
 * Prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR flightEnterMode(void);
void ICACHE_FLASH_ATTR flightExitMode(void);
void ICACHE_FLASH_ATTR flightButtonCallback(uint8_t state __attribute__((unused)),
        int button, int down);

static void ICACHE_FLASH_ATTR flightUpdate(void* arg __attribute__((unused)));
static void ICACHE_FLASH_ATTR flightMenuCb(const char* menuItem);
static void ICACHE_FLASH_ATTR flightStartGame(flGameType type);

static void ICACHE_FLASH_ATTR flightGameUpdate( flight_t * flight );

static int16_t pittsburg[];

/*============================================================================
 * Variables
 *==========================================================================*/

swadgeMode flightMode =
{
    .modeName = "flight",
    .fnEnterMode = flightEnterMode,
    .fnExitMode = flightExitMode,
    .fnButtonCallback = flightButtonCallback,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = NULL,
    .fnAudioCallback = NULL,
    .menuImg = "flight-menu.gif"
};

flight_t* flight;

static const char fl_title[]  = "Flightsim";
static const char fl_flight_exlore[] = "EXPLORE";
static const char fl_flight_timed[] = "TIMED";
static const char fl_quit[]   = "QUIT";

/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * Initializer for flight
 */
void ICACHE_FLASH_ATTR flightEnterMode(void)
{
    // Alloc and clear everything
    flight = os_malloc(sizeof(flight_t));
    ets_memset(flight, 0, sizeof(flight_t));

    flight->mode = FLIGHT_MENU;

    flight->menu = initMenu(fl_title, flightMenuCb);
    addRowToMenu(flight->menu);
    addItemToRow(flight->menu, fl_flight_exlore);
    addItemToRow(flight->menu, fl_flight_timed);
    addRowToMenu(flight->menu);
    addItemToRow(flight->menu, fl_quit);
    drawMenu(flight->menu);

    timerDisarm(&(flight->updateTimer));
    timerSetFn(&(flight->updateTimer), flightUpdate, NULL);
    timerArm(&(flight->updateTimer), FLIGHT_UPDATE_MS, true);
    enableDebounce(false);
}

/**
 * Called when flight is exited
 */
void ICACHE_FLASH_ATTR flightExitMode(void)
{
    timerDisarm(&(flight->updateTimer));
    timerFlush();
    deinitMenu(flight->menu);
    os_free(flight);
}

/**
 * TODO
 *
 * @param menuItem
 */
static void ICACHE_FLASH_ATTR flightMenuCb(const char* menuItem)
{
    if(fl_flight_exlore == menuItem)
    {
        flightStartGame(FL_EXPLORE);
    }
    else if (fl_flight_timed == menuItem)
    {
        flightStartGame(FL_TIMED);
    }
    else if (fl_quit == menuItem)
    {
        switchToSwadgeMode(0);
    }
}

/**
 * TODO
 *
 * @param type
 * @param type
 * @param difficulty
 */
static void ICACHE_FLASH_ATTR flightStartGame(flGameType type)
{
    flight->mode = FLIGHT_GAME;
    flight->type = type;
    flight->frames = 0;
}

/**
 * @brief called on a timer, updates the game state
 *
 * @param arg
 */
static void ICACHE_FLASH_ATTR flightUpdate(void* arg __attribute__((unused)))
{
    switch(flight->mode)
    {
        default:
        case FLIGHT_MENU:
        {
            drawMenu(flight->menu);
            break;
        }
        case FLIGHT_GAME:
        {
            // Increment the frame count
            flight->frames++;

            flightGameUpdate( flight );
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//From https://github.com/cnlohr/channel3/blob/master/user/3d.c

const static uint8_t sintable[128] = { 0, 6, 12, 18, 25, 31, 37, 43, 49, 55, 62, 68, 74, 80, 86, 91, 97, 103, 109, 114, 120, 125, 131, 136, 141, 147, 152, 157, 162, 166, 171, 176, 180, 185, 189, 193, 197, 201, 205, 208, 212, 215, 219, 222, 225, 228, 230, 233, 236, 238, 240, 242, 244, 246, 247, 249, 250, 251, 252, 253, 254, 254, 255, 255, 255, 255, 255, 254, 254, 253, 252, 251, 250, 249, 247, 246, 244, 242, 240, 238, 236, 233, 230, 228, 225, 222, 219, 215, 212, 208, 205, 201, 197, 193, 189, 185, 180, 176, 171, 166, 162, 157, 152, 147, 141, 136, 131, 125, 120, 114, 109, 103, 97, 91, 86, 80, 74, 68, 62, 55, 49, 43, 37, 31, 25, 18, 12, 6, };

int16_t ModelviewMatrix[16];
int16_t ProjectionMatrix[16];

static int16_t ICACHE_FLASH_ATTR ICACHE_FLASH_ATTR tdSIN( uint8_t iv )
{
    if( iv > 127 )
    {
        return -sintable[iv-128];
    }
    else
    {
        return sintable[iv];
    }
}

int16_t ICACHE_FLASH_ATTR tdCOS( uint8_t iv )
{
    return tdSIN( iv + 64 );
}


void ICACHE_FLASH_ATTR tdIdentity( int16_t * matrix )
{
    matrix[0] = 256; matrix[1] = 0; matrix[2] = 0; matrix[3] = 0;
    matrix[4] = 0; matrix[5] = 256; matrix[6] = 0; matrix[7] = 0;
    matrix[8] = 0; matrix[9] = 0; matrix[10] = 256; matrix[11] = 0;
    matrix[12] = 0; matrix[13] = 0; matrix[14] = 0; matrix[15] = 256;
}

#define FBW 128
#define FBH 64

#define m00 0
#define m01 1
#define m02 2
#define m03 3
#define m10 4
#define m11 5
#define m12 6
#define m13 7
#define m20 8
#define m21 9
#define m22 10
#define m23 11
#define m30 12
#define m31 13
#define m32 14
#define m33 15
/*
int vTransform( flight_t * flightsim, int16_t * xformed, const int16_t * input )
{
    int16_t x = input[0];
    int16_t y = input[1];
    int16_t z = input[2];
    if( x == 0 && y == 0 && z == 0 ) return 0;

    x -= flightsim->planeloc[0];
    y -= flightsim->planeloc[1];
    z -= flightsim->planeloc[2];

    xformed[0] = (x>>7) + 20;
    xformed[1] = (y>>7) + 20;
    xformed[2] = (z>>7) + 20;
    return 1;
}
*/

void ICACHE_FLASH_ATTR Perspective( int fovx, int aspect, int zNear, int zFar, int16_t * out )
{
    int16_t f = fovx;
    out[0] = f*256/aspect; out[1] = 0; out[2] = 0; out[3] = 0;
    out[4] = 0; out[5] = f; out[6] = 0; out[7] = 0;
    out[8] = 0; out[9] = 0;
    out[10] = 256*(zFar + zNear)/(zNear - zFar);
    out[11] = 2*zFar*zNear  /(zNear - zFar);
    out[12] = 0; out[13] = 0; out[14] = -256; out[15] = 0;
}


void ICACHE_FLASH_ATTR SetupMatrix( )
{
    int16_t lmatrix[16];
    tdIdentity( ProjectionMatrix );
    tdIdentity( ModelviewMatrix );

    Perspective( 600, 128 /* 0.5 */, 50, 8192, ProjectionMatrix );
}

void ICACHE_FLASH_ATTR tdMultiply( int16_t * fin1, int16_t * fin2, int16_t * fout )
{
    int16_t fotmp[16];

    fotmp[m00] = ((int32_t)fin1[m00] * (int32_t)fin2[m00] + (int32_t)fin1[m01] * (int32_t)fin2[m10] + (int32_t)fin1[m02] * (int32_t)fin2[m20] + (int32_t)fin1[m03] * (int32_t)fin2[m30])>>8;
    fotmp[m01] = ((int32_t)fin1[m00] * (int32_t)fin2[m01] + (int32_t)fin1[m01] * (int32_t)fin2[m11] + (int32_t)fin1[m02] * (int32_t)fin2[m21] + (int32_t)fin1[m03] * (int32_t)fin2[m31])>>8;
    fotmp[m02] = ((int32_t)fin1[m00] * (int32_t)fin2[m02] + (int32_t)fin1[m01] * (int32_t)fin2[m12] + (int32_t)fin1[m02] * (int32_t)fin2[m22] + (int32_t)fin1[m03] * (int32_t)fin2[m32])>>8;
    fotmp[m03] = ((int32_t)fin1[m00] * (int32_t)fin2[m03] + (int32_t)fin1[m01] * (int32_t)fin2[m13] + (int32_t)fin1[m02] * (int32_t)fin2[m23] + (int32_t)fin1[m03] * (int32_t)fin2[m33])>>8;

    fotmp[m10] = ((int32_t)fin1[m10] * (int32_t)fin2[m00] + (int32_t)fin1[m11] * (int32_t)fin2[m10] + (int32_t)fin1[m12] * (int32_t)fin2[m20] + (int32_t)fin1[m13] * (int32_t)fin2[m30])>>8;
    fotmp[m11] = ((int32_t)fin1[m10] * (int32_t)fin2[m01] + (int32_t)fin1[m11] * (int32_t)fin2[m11] + (int32_t)fin1[m12] * (int32_t)fin2[m21] + (int32_t)fin1[m13] * (int32_t)fin2[m31])>>8;
    fotmp[m12] = ((int32_t)fin1[m10] * (int32_t)fin2[m02] + (int32_t)fin1[m11] * (int32_t)fin2[m12] + (int32_t)fin1[m12] * (int32_t)fin2[m22] + (int32_t)fin1[m13] * (int32_t)fin2[m32])>>8;
    fotmp[m13] = ((int32_t)fin1[m10] * (int32_t)fin2[m03] + (int32_t)fin1[m11] * (int32_t)fin2[m13] + (int32_t)fin1[m12] * (int32_t)fin2[m23] + (int32_t)fin1[m13] * (int32_t)fin2[m33])>>8;

    fotmp[m20] = ((int32_t)fin1[m20] * (int32_t)fin2[m00] + (int32_t)fin1[m21] * (int32_t)fin2[m10] + (int32_t)fin1[m22] * (int32_t)fin2[m20] + (int32_t)fin1[m23] * (int32_t)fin2[m30])>>8;
    fotmp[m21] = ((int32_t)fin1[m20] * (int32_t)fin2[m01] + (int32_t)fin1[m21] * (int32_t)fin2[m11] + (int32_t)fin1[m22] * (int32_t)fin2[m21] + (int32_t)fin1[m23] * (int32_t)fin2[m31])>>8;
    fotmp[m22] = ((int32_t)fin1[m20] * (int32_t)fin2[m02] + (int32_t)fin1[m21] * (int32_t)fin2[m12] + (int32_t)fin1[m22] * (int32_t)fin2[m22] + (int32_t)fin1[m23] * (int32_t)fin2[m32])>>8;
    fotmp[m23] = ((int32_t)fin1[m20] * (int32_t)fin2[m03] + (int32_t)fin1[m21] * (int32_t)fin2[m13] + (int32_t)fin1[m22] * (int32_t)fin2[m23] + (int32_t)fin1[m23] * (int32_t)fin2[m33])>>8;

    fotmp[m30] = ((int32_t)fin1[m30] * (int32_t)fin2[m00] + (int32_t)fin1[m31] * (int32_t)fin2[m10] + (int32_t)fin1[m32] * (int32_t)fin2[m20] + (int32_t)fin1[m33] * (int32_t)fin2[m30])>>8;
    fotmp[m31] = ((int32_t)fin1[m30] * (int32_t)fin2[m01] + (int32_t)fin1[m31] * (int32_t)fin2[m11] + (int32_t)fin1[m32] * (int32_t)fin2[m21] + (int32_t)fin1[m33] * (int32_t)fin2[m31])>>8;
    fotmp[m32] = ((int32_t)fin1[m30] * (int32_t)fin2[m02] + (int32_t)fin1[m31] * (int32_t)fin2[m12] + (int32_t)fin1[m32] * (int32_t)fin2[m22] + (int32_t)fin1[m33] * (int32_t)fin2[m32])>>8;
    fotmp[m33] = ((int32_t)fin1[m30] * (int32_t)fin2[m03] + (int32_t)fin1[m31] * (int32_t)fin2[m13] + (int32_t)fin1[m32] * (int32_t)fin2[m23] + (int32_t)fin1[m33] * (int32_t)fin2[m33])>>8;

    ets_memcpy( fout, fotmp, sizeof( fotmp ) );
}


void ICACHE_FLASH_ATTR tdTranslate( int16_t * f, int16_t x, int16_t y, int16_t z )
{
    int16_t ftmp[16];
    tdIdentity(ftmp);
    ftmp[m03] += x;
    ftmp[m13] += y;
    ftmp[m23] += z;
    tdMultiply( f, ftmp, f );
}

void ICACHE_FLASH_ATTR tdRotateEA( int16_t * f, int16_t x, int16_t y, int16_t z )
{
    int16_t ftmp[16];

    //x,y,z must be negated for some reason
    int16_t cx = tdCOS(x);
    int16_t sx = tdSIN(x);
    int16_t cy = tdCOS(y);
    int16_t sy = tdSIN(y);
    int16_t cz = tdCOS(z);
    int16_t sz = tdSIN(z);

    //Row major
    //manually transposed
    ftmp[m00] = (cy*cz)>>8;
    ftmp[m10] = ((((sx*sy)>>8)*cz)-(cx*sz))>>8;
    ftmp[m20] = ((((cx*sy)>>8)*cz)+(sx*sz))>>8;
    ftmp[m30] = 0;

    ftmp[m01] = (cy*sz)>>8;
    ftmp[m11] = ((((sx*sy)>>8)*sz)+(cx*cz))>>8;
    ftmp[m21] = ((((cx*sy)>>8)*sz)-(sx*cz))>>8;
    ftmp[m31] = 0;

    ftmp[m02] = -sy;
    ftmp[m12] = (sx*cy)>>8;
    ftmp[m22] = (cx*cy)>>8;
    ftmp[m32] = 0;

    ftmp[m03] = 0;
    ftmp[m13] = 0;
    ftmp[m23] = 0;
    ftmp[m33] = 1;

    tdMultiply( f, ftmp, f );
}

void ICACHE_FLASH_ATTR td4Transform( int16_t * pin, int16_t * f, int16_t * pout )
{
    int16_t ptmp[3];
    ptmp[0] = (pin[0] * f[m00] + pin[1] * f[m01] + pin[2] * f[m02] + pin[3] * f[m03])>>8;
    ptmp[1] = (pin[0] * f[m10] + pin[1] * f[m11] + pin[2] * f[m12] + pin[3] * f[m13])>>8;
    ptmp[2] = (pin[0] * f[m20] + pin[1] * f[m21] + pin[2] * f[m22] + pin[3] * f[m23])>>8;
    pout[3] = (pin[0] * f[m30] + pin[1] * f[m31] + pin[2] * f[m32] + pin[3] * f[m33])>>8;
    pout[0] = ptmp[0];
    pout[1] = ptmp[1];
    pout[2] = ptmp[2];
}


void ICACHE_FLASH_ATTR LocalToScreenspace( int16_t * coords_3v, int16_t * o1, int16_t * o2 )
{
    int16_t tmppt[4] = { coords_3v[0], coords_3v[1], coords_3v[2], 256 };
    td4Transform( tmppt, ModelviewMatrix, tmppt );
    td4Transform( tmppt, ProjectionMatrix, tmppt );
    if( tmppt[3] >= 0 ) { *o1 = -1; *o2 = -1; return; }

    *o1 = ((256 * tmppt[0] / tmppt[3])/8+(FBW/2))/2;
    *o2 = ((256 * tmppt[1] / tmppt[3])/8+(FBH/2));
}


void ICACHE_FLASH_ATTR Draw3DSegment( int16_t * c1, int16_t * c2 )
{
    int16_t sx0, sy0, sx1, sy1;
    LocalToScreenspace( c1, &sx0, &sy0 );
    LocalToScreenspace( c2, &sx1, &sy1 );

    //GPIO_OUTPUT_SET(GPIO_ID_PIN(1), 0 );
    speedyWhiteLine( sx0, sy0, sx1, sy1 );
    //GPIO_OUTPUT_SET(GPIO_ID_PIN(1), 1 ); 

    //plotLine( sx0, sy0, sx1, sy1, WHITE );
}


int16_t verts[] = { 
           0, -256,    0,   
         185, -114,  134,        -70, -114,  217,       -228, -114,    0,        -70, -114, -217,   
         185, -114, -134,         70,  114,  217,       -185,  114,  134,       -185,  114, -134,   
          70,  114, -217,        228,  114,    0,          0,  256,    0,        108, -217,   79,   
         -41, -217,  127,         67, -134,  207,        108, -217,  -79,        217, -134,    0,   
        -134, -217,    0,       -176, -134,  127,        -41, -217, -127,       -176, -134, -127,   
          67, -134, -207,        243,    0,  -79,        243,    0,   79,        150,    0,  207,   
           0,    0,  256,       -150,    0,  207,       -243,    0,   79,       -243,    0,  -79,   
        -150,    0, -207,          0,    0, -256,        150,    0, -207,        176,  134,  127,   
         -67,  134,  207,       -217,  134,    0,        -67,  134, -207,        176,  134, -127,   
         134,  217,    0,         41,  217,  127,       -108,  217,   79,       -108,  217,  -79,   
          41,  217, -127};
uint16_t indices[] = { /* 120 line segments */
          42,  36,     36,   3,      3,  42,     42,  39,     39,  36,      6,  39,     42,   6,     39,   0,   
           0,  36,     48,   3,     36,  48,     36,  45,     45,  48,     15,  48,     45,  15,      0,  45,   
          54,  39,      6,  54,     54,  51,     51,  39,      9,  51,     54,   9,     51,   0,     60,  51,   
           9,  60,     60,  57,     57,  51,     12,  57,     60,  12,     57,   0,     63,  57,     12,  63,   
          63,  45,     45,  57,     63,  15,     69,   3,     48,  69,     48,  66,     66,  69,     30,  69,   
          66,  30,     15,  66,     75,   6,     42,  75,     42,  72,     72,  75,     18,  75,     72,  18,   
           3,  72,     81,   9,     54,  81,     54,  78,     78,  81,     21,  81,     78,  21,      6,  78,   
          87,  12,     60,  87,     60,  84,     84,  87,     24,  87,     84,  24,      9,  84,     93,  15,   
          63,  93,     63,  90,     90,  93,     27,  93,     90,  27,     12,  90,     96,  69,     30,  96,   
          96,  72,     72,  69,     96,  18,     99,  75,     18,  99,     99,  78,     78,  75,     99,  21,   
         102,  81,     21, 102,    102,  84,     84,  81,    102,  24,    105,  87,     24, 105,    105,  90,   
          90,  87,    105,  27,    108,  93,     27, 108,    108,  66,     66,  93,    108,  30,    114,  18,   
          96, 114,     96, 111,    111, 114,     33, 114,    111,  33,     30, 111,    117,  21,     99, 117,   
          99, 114,    114, 117,     33, 117,    120,  24,    102, 120,    102, 117,    117, 120,     33, 120,   
         123,  27,    105, 123,    105, 120,    120, 123,     33, 123,    108, 111,    108, 123,    123, 111,   
        };

void ICACHE_FLASH_ATTR DrawGeoSphere()
{
    int i;
    int nrv = sizeof(indices)/sizeof(uint16_t);
    for( i = 0; i < nrv; i+=2 )
    {
        int16_t * c1 = &verts[indices[i]];
        int16_t * c2 = &verts[indices[i+1]];
        Draw3DSegment( c1, c2 );
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



static void ICACHE_FLASH_ATTR flightGameUpdate( flight_t * flight )
{
    uint8_t bs = flight->buttonState;

    // First clear the OLED
    clearDisplay();


    char framesStr[8] = {0};
    ets_snprintf(framesStr, sizeof(framesStr), "%d", flight->buttonState);
    plotText(0, 0, framesStr, TOM_THUMB, WHITE);

    if( bs & 1 ) flight->hpr[0]++;
    if( bs & 4 ) flight->hpr[0]--;

    static int ij;
    ij++;
    SetupMatrix();

    tdRotateEA( ProjectionMatrix, -20, 0, 0 );
    tdRotateEA( ModelviewMatrix, ij, 0, 0 );
    //tdTranslate( ModelviewMatrix, 0, 0, 200 );

    PIN_FUNC_SELECT( PERIPHS_IO_MUX_U0TXD_U, 3); //Set to GPIO.  
    GPIO_OUTPUT_SET(GPIO_ID_PIN(1), 0 );
    OVERCLOCK_SECTION_ENABLE();
//    ij = 0;    //Uncomment to prevent animation (for perf test)
    int x = 0;
    int y = 0;
    for( x = -3; x < 4; x++ )
    {
        for( y = 0; y < 4; y++ )
        {
            //7 * 4 * 120 = 3360 lines per frame.
            ModelviewMatrix[11] = 2400 + tdSIN( (x + y)*40 + ij*2 ) * 3;
            ModelviewMatrix[3] = 500*x-800;
            ModelviewMatrix[7] = 500*y+500;
            DrawGeoSphere();
        }
    }
    OVERCLOCK_SECTION_DISABLE();
    GPIO_OUTPUT_SET(GPIO_ID_PIN(1), 1 ); 

/*
    flight->planeloc[0] += tdSIN( flight->hpr[0] )>>4;
    flight->planeloc[1] += tdCOS( flight->hpr[0] )>>4;
    flight->planeloc[2] += tdCOS( flight->hpr[1] )>>4;
*/
//        x -= flightsim->planeplaneloc[0];
/*
    int i;
    int16_t xformlast[3];
    int newseg = 1;
    for( i = 0; ; i++ )
    {
        if( newseg )
        {
            if( vTransform( flight, xformlast, pittsburg+i*3 ) == 0 )
            {
                break;
            }
            
            newseg = 0;
        }
        else
        {
            int16_t xformednow[3];
            if( vTransform( flight, xformednow, pittsburg+i*3 ) == 0 ) { newseg = 1; continue; }
            plotLine( xformlast[0], xformlast[1], xformednow[0], xformednow[1], WHITE );
            ets_memcpy( xformlast, xformednow, sizeof( xformednow) );
        }
    }
*/

#if 0
    // For each chunk coordinate
    for(uint8_t w = 0; w < NUM_CHUNKS + 1; w++)
    {
        // Plot a floor segment line between chunk coordinates
        plotLine(
            (w * CHUNK_WIDTH) - flappy->xOffset,
            flappy->floors[w],
            ((w + 1) * CHUNK_WIDTH) - flappy->xOffset,
            flappy->floors[w + 1],
            WHITE);

        // Plot a ceiling segment line between chunk coordinates
        plotLine(
            (w * CHUNK_WIDTH) - flappy->xOffset,
            flappy->ceils[w],
            ((w + 1) * CHUNK_WIDTH) - flappy->xOffset,
            flappy->ceils[w + 1],
            WHITE);
    }

    // For each obstacle
    node_t* obs = flappy->obstacles.first;
    while(obs != NULL)
    {
        // Shift the obstacle
        obs->val = (void*)(((uintptr_t)obs->val) - 0x100);

        // Extract X and Y coordinates
        int8_t x = (((uintptr_t)obs->val) >> 8) & 0xFF;
        int8_t y = (((uintptr_t)obs->val)     ) & 0xFF;

        // If the obstacle is off the screen
        if(x + 2 <= 0)
        {
            // Move to the next
            obs = obs->next;
            // Remove it from the linked list
            removeEntry(&flappy->obstacles, obs->prev);
        }
        else
        {
            // Otherwise draw it
            plotRect(x, y, x + 2, y + flappy->obsHeight, WHITE);
            // Move to the next
            obs = obs->next;
        }
    }

    // Find the chopper's integer position
    int16_t chopperPos = (int16_t)(flappy->chopperPos + 0.5f);

    // Iterate over the chopper's sprite to see if it would be drawn over a wall
    bool collision = false;
    for(uint8_t x = 0; x < CHOPPER_HEIGHT; x++)
    {
        for(uint8_t y = 0; y < CHOPPER_HEIGHT; y++)
        {
            // The pixel is already white, so there's a collision!
            if(WHITE == getPixel(x, chopperPos + y))
            {
                collision = true;
                break;
            }
            else
            {
                drawPixel(x, chopperPos + y, WHITE);
            }
        }
    }

    // If there was a collision
    if(true == collision)
    {
        // Immediately jump back to the menu
        flappy->mode = FLAPPY_MENU;
        os_printf("Score: %d\n", flappy->frames / 8);
    }

    // Render the score as text
    char framesStr[8] = {0};
    ets_snprintf(framesStr, sizeof(framesStr), "%d", flappy->frames / 8);
    int16_t framesStrWidth = textWidth(framesStr, IBM_VGA_8);
    // Make sure the width is a multiple of 8 to keep it drawn consistently
    while(framesStrWidth % 8 != 0)
    {
        framesStrWidth++;
    }
    // Draw the score in the upper right hand corner
    fillDisplayArea(OLED_WIDTH - 1 - framesStrWidth, 0, OLED_WIDTH, FONT_HEIGHT_IBMVGA8 + 1, BLACK);
    plotText(OLED_WIDTH - framesStrWidth, 0, framesStr, IBM_VGA_8, WHITE);
#endif
}

/**
 * TODO
 *
 * @param state  A bitmask of all button states, unused
 * @param button The button which triggered this event
 * @param down   true if the button was pressed, false if it was released
 */
void ICACHE_FLASH_ATTR flightButtonCallback( uint8_t state,
        int button __attribute__((unused)), int down __attribute__((unused)))
{
    switch (flight->mode)
    {
        default:
        case FLIGHT_MENU:
        {
            if(down)
            {
                menuButton(flight->menu, button);

                static uint8_t shift = 0;
                static uint8_t mode = 0;
                led_t leds[NUM_LIN_LEDS] = {{0}};
                switch(mode)
                {
                    default:
                    case 0:
                    {
                        leds[0].r = 0xFF >> shift;
                        leds[1].g = 0xFF >> shift;
                        leds[2].b = 0xFF >> shift;
                        leds[3].b = 0xFF >> shift;
                        leds[4].g = 0xFF >> shift;
                        leds[5].r = 0xFF >> shift;
                        break;
                    }
                    case 1:
                    {
                        leds[0].r = 0xFF >> shift;
                        leds[0].g = 0xFF >> shift;
                        leds[1].g = 0xFF >> shift;
                        leds[1].b = 0xFF >> shift;
                        leds[2].b = 0xFF >> shift;
                        leds[2].r = 0xFF >> shift;
                        leds[3].b = 0xFF >> shift;
                        leds[3].r = 0xFF >> shift;
                        leds[4].g = 0xFF >> shift;
                        leds[4].b = 0xFF >> shift;
                        leds[5].r = 0xFF >> shift;
                        leds[5].g = 0xFF >> shift;
                        break;
                    }
                    case 2:
                    {
                        ets_memset(leds, 0xFF >> shift, sizeof(leds));
                        break;
                    }
                }
                shift = (shift + 1) % 8;
                if(0 == shift)
                {
                    mode = (mode + 1) % 3;
                }
                setLeds(leds, sizeof(leds));

            }
            break;
        }
        case FLIGHT_GAME:
        {
            // if(down)
            // {
            //     flappy->chopperVel = FLAPPY_JUMP_VEL;
            // }
            flight->buttonState = state;
            break;
        }
    }
}



static int16_t pittsburg[] = {
-4851,33852,0,-5301,32379,0,-5487,31325,0,-5115,30101,0,-4014,29062,0,
-2573,28675,0,-1498,28633,0,-423,28592,0,651,28551,0,1790,27985,0,2929,
27419,0,3632,26603,0,4334,25786,0,5037,24970,0,5409,23699,0,5781,22428,0,6153,
21157,0,5928,20173,0,5704,19189,0,5680,17693,0,5657,16197,0,6122,15255,0,6587,
14314,0,7052,13372,0,7517,12431,0,8401,11237,0,8571,11268,0,8982,12710,0,9393,
14151,0,9889,15213,0,10385,16275,0,11185,17205,0,11986,18135,0,12787,19065,0,
13516,19785,0,14244,20506,0,14973,21227,0,15701,21948,0,16729,22630,0,17757,
23312,0,18786,23994,0,19741,24490,0,20697,24986,0,21653,25482,0,22924,26737,0,
23420,27677,0,23916,28618,0,24412,29558,0,24893,30907,0,25373,32255,0,0,0,0,
-5890,33991,0,-6083,32666,0,-6277,31341,0,-5936,29822,0,-4557,28380,0,-2836,
27838,0,-1756,27796,0,-676,27755,0,403,27714,0,1689,27233,0,2658,26427,0,3627,
25621,0,4185,24668,0,4743,23715,0,4983,22475,0,5223,21235,0,5073,20155,0,4923,
19075,0,4774,17995,0,4758,16833,0,4743,15670,0,5266,14779,0,5789,13888,0,6312,
12996,0,6835,12105,0,7445,11160,0,8054,10214,0,8664,9269,0,9362,7471,0,10049,
6680,0,10736,5890,0,11423,5099,0,12282,4274,0,13140,3450,0,13999,2625,0,14858,
1801,0,15717,976,0,16972,573,0,18197,1007,0,18631,1550,0,19623,1472,0,20599,
1069,0,21917,-124,0,0,0,0,22707,620,0,21343,1875,0,20049,2348,0,18755,2821,0,
17786,3410,0,16817,3999,0,15748,4611,0,14678,5223,0,13632,5332,0,12586,5440,0,
11578,6262,0,10431,7657,0,9842,9114,0,9455,11051,0,9594,11981,0,10075,13857,0,
10586,14911,0,11098,15965,0,12152,17329,0,13004,18160,0,13857,18992,0,14709,
19824,0,15593,20320,0,16476,20816,0,17360,21312,0,18135,22041,0,18910,22769,0,
19741,23343,0,20573,23916,0,21405,24490,0,22482,25443,0,23560,26396,0,24288,
27435,0,25017,28473,0,25347,29455,0,25678,30436,0,26009,31418,0,26210,32767,0,
26412,34115,0,0,0,0,6525,21638,-30,6335,20611,-30,6145,19584,-30,5955,18557,
-30,5766,17530,-30,5797,16259,-30,6401,16523,-30,7540,16918,-30,8680,17313,-30,
9594,17825,-30,10462,17747,-30,11353,16941,-30,12245,16135,-30,12973,17019,-30,
0,0,0,10385,17577,-30,9625,17654,-30,8742,17251,-30,7571,16848,-30,6401,16445,
-30,5828,16182,-30,6296,15236,-30,6765,14291,-30,7234,13345,-30,7703,12400,-30,
8881,12570,-30,10059,12245,-30,10788,12849,-30,11160,14678,-30,12152,15996,-30,
11268,16786,-30,10385,17577,-30,10385,17577,-30,0,0,0,10323,13903,-30,9331,
14182,-30,9346,14229,-30,10338,13950,-30,10323,13903,-30,10323,13903,-30,0,0,0,
10571,14430,-30,9579,14709,-30,9594,14756,-30,10586,14477,-30,10571,14430,-30,
10571,14430,-30,0,0,0,10757,14880,-30,9765,15159,-30,9780,15205,-30,10772,
14926,-30,10757,14880,-30,10757,14880,-30,0,0,0,0,0,0,8602,16492,-30,8602,
16492,-841,8525,16678,-841,8602,16492,-570,8525,16678,-570,8602,16492,-300,8525,
16678,-300,8602,16492,-30,8525,16678,-30,8525,16678,-841,8711,16647,-841,8525,
16678,-570,8711,16647,-570,8525,16678,-300,8711,16647,-300,8525,16678,-30,8711,
16647,-30,8711,16647,-841,8602,16492,-841,8711,16647,-570,8602,16492,-570,8711,
16647,-300,8602,16492,-300,8711,16647,-30,8602,16492,-30,8602,16492,-841,8602,
16492,-841,8602,16492,-570,8602,16492,-570,8602,16492,-300,8602,16492,-300,
8602,16492,-30,8602,16492,-30,0,0,0,7905,16151,-30,7905,16151,-725,7827,16182,
-725,7905,16151,-377,7827,16182,-377,7905,16151,-30,7827,16182,-30,7827,16182,
-725,7781,16275,-725,7827,16182,-377,7781,16275,-377,7827,16182,-30,7781,16275,
-30,7781,16275,-725,7812,16337,-725,7781,16275,-377,7812,16337,-377,7781,16275,
-30,7812,16337,-30,7812,16337,-725,7889,16383,-725,7812,16337,-377,7889,16383,
-377,7812,16337,-30,7889,16383,-30,7889,16383,-725,7967,16352,-725,7889,16383,
-377,7967,16352,-377,7889,16383,-30,7967,16352,-30,7967,16352,-725,8013,16259,
-725,7967,16352,-377,8013,16259,-377,7967,16352,-30,8013,16259,-30,8013,16259,
-725,7982,16182,-725,8013,16259,-377,7982,16182,-377,8013,16259,-30,7982,16182,
-30,7982,16182,-725,7905,16151,-725,7982,16182,-377,7905,16151,-377,7982,16182,
-30,7905,16151,-30,7905,16151,-725,7905,16151,-725,7905,16151,-377,7905,16151,
-377,7905,16151,-30,7905,16151,-30,0,0,0,8246,13082,-30,8246,13082,-250,8277,
13159,-250,8246,13082,-30,8277,13159,-30,8277,13159,-250,8556,13082,-250,8277,
13159,-30,8556,13082,-30,8556,13082,-250,8525,13004,-250,8556,13082,-30,8525,
13004,-30,8525,13004,-250,8246,13082,-250,8525,13004,-30,8246,13082,-30,8246,
13082,-250,8246,13082,-250,8246,13082,-30,8246,13082,-30,0,0,0,8416,13981,-30,
8416,13981,-450,8478,14151,-450,8416,13981,-240,8478,14151,-240,8416,13981,-30,
8478,14151,-30,8478,14151,-450,8649,14089,-450,8478,14151,-240,8649,14089,-240,
8478,14151,-30,8649,14089,-30,8649,14089,-450,8587,13919,-450,8649,14089,-240,
8587,13919,-240,8649,14089,-30,8587,13919,-30,8587,13919,-450,8416,13981,-450,
8587,13919,-240,8416,13981,-240,8587,13919,-30,8416,13981,-30,8416,13981,-450,
8416,13981,-450,8416,13981,-240,8416,13981,-240,8416,13981,-30,8416,13981,-30,
0,0,0,7657,13640,-30,7657,13640,-630,7595,13795,-630,7657,13640,-330,7595,
13795,-330,7657,13640,-30,7595,13795,-30,7595,13795,-630,7750,13857,-630,7595,
13795,-330,7750,13857,-330,7595,13795,-30,7750,13857,-30,7750,13857,-630,7812,
13702,-630,7750,13857,-330,7812,13702,-330,7750,13857,-30,7812,13702,-30,7812,
13702,-630,7657,13640,-630,7812,13702,-330,7657,13640,-330,7812,13702,-30,7657,
13640,-30,7657,13640,-630,7657,13640,-630,7657,13640,-330,7657,13640,-330,7657,
13640,-30,7657,13640,-30,0,0,0,6882,15267,-30,6882,15267,-600,6820,15422,-600,
6882,15267,-410,6820,15422,-410,6882,15267,-220,6820,15422,-220,6882,15267,-30,
6820,15422,-30,6820,15422,-600,6975,15484,-600,6820,15422,-410,6975,15484,-410,
6820,15422,-220,6975,15484,-220,6820,15422,-30,6975,15484,-30,6975,15484,-600,
7037,15329,-600,6975,15484,-410,7037,15329,-410,6975,15484,-220,7037,15329,
-220,6975,15484,-30,7037,15329,-30,7037,15329,-600,6882,15267,-600,7037,15329,
-410,6882,15267,-410,7037,15329,-220,6882,15267,-220,7037,15329,-30,6882,15267,
-30,6882,15267,-600,6882,15267,-600,6882,15267,-410,6882,15267,-410,6882,15267,
-220,6882,15267,-220,6882,15267,-30,6882,15267,-30,0,0,0,8339,14461,-30,8339,
14461,-500,8261,14647,-500,8339,14461,-265,8261,14647,-265,8339,14461,-30,8261,
14647,-30,8261,14647,-500,8401,14709,-500,8261,14647,-265,8401,14709,-265,8261,
14647,-30,8401,14709,-30,8401,14709,-500,8478,14523,-500,8401,14709,-265,8478,
14523,-265,8401,14709,-30,8478,14523,-30,8478,14523,-500,8339,14461,-500,8478,
14523,-265,8339,14461,-265,8478,14523,-30,8339,14461,-30,8339,14461,-500,8339,
14461,-500,8339,14461,-265,8339,14461,-265,8339,14461,-30,8339,14461,-30,0,0,0,
7440,13500,-30,7440,13500,-350,7300,13438,-350,7440,13500,-190,7300,13438,-190,
7440,13500,-30,7300,13438,-30,7300,13438,-350,7223,13609,-350,7300,13438,-190,
7223,13609,-190,7300,13438,-30,7223,13609,-30,7223,13609,-350,7362,13671,-350,
7223,13609,-190,7362,13671,-190,7223,13609,-30,7362,13671,-30,7362,13671,-350,
7440,13500,-350,7362,13671,-190,7440,13500,-190,7362,13671,-30,7440,13500,-30,
7440,13500,-350,7440,13500,-350,7440,13500,-190,7440,13500,-190,7440,13500,-30,
7440,13500,-30,0,0,0,7192,11547,0,7192,11547,-30,7292,11698,-151,7393,11849,
-191,7494,12000,-151,7595,12152,-30,7595,12152,0,0,0,0,7269,11501,0,7269,11501,
-30,7366,11652,-149,7463,11803,-189,7560,11954,-149,7657,12105,-30,7657,12105,
0,0,0,0,5688,14198,0,5688,14198,-80,5866,14291,-130,6223,14477,-30,6401,14570,
-30,6223,14477,-130,5866,14291,-30,5688,14198,-80,0,0,0,6401,14570,-30,6401,
14570,0,0,0,0,6370,14632,0,6370,14632,-80,6192,14539,-130,5835,14353,-30,5657,
14260,-30,5835,14353,-130,6192,14539,-30,6370,14632,-80,0,0,0,5657,14260,-30,
5657,14260,0,0,0,0,9641,12369,0,9641,12369,-30,9482,12411,-139,9323,12454,-176,
9164,12496,-139,9005,12539,-30,9005,12539,0,0,0,0,9610,12260,0,9610,12260,-30,
9451,12303,-139,9292,12345,-176,9133,12388,-139,8974,12431,-30,8974,12431,0,0,
0,0,10168,13950,-30,10092,13971,-60,10016,13993,-150,9902,14025,-60,9788,14058,
-35,9674,14091,-60,9560,14123,-150,9484,14145,-60,9408,14167,-30,0,0,0,10016,
13993,-150,10016,13993,0,0,0,0,9560,14123,-150,9560,14123,0,0,0,0,10183,13996,
-30,10107,14018,-60,10031,14039,-150,9917,14072,-60,9803,14105,-35,9689,14137,
-60,9575,14170,-150,9499,14191,-60,9424,14213,-30,0,0,0,10031,14039,-150,10031,
14039,0,0,0,0,9575,14170,-150,9575,14170,0,0,0,0,10369,14492,-30,10296,14512,
-60,10223,14532,-150,10114,14563,-60,10005,14593,-35,9895,14623,-60,9786,14653,
-150,9713,14673,-60,9641,14694,-30,0,0,0,10223,14532,-150,10223,14532,0,0,0,0,
9786,14653,-150,9786,14653,0,0,0,0,10385,14539,-30,10312,14559,-60,10239,14579,
-150,10130,14609,-60,10020,14639,-35,9911,14669,-60,9802,14700,-150,9729,14720,
-60,9656,14740,-30,0,0,0,10239,14579,-150,10239,14579,0,0,0,0,9802,14700,-150,
9802,14700,0,0,0,0,10602,14926,-30,10526,14948,-60,10450,14969,-150,10336,15002,
-60,10222,15035,-35,10108,15067,-60,9994,15100,-150,9918,15121,-60,9842,15143,
-30,0,0,0,10450,14969,-150,10450,14969,0,0,0,0,9994,15100,-150,9994,15100,0,0,0,
0,10617,14973,-30,10541,14994,-60,10465,15016,-150,10351,15048,-60,10237,15081,
-35,10123,15114,-60,10009,15146,-150,9933,15168,-60,9858,15190,-30,0,0,0,10465,
15016,-150,10465,15016,0,0,0,0,10009,15146,-150,10009,15146,0,0,0,0,8416,11640,
0,8277,11749,0,8385,11873,0,8509,11764,0,8416,11640,0,8416,11640,0,0,0,0,6432,
21638,-30,6246,20626,-30,6060,19615,-30,5874,18603,-30,5688,17592,-30,5735,
16213,-30,4107,15314,-30,4138,15236,-30,5766,16151,-30,6479,14694,-30,5099,
13965,-30,5130,13903,-30,6510,14632,-30,7044,13531,-30,7579,12431,-30,7579,
12136,-30,6789,10943,-30,6851,10881,-30,7781,12291,-30,8866,12462,-30,10106,
12152,-30,10819,11516,-30,10839,10493,-30,10860,9470,-30,10881,8447,-30,11051,
7827,-30,9300,7037,-30,9346,6959,-30,11098,7750,-30,11532,7083,-30,12198,6680,
-30,0,0,0,12260,6820,-30,11640,7223,-30,11253,7812,-30,11051,8509,-30,11016,
9567,-30,10981,10625,-30,10946,11683,-30,10912,12741,-30,11284,14601,-30,12183,
15779,-30,13082,16957,-30,0,0,0,10788,11702,-30,10199,12198,-30,10757,12663,
-30,10788,11702,-30,10788,11702,-30,0,0,0,10524,7579,0,10524,7579,-30,10261,
7463,-222,9997,7347,-286,9734,7230,-222,9470,7114,-30,9470,7114,0,0,0,0,10602,
7517,0,10602,7517,-30,10338,7401,-222,10075,7285,-286,9811,7168,-222,9548,7052,
-30,9548,7052,0,0,0,0,16352,1581,0,15558,2305,0,14763,3030,0,13969,3754,0,
13175,4479,0,14260,4495,0,15391,4045,0,16316,3410,0,17241,2774,0,18166,2139,0,
18150,1720,0,17670,1457,0,16352,1581,0,16352,1581,0,0,0,0,20227,976,-30,20754,
2402,-30,20692,2418,-30,20165,1038,-30,20227,976,-30,20227,976,-30,0,0,0,16724,
21018,0,17507,21700,0,18290,22382,0,18150,22521,0,17088,21979,0,16027,21436,0,
14802,20181,0,15763,20599,0,16724,21018,0,16724,21018,0,0,0,0,12400,17019,-30,
11408,17949,-30,11470,17980,-30,12446,17050,-30,12400,17019,-30,12400,17019,
-30,0,0,0,16414,20553,-30,15624,21963,-30,15686,22025,-30,16476,20599,-30,
16414,20553,-30,16414,20553,-30,0,0,0,19654,23002,-30,18693,24242,-30,18770,
24288,-30,19747,23048,-30,19654,23002,-30,19654,23002,-30,0,0,0,5704,17887,-30,
4371,18228,-30,4402,18290,-30,5750,17933,-30,5704,17887,-30,5704,17887,-30,0,0,
0,6138,22521,-30,4836,22335,-30,4836,22444,-30,6138,22614,-30,6138,22521,-30,
6138,22521,-30,0,0,0,-4913,32472,-30,-6463,32643,-30,-6401,32736,-30,-4944,
32581,-30,-4913,32472,-30,-4913,32472,-30,0,0,0,26365,31976,-30,24939,32302,
-30,24986,32410,-30,26412,32069,-30,26365,31976,-30,26365,31976,-30,0,0,0,0,0,0
};

