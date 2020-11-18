/*
 * mode_flight.c
 *
 *  Created on: Sept 15, 2020
 *      Author: <>< CNLohr

 * Consider doing an optimizing video discussing:
   -> De-swizzling format
   -> Faster I2C Transfer
   -> Vertex Caching
   -> Occlusion Testing
   -> Fast line-drawing algorithm
   -> Switch Statement at end of OLED.c
   -> Weird code structure of drawPixelUnsafeC
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
#include "cndraw.h"
#include "bresenham.h"
#include "assets.h"
#include "buttons.h"
#include "menu2d.h"
#include "linked_list.h"
#include "font.h"
#include "gpio.h"
#include "esp_niceness.h"
#include "hsv_utils.h"

#include "embeddednf.h"
#include "embeddedout.h"

/*============================================================================
 * Defines, Structs, Enums
 *==========================================================================*/

#define FLIGHT_UPDATE_MS 33

typedef enum
{
    FL_PERFTEST,
    FL_TRIANGLES,
    FL_ENV,
} flGameType;


typedef enum
{
    FLIGHT_MENU,
    FLIGHT_GAME,
    FLIGHT_GAME_OVER,
} flightModeScreen;

typedef struct
{
    uint16_t nrvertnums;
    uint16_t nrfaces;
    uint16_t indices_per_face;
    int16_t center[3];
    int16_t radius;
    uint16_t label;
    int16_t indices_and_vertices[1];
} tdModel;

#define MAXRINGS 30

typedef enum
{
    FLIGHT_LED_NONE,
    FLIGHT_LED_MENU_TICK,
    FLIGHT_LED_GAME_START,
    FLIGHT_LED_BEAN,
    FLIGHT_LED_DONUT,
    FLIGHT_LED_ENDING,
} flLEDAnimation;

typedef struct
{
    flightModeScreen mode;

    timer_t updateTimer;
    int frames;
    uint8_t buttonState;
    flGameType type;

    int16_t planeloc[3];
    int16_t hpr[3];
    int16_t speed;
    int16_t pitchmoment;
    int16_t yawmoment;
    bool perfMotion;
    tdModel * isosphere;

    int enviromodels;
    tdModel ** environment;

    menu_t* menu;

    int beans;
    int ondonut;
    int timer;
    int wintime;

    flLEDAnimation ledAnimation;
    uint8_t        ledAnimationTime;

    uint8_t beangotmask[MAXRINGS];
} flight_t;

int renderlinecolor = WHITE;

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
static bool ICACHE_FLASH_ATTR flightRender(void);
static void ICACHE_FLASH_ATTR flightGameUpdate( flight_t * tflight );
static void ICACHE_FLASH_ATTR flightUpdateLEDs(flight_t * tflight);
static void ICACHE_FLASH_ATTR flightLEDAnimate( flLEDAnimation anim );
static tdModel * ICACHE_FLASH_ATTR tdAllocateModel( int faces, const uint16_t * indices, const int16_t * vertices, int indices_per_face /* 2= lines 3= tris */ );
int ICACHE_FLASH_ATTR tdModelVisibilitycheck( const tdModel * m );
void ICACHE_FLASH_ATTR tdDrawModel( const tdModel * m );

//Forward libc declarations.
void qsort(void *base, size_t nmemb, size_t size,
          int (*compar)(const void *, const void *));
int abs(int j);


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
    .fnRenderTask = flightRender,
    .fnAccelerometerCallback = NULL,
    .fnAudioCallback = NULL,
    .menuImg = "flight-menu.gif"
};

flight_t* flight;

static const char fl_title[]  = "Flightsim";
// static const char fl_flight_perf[] = "PERF";
// static const char fl_flight_triangles[] = "TRIS";
static const char fl_flight_env[] = "Take Flight";
static const char fl_quit[]   = "QUIT";

/*============================================================================
 * Functions
 *==========================================================================*/






