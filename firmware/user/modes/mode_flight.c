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
#include <user_interface.h>
#include "text_entry.h"

#include "nvm_interface.h"
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
#include "menu_strings.h"

#include "embeddednf.h"
#include "embeddedout.h"

/*============================================================================
 * Defines, Structs, Enums
 *==========================================================================*/

#define FLIGHT_UPDATE_MS 33

//XXX TODO: Refactor - these should probably be unified.
#define MAXRINGS 15
#define MAX_DONUTS 14
#define MAX_BEANS 69


typedef enum
{
    FLIGHT_MENU,
    FLIGHT_GAME,
    FLIGHT_GAME_OVER,
    FLIGHT_HIGH_SCORE_ENTRY,
    FLIGHT_SHOW_HIGH_SCORES,
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
    int frames, tframes;
    uint8_t buttonState;

    int16_t planeloc[3];
    int16_t hpr[3];
    int16_t speed;
    int16_t pitchmoment;
    int16_t yawmoment;
    bool perfMotion;
    bool oob;

    int enviromodels;
    tdModel ** environment;

    menu_t* menu;
    linkedInfo_t* invYmnu;

    int beans;
    int ondonut;
    uint32_t timeOfStart;
    uint32_t timeGot100Percent;
    int wintime;
    bool inverty;

    flLEDAnimation ledAnimation;
    uint8_t        ledAnimationTime;

    char highScoreNameBuffer[FLIGHT_HIGH_SCORE_NAME_LEN+1];

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
static void ICACHE_FLASH_ATTR flightStartGame(void);
static bool ICACHE_FLASH_ATTR flightRender(void);
static void ICACHE_FLASH_ATTR flightGameUpdate( flight_t * tflight );
static void ICACHE_FLASH_ATTR flightUpdateLEDs(flight_t * tflight);
static void ICACHE_FLASH_ATTR flightLEDAnimate( flLEDAnimation anim );
int ICACHE_FLASH_ATTR tdModelVisibilitycheck( const tdModel * m );
void ICACHE_FLASH_ATTR tdDrawModel( const tdModel * m );
static int ICACHE_FLASH_ATTR flightTimeHighScorePlace( int wintime, bool is100percent );
static void ICACHE_FLASH_ATTR flightTimeHighScoreInsert( int insertplace, bool is100percent, char * name, int timeCentiseconds );

void iplotRectB( int x1, int y1, int x2, int y2 );

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
static const char fl_flight_env[] = "Take Flight";
static const char fl_flight_invertY0_env[] = "Y NOT INVERTED";
static const char fl_flight_invertY1_env[] = "Y INVERTED";

/*============================================================================
 * Functions
 *==========================================================================*/

void iplotRectB( int x1, int y1, int x2, int y2 )
{
    int x;
    for( ; y1 < y2; y1++ )
    for( x = x1; x < x2; x++ )
    {
        drawPixelUnsafeBlack( x, y1 );
    }
}

/**
 * Initializer for flight
 */
void ICACHE_FLASH_ATTR flightEnterMode(void)
{
    // Alloc and clear everything
    flight = os_malloc(sizeof(flight_t));
    ets_memset(flight, 0, sizeof(flight_t));

    flight->mode = FLIGHT_MENU;

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

    flight->menu = initMenu(fl_title, flightMenuCb);
    addRowToMenu(flight->menu);
    // addItemToRow(flight->menu, fl_flight_perf);
    // addItemToRow(flight->menu, fl_flight_triangles);
    addItemToRow(flight->menu, fl_flight_env);
    addRowToMenu(flight->menu);
    addItemToRow(flight->menu, str_quit);

    addRowToMenu(flight->menu);
    flight->invYmnu = addItemToRow(flight->menu,
        getFlightSaveData()->flightInvertY?
            fl_flight_invertY1_env:
            fl_flight_invertY0_env );

    addRowToMenu(flight->menu);
    addItemToRow(flight->menu, str_high_scores );

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
        flightStartGame();
    }
    else if ( fl_flight_invertY0_env == menuItem )
    {
        flightSimSaveData_t * sd = getFlightSaveData();
        sd->flightInvertY = 1;
        setFlightSaveData( sd );
        flight->invYmnu->item.name = fl_flight_invertY1_env;
    }
    else if ( fl_flight_invertY1_env == menuItem )
    {
        flightSimSaveData_t * sd = getFlightSaveData();
        sd->flightInvertY = 0;
        setFlightSaveData( sd );
        flight->invYmnu->item.name = fl_flight_invertY0_env;
    }
    else if ( str_high_scores == menuItem )
    {
        flight->mode = FLIGHT_SHOW_HIGH_SCORES;
    }
    else if (str_quit == menuItem)
    {
        switchToSwadgeMode(0);
    }
}

static void ICACHE_FLASH_ATTR flightEndGame()
{
    if( flightTimeHighScorePlace( flight->wintime, flight->beans == MAX_BEANS ) < NUM_FLIGHTSIM_TOP_SCORES )
    {
        flight->mode = FLIGHT_HIGH_SCORE_ENTRY;
        textEntryStart( FLIGHT_HIGH_SCORE_NAME_LEN+1, flight->highScoreNameBuffer );
    }
    else
    {
        flight->mode = FLIGHT_MENU;
    }
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
static void ICACHE_FLASH_ATTR flightStartGame(void)
{
    flight->mode = FLIGHT_GAME;
    flight->frames = 0;


    flight->ondonut = 0; //Set to 14 to b-line it to the end for testing.
    flight->beans = 0; //Set to MAX_BEANS for 100% instant.
    flight->timeOfStart = system_get_time();//-1000000*190; (Do this to force extra coursetime)
    flight->timeGot100Percent = 0;
    flight->wintime = 0;
    flight->speed = 0;

    //Starting location/orientation
    flight->planeloc[0] = 24*48;
    flight->planeloc[1] = 18*48; //Start pos * 48 since 48 is the fixed scale.
    flight->planeloc[2] = 60*48;
    flight->hpr[0] = 2061;
    flight->hpr[1] = 190;
    flight->hpr[2] = 0;
    flight->pitchmoment = 0;
    flight->yawmoment = 0;

    flight->inverty = getFlightSaveData()->flightInvertY;

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
    static const char * EnglishNumberSuffix[] = { "st", "nd", "rd", "th" };
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
        case FLIGHT_SHOW_HIGH_SCORES:
        {
            clearDisplay();

            char buffer[32];
            flightSimSaveData_t * sd = getFlightSaveData();
            int line;

            plotText( 20, 1, "ANY %", IBM_VGA_8, WHITE );
            plotText( 84, 1, "100 %", IBM_VGA_8, WHITE );

            for( line = 0; line < NUM_FLIGHTSIM_TOP_SCORES; line++ )
            {
                int anyp = 0;
                ets_snprintf( buffer, sizeof(buffer), "%d%s", line+1, EnglishNumberSuffix[line] );
                plotText( 3, (line+1)*10+10, buffer, TOM_THUMB, WHITE );

                for( anyp = 0; anyp < 2; anyp++ )
                {
                    int cs = sd->timeCentiseconds[line+anyp*NUM_FLIGHTSIM_TOP_SCORES];
                    char * name = sd->displayName[line+anyp*NUM_FLIGHTSIM_TOP_SCORES];
                    char namebuff[FLIGHT_HIGH_SCORE_NAME_LEN+1];    //Force pad of null.
                    memcpy( namebuff, name, FLIGHT_HIGH_SCORE_NAME_LEN );
                    namebuff[FLIGHT_HIGH_SCORE_NAME_LEN] = 0;
                    ets_snprintf( buffer, sizeof(buffer), "%4s %3d.%02d", namebuff, cs/100,cs%100 );
                    plotText( anyp?81:17, (line+1)*10+10, buffer, TOM_THUMB, WHITE );
                }
            }
            break;
        }
        case FLIGHT_HIGH_SCORE_ENTRY:
        {
            int place = flightTimeHighScorePlace( flight->wintime, flight->beans >= MAX_BEANS );
            textEntryDraw();

            char placeStr[32] = {0};
            ets_snprintf(placeStr, sizeof(placeStr), "%d%s %s", place + 1, EnglishNumberSuffix[place],
                (flight->beans == MAX_BEANS)?"100%":"ANY%" );
            plotText(65,2, placeStr, IBM_VGA_8, WHITE);
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
    tflight->tframes++;
    if( tflight->mode != FLIGHT_GAME && tflight->mode != FLIGHT_GAME_OVER ) return false;

    // First clear the OLED

    SetupMatrix();

#ifdef EMU
    uint32_t start = 0;
#else
    // uint32_t start = xthal_get_ccount();
#endif

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
                if( flight->mode != FLIGHT_GAME_OVER && tdDist( tflight->planeloc, m->center ) < 200 && tflight->ondonut == MAX_DONUTS)
                {
                    flightLEDAnimate( FLIGHT_LED_ENDING );
                    tflight->frames = 0;
                    tflight->wintime = (system_get_time() - tflight->timeOfStart)/10000;
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
                draw = (tflight->ondonut==MAX_DONUTS)?2:1; //flash on last donut.
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
#ifdef EMU
    uint32_t stop = 0;
#else
    // uint32_t stop = xthal_get_ccount();
#endif


    if( flight->mode == FLIGHT_GAME )
    {
        char framesStr[32] = {0};
        //ets_snprintf(framesStr, sizeof(framesStr), "%02x %dus", tflight->buttonState, (stop-start)/160);
        int elapsed = (system_get_time()-tflight->timeOfStart)/10000;
        ets_snprintf(framesStr, sizeof(framesStr), "%d/%d, %d", tflight->ondonut, MAX_DONUTS, tflight->beans );
        int16_t width = textWidth(framesStr, TOM_THUMB);
        iplotRectB(0, 0, width, FONT_HEIGHT_TOMTHUMB + 1);
        plotText(0, 0, framesStr, TOM_THUMB, WHITE);

        ets_snprintf(framesStr, sizeof(framesStr), "%d.%02d", elapsed/100, elapsed%100 );
        width = textWidth(framesStr, TOM_THUMB);
        iplotRectB(OLED_WIDTH - width, 0, OLED_WIDTH, FONT_HEIGHT_TOMTHUMB + 1);
        plotText(OLED_WIDTH - width + 1, 0, framesStr, TOM_THUMB, WHITE);

        ets_snprintf(framesStr, sizeof(framesStr), "%d", tflight->speed);
        width = textWidth(framesStr, TOM_THUMB);
        iplotRectB(OLED_WIDTH - width, OLED_HEIGHT - FONT_HEIGHT_TOMTHUMB - 1, OLED_WIDTH, OLED_HEIGHT);
        plotText(OLED_WIDTH - width + 1, OLED_HEIGHT - FONT_HEIGHT_TOMTHUMB, framesStr, TOM_THUMB, WHITE);

        if(flight->oob)
        {
            width = textWidth("TURN AROUND", IBM_VGA_8);
            plotText((OLED_WIDTH - width) / 2, (OLED_HEIGHT - FONT_HEIGHT_IBMVGA8) / 2, "TURN AROUND", IBM_VGA_8, WHITE);
        }
    }
    else
    {
        char framesStr[32] = {0};
        //ets_snprintf(framesStr, sizeof(framesStr), "%02x %dus", tflight->buttonState, (stop-start)/160);
        ets_snprintf(framesStr, sizeof(framesStr), "YOU  WIN:" );
        plotText(20, 0, framesStr, RADIOSTARS, WHITE);
        ets_snprintf(framesStr, sizeof(framesStr), "TIME:%d.%02d", tflight->wintime/100,tflight->wintime%100 );
        plotText((tflight->wintime>10000)?14:20, 18, framesStr, RADIOSTARS, WHITE);
        ets_snprintf(framesStr, sizeof(framesStr), "BEANS:%2d",tflight->beans );
        plotText(20, 36, framesStr, RADIOSTARS, WHITE);
    }

    if( tflight->beans >= MAX_BEANS )
    {
        if( tflight->timeGot100Percent == 0 )
            tflight->timeGot100Percent = (system_get_time() - tflight->timeOfStart);

        int crazy = ((system_get_time() - tflight->timeOfStart)-tflight->timeGot100Percent) < 3000000;
        plotText(10, 52, "100% 100% 100%", IBM_VGA_8, crazy?( tflight->tframes & 1)?WHITE:BLACK:WHITE );
    }

    //If perf test, force full frame refresh
    //Otherwise, don't force full-screen refresh
    return false;
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
    const int flight_max_speed = 150;
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

        if( tflight->inverty ) dyaw *= -1;

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

    // Bound the area
    tflight->oob = false;
    if(tflight->planeloc[0] < -1900)
    {
        tflight->planeloc[0] = -1900;
        tflight->oob = true;
    }
    else if(tflight->planeloc[0] > 1900)
    {
        tflight->planeloc[0] = 1900;
        tflight->oob = true;
    }

    if(tflight->planeloc[1] < -800)
    {
        tflight->planeloc[1] = -800;
        tflight->oob = true;
    }
    else if(tflight->planeloc[1] > 3500)
    {
        tflight->planeloc[1] = 3500;
        tflight->oob = true;
    }

    if(tflight->planeloc[2] < -1300)
    {
        tflight->planeloc[2] = -1300;
        tflight->oob = true;
    }
    else if(tflight->planeloc[2] > 3700)
    {
        tflight->planeloc[2] = 3700;
        tflight->oob = true;
    }
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
        case FLIGHT_SHOW_HIGH_SCORES:
        {
            if( down )
            {
                //Return to main mode.
                flight->mode = FLIGHT_MENU;
            }
            break;
        }
        case FLIGHT_HIGH_SCORE_ENTRY:
        {
            if( !textEntryInput( down, button ) )
            {
                //Actually insert high score.
                textEntryEnd();
                if( strlen( flight->highScoreNameBuffer ) )
                {
                    flightTimeHighScoreInsert(
                        flightTimeHighScorePlace( flight->wintime, flight->beans == MAX_BEANS ),
                        flight->beans == MAX_BEANS,
                        flight->highScoreNameBuffer,
                        flight->wintime );
                }
                flight->mode = FLIGHT_SHOW_HIGH_SCORES;
            }
            break;
        }
        case FLIGHT_GAME_OVER:
        case FLIGHT_GAME:
        {
            flight->buttonState = state;
            break;
        }
    }
}




//Handling high scores.
/**
 *
 * @param wintime in centiseconds
 * @param whether this is a 100% run.
 * @return place in top score list.  If 4, means not in top score list.
 *
 */
static int ICACHE_FLASH_ATTR flightTimeHighScorePlace( int wintime, bool is100percent )
{
    flightSimSaveData_t * sd = getFlightSaveData();
    int i;
    for( i = 0; i < NUM_FLIGHTSIM_TOP_SCORES; i++ )
    {
        int cs = sd->timeCentiseconds[i+is100percent*NUM_FLIGHTSIM_TOP_SCORES];
        if( !cs || cs > wintime ) break;
    }
    return i;
}

/**
 *
 * @param which winning slot to place player into
 * @param whether this is a 100% run.
 * @param display name for player (truncated to
 * @param wintime in centiseconds
 *
 */
static void ICACHE_FLASH_ATTR flightTimeHighScoreInsert( int insertplace, bool is100percent, char * name, int timeCentiseconds )
{
    if( insertplace >= NUM_FLIGHTSIM_TOP_SCORES || insertplace < 0 ) return;

    flightSimSaveData_t * sd = getFlightSaveData();
    int i;
    for( i = NUM_FLIGHTSIM_TOP_SCORES-1; i > insertplace; i-- )
    {
        memcpy( sd->displayName[i+is100percent*NUM_FLIGHTSIM_TOP_SCORES],
            sd->displayName[(i-1)+is100percent*NUM_FLIGHTSIM_TOP_SCORES],
            NUM_FLIGHTSIM_TOP_SCORES );
        sd->timeCentiseconds[i+is100percent*NUM_FLIGHTSIM_TOP_SCORES] =
            sd->timeCentiseconds[i-1+is100percent*NUM_FLIGHTSIM_TOP_SCORES];
    }
    int namelen = strlen( name );
    if( namelen > FLIGHT_HIGH_SCORE_NAME_LEN ) namelen = FLIGHT_HIGH_SCORE_NAME_LEN;
    memcpy( sd->displayName[insertplace+is100percent*NUM_FLIGHTSIM_TOP_SCORES],
        name, namelen );

    //Zero pad if less than 4 chars.
    if( namelen < FLIGHT_HIGH_SCORE_NAME_LEN )
        sd->displayName[insertplace+is100percent*NUM_FLIGHTSIM_TOP_SCORES][namelen] = 0;

    sd->timeCentiseconds[insertplace+is100percent*NUM_FLIGHTSIM_TOP_SCORES] = timeCentiseconds;
    setFlightSaveData( sd );
}