static const int16_t IsoSphereVertices[] RODATA_ATTR = { 
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
static const uint16_t IsoSphereIndices[] RODATA_ATTR = { /* 120 line segments */
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



/**
 * Initializer for flight
 */
void ICACHE_FLASH_ATTR flightEnterMode(void)
{
    // Alloc and clear everything
    flight = os_malloc(sizeof(flight_t));
    ets_memset(flight, 0, sizeof(flight_t));

    flight->mode = FLIGHT_MENU;
    flight->isosphere = tdAllocateModel( sizeof(IsoSphereIndices)/sizeof(uint16_t)/2, IsoSphereIndices, IsoSphereVertices, 2 );

    {
        uint32_t retlen;
        uint16_t * data = (uint16_t*)getAsset( "3denv.obj", &retlen );
        data+=2; //header
        flight->enviromodels = *(data++);
        flight->environment = os_malloc( sizeof(tdModel *) * flight->enviromodels );
        int i;
        for( i = 0; i < flight->enviromodels; i++ )
        {
            tdModel * m = flight->environment[i] = (tdModel*)data;
            data += 8 + m->nrvertnums + m->nrfaces * m->indices_per_face;
        }
    }

    flight->menu = initMenu(fl_title, flightMenuCb);
    addRowToMenu(flight->menu);
    // addItemToRow(flight->menu, fl_flight_perf);
    // addItemToRow(flight->menu, fl_flight_triangles);
    addItemToRow(flight->menu, fl_flight_env);
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
    os_free(flight->isosphere);
    os_free(flight);
}

/**
 * TODO
 *
 * @param menuItem
 */
static void ICACHE_FLASH_ATTR flightMenuCb(const char* menuItem)
{
    /*
    if( fl_flight_triangles == menuItem )
    {
        flightStartGame(FL_TRIANGLES);
    }
    else if( fl_flight_perf == menuItem )
    {
        flightStartGame(FL_PERFTEST);
    }
    else */ if (fl_flight_env == menuItem)
    {
        flightStartGame(FL_ENV);
    }
    else if (fl_quit == menuItem)
    {
        switchToSwadgeMode(0);
    }
}

static void ICACHE_FLASH_ATTR flightEndGame()
{
    flight->mode = FLIGHT_MENU;
    //called when ending animation complete.
}

static void ICACHE_FLASH_ATTR flightLEDAnimate( flLEDAnimation anim )
{
    flight->ledAnimation = anim;
    flight->ledAnimationTime = 0;
}

static void ICACHE_FLASH_ATTR flightUpdateLEDs(flight_t * tflight)
{    
    led_t leds[NUM_LIN_LEDS] = {{0}};

    uint8_t        ledAnimationTime = tflight->ledAnimationTime++;

    switch( tflight->ledAnimation )
    {
    default:
    case FLIGHT_LED_NONE:
        tflight->ledAnimationTime = 0;
        break;
    case FLIGHT_LED_ENDING:
        leds[0] = SafeEHSVtoHEXhelper(ledAnimationTime*4+0, 255, 2200-10*ledAnimationTime, 1 );
        leds[1] = SafeEHSVtoHEXhelper(ledAnimationTime*4+50, 255, 2200-10*ledAnimationTime, 1 );
        leds[2] = SafeEHSVtoHEXhelper(ledAnimationTime*4+100, 255, 2200-10*ledAnimationTime, 1 );
        leds[3] = SafeEHSVtoHEXhelper(ledAnimationTime*4+150, 255, 2200-10*ledAnimationTime, 1 );
        leds[4] = SafeEHSVtoHEXhelper(ledAnimationTime*4+200, 255, 2200-10*ledAnimationTime, 1 );
        leds[5] = SafeEHSVtoHEXhelper(ledAnimationTime*4+250, 255, 2200-10*ledAnimationTime, 1 );
        if( ledAnimationTime == 255 ) flightLEDAnimate( FLIGHT_LED_NONE );
        break;
    case FLIGHT_LED_GAME_START:
    case FLIGHT_LED_DONUT:
        leds[0] = leds[5] = SafeEHSVtoHEXhelper(ledAnimationTime*8+0, 255, 200-10*ledAnimationTime, 1 );
        leds[1] = leds[4] = SafeEHSVtoHEXhelper(ledAnimationTime*8+90, 255, 200-10*ledAnimationTime, 1 );
        leds[2] = leds[3] = SafeEHSVtoHEXhelper(ledAnimationTime*8+180, 255, 200-10*ledAnimationTime, 1 );
        if( ledAnimationTime == 30 ) flightLEDAnimate( FLIGHT_LED_NONE );
        break;
    case FLIGHT_LED_MENU_TICK:
        leds[0] = leds[5] = SafeEHSVtoHEXhelper(0, 0, 60 - 40*abs(ledAnimationTime-2), 1 );
        leds[1] = leds[4] = SafeEHSVtoHEXhelper(0, 0, 60 - 40*abs(ledAnimationTime-6), 1 );
        leds[2] = leds[3] = SafeEHSVtoHEXhelper(0, 0, 60 - 40*abs(ledAnimationTime-10), 1 );
        if( ledAnimationTime == 50 ) flightLEDAnimate( FLIGHT_LED_NONE );
        break;
    case FLIGHT_LED_BEAN:    
        leds[0] = leds[5] = SafeEHSVtoHEXhelper(ledAnimationTime*16, 128, 150 - 40*abs(ledAnimationTime-2), 1 );
        leds[1] = leds[4] = SafeEHSVtoHEXhelper(ledAnimationTime*16, 128, 150 - 40*abs(ledAnimationTime-6), 1 );
        leds[2] = leds[3] = SafeEHSVtoHEXhelper(ledAnimationTime*16, 128, 150 - 40*abs(ledAnimationTime-10), 1 );        
        if( ledAnimationTime == 30 ) flightLEDAnimate( FLIGHT_LED_NONE );
        break;
    }

    setLeds(leds, sizeof(leds));
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


    flight->planeloc[0] = 800;
    flight->planeloc[1] = 400;
    flight->planeloc[2] = -500;
    flight->ondonut = 0; //SEt to 14 to b-line it to the end 
    flight->beans = 0;
    flight->timer = 0;
    flight->wintime = 0;
    flight->speed = 0;
    flight->hpr[0] = 0;
    flight->hpr[1] = 0;
    flight->hpr[2] = 0;
    flight->pitchmoment = 0;
    flight->yawmoment = 0;


    ets_memset(flight->beangotmask, 0, sizeof( flight->beangotmask) );

    flightLEDAnimate( FLIGHT_LED_GAME_START );
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
            break;
        }
        case FLIGHT_GAME_OVER:
        {
            flight->frames++;
            flightGameUpdate( flight );
            if( flight->frames > 200 ) flight->frames = 200; //Keep it at 200, so we can click any button to continue.
            break;
        }
    }

    flightUpdateLEDs( flight );

}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int16_t ICACHE_FLASH_ATTR tdCOS( uint8_t iv );
void ICACHE_FLASH_ATTR tdIdentity( int16_t * matrix );
static int16_t ICACHE_FLASH_ATTR tdSIN( uint8_t iv );
void ICACHE_FLASH_ATTR Perspective( int fovx, int aspect, int zNear, int zFar, int16_t * out );
int ICACHE_FLASH_ATTR LocalToScreenspace( const int16_t * coords_3v, int16_t * o1, int16_t * o2 );
void ICACHE_FLASH_ATTR SetupMatrix( void );
void ICACHE_FLASH_ATTR tdMultiply( int16_t * fin1, int16_t * fin2, int16_t * fout );
void ICACHE_FLASH_ATTR tdRotateEA( int16_t * f, int16_t x, int16_t y, int16_t z );
void ICACHE_FLASH_ATTR tdScale( int16_t * f, int16_t x, int16_t y, int16_t z );
void ICACHE_FLASH_ATTR td4Transform( int16_t * pin, int16_t * f, int16_t * pout );
void ICACHE_FLASH_ATTR tdTranslate( int16_t * f, int16_t x, int16_t y, int16_t z );
void ICACHE_FLASH_ATTR Draw3DSegment( const int16_t * c1, const int16_t * c2 );
uint16_t ICACHE_FLASH_ATTR tdSQRT( uint32_t inval );
int16_t ICACHE_FLASH_ATTR tdDist( int16_t * a, int16_t * b );


//From https://github.com/cnlohr/channel3/blob/master/user/3d.c

static const uint8_t sintable[128] = { 0, 6, 12, 18, 25, 31, 37, 43, 49, 55, 62, 68, 74, 80, 86, 91, 97, 103, 109, 114, 120, 125, 131, 136, 141, 147, 152, 157, 162, 166, 171, 176, 180, 185, 189, 193, 197, 201, 205, 208, 212, 215, 219, 222, 225, 228, 230, 233, 236, 238, 240, 242, 244, 246, 247, 249, 250, 251, 252, 253, 254, 254, 255, 255, 255, 255, 255, 254, 254, 253, 252, 251, 250, 249, 247, 246, 244, 242, 240, 238, 236, 233, 230, 228, 225, 222, 219, 215, 212, 208, 205, 201, 197, 193, 189, 185, 180, 176, 171, 166, 162, 157, 152, 147, 141, 136, 131, 125, 120, 114, 109, 103, 97, 91, 86, 80, 74, 68, 62, 55, 49, 43, 37, 31, 25, 18, 12, 6, };

int16_t ModelviewMatrix[16];
int16_t ProjectionMatrix[16];

static int16_t ICACHE_FLASH_ATTR tdSIN( uint8_t iv )
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

uint16_t ICACHE_FLASH_ATTR tdSQRT( uint32_t inval )
{
    uint32_t res = 0;
    uint32_t one = 1UL << 30;
    while (one > inval)
    {
        one >>= 2;
    }

    while (one != 0)
    {
        if (inval >= res + one)
        {
            inval = inval - (res + one);
            res = res +  (one<<1);
        }
        res >>= 1;
        one >>= 2;
    }
    return res;
}

int16_t ICACHE_FLASH_ATTR tdDist( int16_t * a, int16_t * b )
{
    int32_t dx = a[0] - b[0];
    int32_t dy = a[1] - b[1];
    int32_t dz = a[2] - b[2];
    return tdSQRT( dx*dx+dy*dy+dz*dz );
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


void ICACHE_FLASH_ATTR SetupMatrix( void )
{
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

void ICACHE_FLASH_ATTR tdScale( int16_t * f, int16_t x, int16_t y, int16_t z )
{
    f[m00] = (f[m00] * x)>>8;
    f[m01] = (f[m01] * x)>>8;
    f[m02] = (f[m02] * x)>>8;
//    f[m03] = (f[m03] * x)>>8;

    f[m10] = (f[m10] * y)>>8;
    f[m11] = (f[m11] * y)>>8;
    f[m12] = (f[m12] * y)>>8;
//    f[m13] = (f[m13] * y)>>8;

    f[m20] = (f[m20] * z)>>8;
    f[m21] = (f[m21] * z)>>8;
    f[m22] = (f[m22] * z)>>8;
//    f[m23] = (f[m23] * z)>>8;
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


int ICACHE_FLASH_ATTR LocalToScreenspace( const int16_t * coords_3v, int16_t * o1, int16_t * o2 )
{
    int16_t tmppt[4] = { coords_3v[0], coords_3v[1], coords_3v[2], 256 };
    td4Transform( tmppt, ModelviewMatrix, tmppt );
    td4Transform( tmppt, ProjectionMatrix, tmppt );
    if( tmppt[3] >= -4 ) { return -1; }
    int calcx = ((256 * tmppt[0] / tmppt[3])/16+(FBW/2));
    int calcy = ((256 * tmppt[1] / tmppt[3])/8+(FBH/2));
    if( calcx < -16000 || calcx > 16000 || calcy < -16000 || calcy > 16000 ) return -2;
    *o1 = calcx;
    *o2 = calcy;
    return 0;
}


void ICACHE_FLASH_ATTR Draw3DSegment( const int16_t * c1, const int16_t * c2 )
{
    int16_t sx0, sy0, sx1, sy1;
    if( LocalToScreenspace( c1, &sx0, &sy0 ) ||
        LocalToScreenspace( c2, &sx1, &sy1 ) ) return;

    //GPIO_OUTPUT_SET(GPIO_ID_PIN(1), 0 );
    speedyWhiteLine( sx0, sy0, sx1, sy1, false );
    //GPIO_OUTPUT_SET(GPIO_ID_PIN(1), 1 ); 

    //plotLine( sx0, sy0, sx1, sy1, WHITE );
}

static tdModel * ICACHE_FLASH_ATTR tdAllocateModel( int nrfaces, const uint16_t * indices, const int16_t * vertices, int indices_per_face )
{
    int i;
    int highest_v = 0;
    for( i = 0; i < nrfaces*2; i++ )
    {
        if( indices[i] > highest_v ) highest_v = indices[i];
    }
    highest_v += 3; //We only looked at indices.

    tdModel * ret = os_malloc( sizeof( tdModel ) + highest_v * sizeof(uint16_t) + nrfaces * sizeof(uint16_t) * 2  );
    ret->nrfaces = nrfaces;
    ret->indices_per_face = indices_per_face;

    ets_memcpy( ret->indices_and_vertices, indices, nrfaces * sizeof(uint16_t) * 2 );
    int16_t * voffset = &ret->indices_and_vertices[nrfaces * 2];

    int16_t mins[3] = {  0x7fff,  0x7fff,  0x7fff };
    int16_t maxs[3] = { -0x7fff, -0x7fff, -0x7fff };
    for( i = 0; i < highest_v; i+=3 )
    {
        const int16_t * v = &vertices[i];
        int k;
        for( k = 0; k < 3; k++ )
        {
            int16_t vk = v[k];
            if( vk < mins[k] ) mins[k] = vk;
            if( vk > maxs[k] ) maxs[k] = vk;
            voffset[i+k] = v[k];
        }
    }
    ret->nrvertnums = highest_v;
    for( i = 0; i < 3; i++ )
    {
        ret->center[i] = (maxs[i] + mins[i])/2;
    }
    uint32_t dSq = 0;
    for( i = 0; i < highest_v; i+= 3 )
    {
        int k;
        uint32_t difftot = 0;
        for( k = 0; k < 3; k++ )
        {
            int32_t ik = vertices[i+k] - ret->center[k];
            difftot += ik*ik;
        }
        if( difftot > dSq ) dSq = difftot;
    }
    ret->radius = tdSQRT( dSq );

    return ret;
}

int ICACHE_FLASH_ATTR tdModelVisibilitycheck( const tdModel * m )
{

    //For computing visibility check
    int16_t tmppt[4] = { m->center[0], m->center[1], m->center[2], 256 }; //No multiplier seems to work right here.
    td4Transform( tmppt, ModelviewMatrix, tmppt );
    td4Transform( tmppt, ProjectionMatrix, tmppt );
    if( tmppt[3] < -2 )
    {
        int scx = ((256 * tmppt[0] / tmppt[3])/16+(OLED_WIDTH/2));
        int scy = ((256 * tmppt[1] / tmppt[3])/8+(OLED_HEIGHT/2));
       // int scz = ((65536 * tmppt[2] / tmppt[3]));
        int scd = ((-256 * 2 * m->radius / tmppt[3])/8);
        scd += 3; //Slack
        if( scx < -scd || scy < -scd || scx >= OLED_WIDTH + scd || scy >= OLED_HEIGHT + scd )
        {
            return -1;
        }
        else
        {
            return -tmppt[3];
        }
    }
    else
    {
        return -2;
    }
}

void ICACHE_FLASH_ATTR tdDrawModel( const tdModel * m )
{
    int i;

    int nrv = m->nrvertnums;
    int nri = m->nrfaces*m->indices_per_face;
    int16_t * verticesmark = (int16_t*)&m->indices_and_vertices[nri];

    if( tdModelVisibilitycheck( m ) < 0 )
    {
        return;
    }


    //This looks a little odd, but what we're doing is caching our vertex computations
    //so we don't have to re-compute every time round.
    //f( "%d\n", nrv );
    int16_t cached_verts[nrv];

    for( i = 0; i < nrv; i+=3 )
    {
        int16_t * cv1 = &cached_verts[i];
        if( LocalToScreenspace( &verticesmark[i], cv1, cv1+1 ) )
            cv1[2] = 2;
        else
            cv1[2] = 1;
    }

    if( m->indices_per_face == 2 )
    {
        if( renderlinecolor == BLACK )
        {
            for( i = 0; i < nri; i+=2 )
            {
                int i1 = m->indices_and_vertices[i];
                int i2 = m->indices_and_vertices[i+1];
                int16_t * cv1 = &cached_verts[i1];
                int16_t * cv2 = &cached_verts[i2];

                if( cv1[2] != 2 && cv2[2] != 2 )
                {
                    speedyBlackLine( cv1[0], cv1[1], cv2[0], cv2[1], false );
                }
            }
        }
        else
        {
            for( i = 0; i < nri; i+=2 )
            {
                int i1 = m->indices_and_vertices[i];
                int i2 = m->indices_and_vertices[i+1];
                int16_t * cv1 = &cached_verts[i1];
                int16_t * cv2 = &cached_verts[i2];

                if( cv1[2] != 2 && cv2[2] != 2 )
                {
                    speedyWhiteLine( cv1[0], cv1[1], cv2[0], cv2[1], false );
                }
            }
        }
    }
    else if( m->indices_per_face == 3 )
    {
        for( i = 0; i < nri; i+=3 )
        {
            int i1 = m->indices_and_vertices[i];
            int i2 = m->indices_and_vertices[i+1];
            int i3 = m->indices_and_vertices[i+2];
            int16_t * cv1 = &cached_verts[i1];
            int16_t * cv2 = &cached_verts[i2];
            int16_t * cv3 = &cached_verts[i3];
            //printf( "%d/%d/%d  %d %d %d\n", i1, i2, i3, cv1[2], cv2[2], cv3[2] );

            if( cv1[2] != 2 && cv2[2] != 2 && cv3[2] != 2 )
            {

                //Perform screen-space cross product to determine if we're looking at a backface.
                int Ux = cv3[0] - cv1[0];
                int Uy = cv3[1] - cv1[1];
                int Vx = cv2[0] - cv1[0];
                int Vy = cv2[1] - cv1[1];
                if( Ux*Vy-Uy*Vx >= 0 )
                    outlineTriangle( cv1[0], cv1[1], cv2[0], cv2[1], cv3[0], cv3[1], BLACK, WHITE );
            }
        }
    }
}


struct ModelRangePair
{
    tdModel * model;
    int       mrange;
};

//Do not put this in icache.
int mdlctcmp( const void * va, const void * vb );
int mdlctcmp( const void * va, const void * vb )
{
    struct ModelRangePair * a = (struct ModelRangePair *)va;
    struct ModelRangePair * b = (struct ModelRangePair *)vb;
    return b->mrange - a->mrange;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static bool ICACHE_FLASH_ATTR flightRender(void)
{
    flight_t * tflight = flight;
    if( tflight->mode != FLIGHT_GAME && tflight->mode != FLIGHT_GAME_OVER ) return false;

    // First clear the OLED

    int ij = tflight->frames;
    SetupMatrix();

#ifdef EMU
    uint32_t start = 0;
#else
    // uint32_t start = xthal_get_ccount();
#endif
    if( tflight->type == FL_PERFTEST )
    {
        tdRotateEA( ProjectionMatrix, -20, 0, 0 );
        clearDisplay();
        int x = 0;
        int y = -1;
#ifndef EMU
        PIN_FUNC_SELECT( PERIPHS_IO_MUX_U0TXD_U, 3); //Set to GPIO.  
        GPIO_OUTPUT_SET(GPIO_ID_PIN(1), 0 );
        OVERCLOCK_SECTION_ENABLE();
#endif
        //45 spheres x 120 line segments = 5,400 edges per frame
        //45 spheres x 42 vertices per = 1,890 vertices per frame
        //As of 2020-09-23 19:54, Render is: 20.89584ms + ~9.16ms for output.

        if( !tflight->perfMotion )
        {
            ij = 0;
        }
        else
        {
            tdRotateEA( ModelviewMatrix, ij, 0, 0 );
        }

        for( x = -4; x < 5; x++ )
        {
            for( y = 0; y < 5; y++ )
            {
                //7 * 4 * 120 = 3360 lines per frame.
                ModelviewMatrix[11] = 2600+(tflight->perfMotion?(tdSIN( (x + y)*40 + ij*1 ) )*5:0);
                ModelviewMatrix[3] = 450*x;
                ModelviewMatrix[7] = 500*y+500;
                tdDrawModel( tflight->isosphere );
            }
        }
#ifndef EMU
        OVERCLOCK_SECTION_DISABLE();
        GPIO_OUTPUT_SET(GPIO_ID_PIN(1), 1 ); 
#endif
    }
    else if( tflight->type == FL_TRIANGLES )
    {
#ifndef EMU
        PIN_FUNC_SELECT( PERIPHS_IO_MUX_U0TXD_U, 3); //Set to GPIO.  
        GPIO_OUTPUT_SET(GPIO_ID_PIN(1), 0 );
        OVERCLOCK_SECTION_ENABLE();
#endif
        int x,y;
        int overlay = 0;
        //1,000 triangles @ 28.3ms.
        if( tflight->perfMotion )
        {
            for( overlay = 0; overlay < 100; overlay++ )
            {
                int col = os_random()%2;
                outlineTriangle( (os_random()%256)-64, (os_random()%128)-32, (os_random()%256)-64, (os_random()%128)-32,
                    (os_random()%256)-64, (os_random()%128)-32, col, !col );
            }
        }
        else
        {
            ij = 32;
            for( overlay = 0; overlay < 10; overlay++ )
            for( y = 0; y < 10; y++ )
            for( x = 0; x < 10; x++ )
            {
                int mx = x * 12;
                int my = y * 6;
                int mx1 = x*12+tdSIN( ij+x+y )/25;
                int my1 = y*6+tdCOS( ij+x+y )/25;
                outlineTriangle( mx, my, mx1, my, mx, my1, 0, 1 );
                outlineTriangle( mx, my1, mx1, my1, mx1, my, 0, 1 );
            }
        }
#ifndef EMU
        OVERCLOCK_SECTION_DISABLE();
        GPIO_OUTPUT_SET(GPIO_ID_PIN(1), 1 ); 
#endif
    }
    else if( tflight->type == FL_ENV )
    {

#ifndef EMU
        PIN_FUNC_SELECT( PERIPHS_IO_MUX_U0TXD_U, 3); //Set to GPIO.  
        GPIO_OUTPUT_SET(GPIO_ID_PIN(1), 0 );
        OVERCLOCK_SECTION_ENABLE();
#endif

        clearDisplay();
        tdRotateEA( ProjectionMatrix, tflight->hpr[1]/16, tflight->hpr[0]/16, 0 );
        tdTranslate( ModelviewMatrix, -tflight->planeloc[0], -tflight->planeloc[1], -tflight->planeloc[2] );


        struct ModelRangePair mrp[tflight->enviromodels];
        int mdlct = 0;

/////////////////////////////////////////////////////////////////////////////////////////
////GAME LOGIC GOES HERE (FOR COLLISIONS/////////////////////////////////////////////////

        int i;
        for( i = 0; i < tflight->enviromodels;i++ )
        {
            tdModel * m = tflight->environment[i];

            int label = m->label;
            int draw = 1;
            if( label )
            {
                draw = 0;
                if( label >= 100 && (label - 100) == tflight->ondonut )
                {
                    draw = 1;
                    if( tdDist( tflight->planeloc, m->center ) < 130 )
                    {
                        flightLEDAnimate( FLIGHT_LED_DONUT );
                        tflight->ondonut++;
                    }
                }
                //bean? 1000... groupings of 8.
                int beansec = ((label-1000)/10);

                if( label >= 1000 && ( beansec == tflight->ondonut || beansec == (tflight->ondonut-1) || beansec == (tflight->ondonut+1)) )
                {
                    if( ! (tflight->beangotmask[beansec] & (1<<((label-1000)%10))) )
                    {
                        draw = 1;

                        if( tdDist( tflight->planeloc, m->center ) < 100 )
                        {
                            tflight->beans++;
                            tflight->beangotmask[beansec] |= (1<<((label-1000)%10));
                            flightLEDAnimate( FLIGHT_LED_BEAN );
                        }

                    }
                }
                if( label == 999 ) //gazebo
                {
                    draw = 1;
                    if( flight->mode != FLIGHT_GAME_OVER && tdDist( tflight->planeloc, m->center ) < 200 && tflight->ondonut == 14)
                    {
                        flightLEDAnimate( FLIGHT_LED_ENDING );
                        tflight->frames = 0;
                        tflight->wintime = tflight->timer;
                        tflight->mode = FLIGHT_GAME_OVER;
                    }
                }
            }

            if( draw == 0 ) continue;

            int r = tdModelVisibilitycheck( m );
            if( r < 0 ) continue;
            mrp[mdlct].model = m;
            mrp[mdlct].mrange = r;
            mdlct++;
        }

        //Painter's algorithm
        qsort( mrp, mdlct, sizeof( struct ModelRangePair ), mdlctcmp );

        for( i = 0; i < mdlct; i++ )
        {
            tdModel * m = mrp[i].model;
            int label = m->label;
            int draw = 1;
            if( label )
            {
                draw = 0;
                if( label >= 100 && label < 999 )
                {
                    draw = 2; //All donuts flash on.
                }
                if( label >= 1000 )
                {
                    draw = 3; //All beans flash-invert
                }
                if( label == 999 ) //gazebo
                {
                    draw = (tflight->ondonut==14)?2:1; //flash on last donut.
                }
            }

            //XXX TODO:
            // Flash light when you get a bean or a ring.
            // Do laptiming per ring for fastest time.
            // Fix time counting and presentation

            //draw = 0 = invisible
            //draw = 1 = regular
            //draw = 2 = flashing
            //draw = 3 = other flashing
            if( draw == 1 )
                tdDrawModel( m );
            else if( draw == 2 || draw == 3 )
            {
                if( draw == 2 )
                    renderlinecolor = (tflight->frames&1)?WHITE:BLACK;
                if( draw == 3 ) 
                    renderlinecolor = (tflight->frames&1)?BLACK:WHITE;
                tdDrawModel( m );
                renderlinecolor = WHITE;
            }
        }


#ifndef EMU
        OVERCLOCK_SECTION_DISABLE();
        GPIO_OUTPUT_SET(GPIO_ID_PIN(1), 1 ); 
#endif

    }
    else
    {
        //Normal game
        int x = 0;
        int y = -1;

#ifndef EMU
        PIN_FUNC_SELECT( PERIPHS_IO_MUX_U0TXD_U, 3); //Set to GPIO.  
        GPIO_OUTPUT_SET(GPIO_ID_PIN(1), 0 );
        OVERCLOCK_SECTION_ENABLE();
#endif

        clearDisplay();
        tdRotateEA( ProjectionMatrix, tflight->hpr[1], tflight->hpr[0], 0 );
        tdTranslate( ModelviewMatrix, -tflight->planeloc[0], -tflight->planeloc[1], -tflight->planeloc[2] );

        int16_t BackupMatrix[16];
        for( x = -6; x < 17; x++ )
        {
            for( y = -2; y < 10; y++ )
            {
                ets_memcpy( BackupMatrix, ModelviewMatrix, sizeof( BackupMatrix ) );
                tdTranslate( ModelviewMatrix, 
                    500*x-800,
                    140 + (tdSIN( (x + y)*40 + ij*1 )>>2),
                    500*y+500 );
                tdScale( ModelviewMatrix, 70, 70, 70 );
                tdDrawModel( tflight->isosphere );
                ets_memcpy( ModelviewMatrix, BackupMatrix, sizeof( BackupMatrix ) );
            }
        }
#ifndef EMU
        OVERCLOCK_SECTION_DISABLE();
        GPIO_OUTPUT_SET(GPIO_ID_PIN(1), 1 ); 
#endif
    }
#ifdef EMU
    uint32_t stop = 0;
#else
    // uint32_t stop = xthal_get_ccount();
#endif


    if( flight->mode == FLIGHT_GAME )
    {
        char framesStr[32] = {0};
        //ets_snprintf(framesStr, sizeof(framesStr), "%02x %dus", tflight->buttonState, (stop-start)/160);
        ets_snprintf(framesStr, sizeof(framesStr), "%d %d %d", tflight->ondonut, tflight->beans, tflight->timer );

        plotText(1, 1, framesStr, TOM_THUMB, WHITE);
    }
    else
    {
        char framesStr[32] = {0};
        //ets_snprintf(framesStr, sizeof(framesStr), "%02x %dus", tflight->buttonState, (stop-start)/160);
        ets_snprintf(framesStr, sizeof(framesStr), "YOU  WIN:" );
        plotText(20, 0, framesStr, RADIOSTARS, WHITE);
        ets_snprintf(framesStr, sizeof(framesStr), "TIME:%5d", tflight->wintime );
        plotText(20, 20, framesStr, RADIOSTARS, WHITE);
        ets_snprintf(framesStr, sizeof(framesStr), "BEANS:%3d",tflight->beans );
        plotText(20, 40, framesStr, RADIOSTARS, WHITE);
    }

    //If perf test, force full frame refresh
    //Otherwise, don't force full-screen refresh
    return tflight->type == FL_PERFTEST; 
}

static void ICACHE_FLASH_ATTR flightGameUpdate( flight_t * tflight )
{
    uint8_t bs = tflight->buttonState;

    int dpitch = 0;
    int dyaw = 0;

    const int thruster_accel = 4;
    const int thruster_max = 40; //NOTE: thruster_max must be divisble by thruster_accel
    const int thruster_decay = 4;
    const int FLIGHT_SPEED_DEC = 10;
    const int flight_max_speed = 50;
    const int flight_min_speed = 10;

    //If we're at the ending screen and the user presses a button end game.
    if( tflight->mode == FLIGHT_GAME_OVER && ( bs & 16 ) && flight->frames > 199 )
	{
		flightEndGame();
	}

    if( tflight->mode == FLIGHT_GAME )
    {
        if( bs & 1 ) dpitch += thruster_accel;
        if( bs & 4 ) dpitch -= thruster_accel;
        if( bs & 2 ) dyaw += thruster_accel;
        if( bs & 8 ) dyaw -= thruster_accel;

        if( dpitch )
        {
            tflight->pitchmoment += dpitch;
            if( tflight->pitchmoment > thruster_max ) tflight->pitchmoment = thruster_max;
            if( tflight->pitchmoment < -thruster_max ) tflight->pitchmoment = -thruster_max;
        }
        else
        {
            if( tflight->pitchmoment > 0 ) tflight->pitchmoment-=thruster_decay;
            if( tflight->pitchmoment < 0 ) tflight->pitchmoment+=thruster_decay;
        }

        if( dyaw )
        {
            tflight->yawmoment += dyaw;
            if( tflight->yawmoment > thruster_max ) tflight->yawmoment = thruster_max;
            if( tflight->yawmoment < -thruster_max ) tflight->yawmoment = -thruster_max;
        }
        else
        {
            if( tflight->yawmoment > 0 ) tflight->yawmoment-=thruster_decay;
            if( tflight->yawmoment < 0 ) tflight->yawmoment+=thruster_decay;
        }

        tflight->hpr[0] += tflight->pitchmoment;
        tflight->hpr[1] += tflight->yawmoment;


        if( bs & 16 ) tflight->speed++;
        else tflight->speed--;
		if( tflight->speed < flight_min_speed ) tflight->speed = flight_min_speed;
	    if( tflight->speed > flight_max_speed ) tflight->speed = flight_max_speed;
    }

    //If game over, just keep status quo.

    tflight->planeloc[0] += (tflight->speed * tdSIN( tflight->hpr[0]/16 ) )>>FLIGHT_SPEED_DEC;
    tflight->planeloc[2] += (tflight->speed * tdCOS( tflight->hpr[0]/16 ) )>>FLIGHT_SPEED_DEC;
    tflight->planeloc[1] -= (tflight->speed * tdSIN( tflight->hpr[1]/16 ) )>>FLIGHT_SPEED_DEC;

    flight->timer++;
}

/**
 *
 * @param state  A bitmask of all button states, unused
 * @param button The button which triggered this event
 * @param down   true if the button was pressed, false if it was released
 */
void ICACHE_FLASH_ATTR flightButtonCallback( uint8_t state,
        int button, int down )
{
    switch (flight->mode)
    {
        default:
        case FLIGHT_MENU:
        {
            if(down)
            {
                flightLEDAnimate( FLIGHT_LED_MENU_TICK ); 
                menuButton(flight->menu, button);
            }
            break;
        }
        case FLIGHT_GAME_OVER:
        case FLIGHT_GAME:
        {
            if( (flight->type == FL_TRIANGLES || flight->type == FL_PERFTEST) && button == 4 && down ) flight->perfMotion = !flight->perfMotion;

            flight->buttonState = state;
            break;
        }
    }
}


