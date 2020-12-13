/*
 * mode_rssi.c
 *
 *  Created on: Mar 27, 2019
 *      Author: adam
 */
#include "user_config.h"

/*============================================================================
 * Includes
 *==========================================================================*/

#include <osapi.h>
#include <mem.h>
#include <stdint.h>
#include <sntp.h>
#include <time.h>

#include "user_main.h"
#include "embeddednf.h"
#include "oled.h"
#include "bresenham.h"
#include "cndraw.h"
#include "assets.h"
#include "buttons.h"
#include "menu2d.h"
#include "linked_list.h"
#include "font.h"
#include "text_entry.h"
#include "hsv_utils.h"
#include "ip_addr.h"
#include "user_interface.h"
#include "printControl.h"
#include "nvm_interface.h"

/*============================================================================
 * Defines, Structs, Enums
 *==========================================================================*/

#define RSSI_UPDATE_MS 20

typedef enum
{
    RSSI_MENU,
    RSSI_PASSWORD_ENTER,
    RSSI_SCAN,
    RSSI_STATION,
    RSSI_SOFTAP,
} rssiModeScreen;

typedef enum
{
    RSSI_SUBMODE_REGULAR,
    RSSI_SUBMODE_ALTERNATE,
    RSSI_SUBMODE_ALTERNATE2,
    RSSI_SUBMODE_CLOCK,
    RSSI_SUBMODE_MAX,
} rssiSubmode;

#define MAX_SCAN 20

typedef struct
{
    char ssid[SSID_NAME_LEN];
    AUTH_MODE auth;
    sint8 rssi;
    uint8 channel;
} accessPoint_t;

typedef struct
{
    rssiModeScreen mode;
    timer_t updateTimer;
    uint8_t buttonState;
    rssiSubmode submode;
    menu_t* menu;

    accessPoint_t aps [MAX_SCAN];
    char    password[SSID_NAME_LEN];
    char    connectssid[SSID_NAME_LEN];
    int8_t  rssi_history[128];
    int     rssi_head;
    int num_scan;

    bool sntpInit; // time init
    int8_t tz; // timezone
} rssi_t;

#define ABS(X) (((X) < 0) ? -(X) : (X))

/*============================================================================
 * Prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR rssiEnterMode(void);
void ICACHE_FLASH_ATTR rssiExitMode(void);
void ICACHE_FLASH_ATTR rssiButtonCallback(uint8_t state __attribute__((unused)),
        int button, int down);

static void ICACHE_FLASH_ATTR rssiUpdate(void* arg __attribute__((unused)));
static void ICACHE_FLASH_ATTR rssiMenuCb(const char* menuItem);
static void ICACHE_FLASH_ATTR rssi_scan_done_cb(void* arg, STATUS status);
static void ICACHE_FLASH_ATTR rssiSetupMenu(void);

void ICACHE_FLASH_ATTR strDow(char* str, int dow);
void ICACHE_FLASH_ATTR strMon(char* str, int mDay, int mon);
void ICACHE_FLASH_ATTR strTime(char* str, int h, int m, int s, bool space);
void ICACHE_FLASH_ATTR drawClockArm(led_t* leds, uint8_t hue, int16_t handAngle);

// Prototype missing from sntp.h
struct tm* sntp_localtime(const time_t* tim_p);

/*============================================================================
 * Variables
 *==========================================================================*/

swadgeMode rssiMode =
{
    .modeName = "rssi",
    .fnEnterMode = rssiEnterMode,
    .fnExitMode = rssiExitMode,
    .fnButtonCallback = rssiButtonCallback,
    .wifiMode = WIFI_REGULAR,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = NULL,
    .fnAudioCallback = NULL,
    .menuImg = "rssi-menu.gif"
};

rssi_t* rssi;

static const char fl_title[]  = "Rssi";
static const char fl_station[]   = "Station";
static const char fl_scan[]   = "Scan";
static const char fl_softap[]   = "Soft AP";
static const char fl_connlast[]   = "Connect Last";
static const char fl_quit[]   = "QUIT";

/*============================================================================
 * Functions
 *==========================================================================*/

static void ICACHE_FLASH_ATTR rssiSetupMenu(void)
{
    if( rssi->menu )
    {
        deinitMenu(rssi->menu);
    }
    rssi->menu = initMenu(fl_title, rssiMenuCb);
    addRowToMenu(rssi->menu);
    addItemToRow(rssi->menu, fl_scan);
    int i;
    for( i = 0; i < rssi->num_scan; i++ )
    {
        addRowToMenu(rssi->menu);
        addItemToRow(rssi->menu, rssi->aps[i].ssid);
    }

    addRowToMenu(rssi->menu);
    addItemToRow(rssi->menu, fl_connlast);

    addRowToMenu(rssi->menu);
    addItemToRow(rssi->menu, fl_softap);
    addRowToMenu(rssi->menu);
    addItemToRow(rssi->menu, fl_quit);
    drawMenu(rssi->menu);
}

/**
 * Initializer for RSSI
 */
void ICACHE_FLASH_ATTR rssiEnterMode(void)
{
    // Alloc and clear everything
    rssi = os_malloc(sizeof(rssi_t));
    ets_memset(rssi, 0, sizeof(rssi_t));
    rssi->mode = RSSI_MENU;

    rssiSetupMenu();

    wifi_set_sleep_type(NONE_SLEEP_T);

    timerDisarm(&(rssi->updateTimer));
    timerSetFn(&(rssi->updateTimer), rssiUpdate, NULL);
    timerArm(&(rssi->updateTimer), RSSI_UPDATE_MS, true);
    enableDebounce(false);

    rssi->sntpInit = false;
    rssi->tz = -5; // eastern US
}

/**
 * Called when rssi is exited
 */
void ICACHE_FLASH_ATTR rssiExitMode(void)
{
    timerDisarm(&(rssi->updateTimer));
    timerFlush();
    deinitMenu(rssi->menu);
    os_free(rssi);
    rssi = 0;

    led_t leds[NUM_LIN_LEDS] = {{0}};
    setLeds(leds, sizeof(leds));
}


static void ICACHE_FLASH_ATTR rssi_scan_done_cb(void* bss_struct, STATUS status)
{
    if (status == OK)
    {
        RSSI_PRINTF("Scan OK [%u]\n", status);
        struct bss_info* bss = (struct bss_info*) bss_struct;
        int i = 0;
        while (bss != 0 && i < MAX_SCAN)
        {
            if(bss->ssid_len > 0)
            {
                // Copy the SSID
                ets_strncpy(rssi->aps[i].ssid, (char*)bss->ssid, sizeof(rssi->aps[i].ssid) - 1);
                // Save the other info
                rssi->aps[i].auth = bss->authmode;
                rssi->aps[i].channel = bss->channel;
                rssi->aps[i].rssi = bss->rssi;

                RSSI_PRINTF("%s\n  bssid: " MACSTR
                            "\n  ssid_len: %d\n  channel: %d\n  rssi: %d\n  authmode: %d\n  is_hidden: %d\n  freq_offset: %d\n  freqcal_val: %d\n  *esp_mesh_ie: %p\n  simple_pair: %d\n  pairwise_cipher: %d\n  group_cipher: %d\n  phy_11b: %d\n  phy_11g: %d\n  phy_11n: %d\n  wps: %d\n  reserved: %d\n",
                            bss->ssid,
                            MAC2STR(bss->bssid),
                            bss->ssid_len,
                            bss->channel,
                            bss->rssi,
                            bss->authmode,
                            bss->is_hidden,
                            bss->freq_offset,
                            bss->freqcal_val,
                            bss->esp_mesh_ie,
                            bss->simple_pair,
                            bss->pairwise_cipher,
                            bss->group_cipher,
                            bss->phy_11b,
                            bss->phy_11g,
                            bss->phy_11n,
                            bss->wps,
                            bss->reserved);
                i++;
            }
            bss = STAILQ_NEXT(bss, next);
        }
        rssi->num_scan = i;
        rssi->mode = RSSI_MENU;
        rssiSetupMenu();
    }
    else
    {
        RSSI_PRINTF( "Scan Fail\n" );
    }
}

static void ICACHE_FLASH_ATTR to_scan();
static void ICACHE_FLASH_ATTR to_scan()
{
    struct scan_config sc;
    sc.ssid = 0;
    sc.bssid = 0;
    sc.channel = 0;
    sc.show_hidden = 1;
    sc.scan_type = WIFI_SCAN_TYPE_ACTIVE;
    sc.scan_time.active.min = 100;
    sc.scan_time.active.max = 1500;
    sc.scan_time.passive = 1500;
    wifi_station_scan( &sc, rssi_scan_done_cb);
    rssi->num_scan = -1;
}


static void ICACHE_FLASH_ATTR rssiMenuCb(const char* menuItem)
{
    //if(fl_station == menuItem)
    if(fl_scan == menuItem)
    {
        wifi_set_sleep_type(NONE_SLEEP_T);
        wifi_set_opmode( STATION_MODE );
        wifi_set_opmode_current( STATION_MODE );
        rssi->mode = RSSI_SCAN;
        to_scan();
    }
    else if(fl_softap == menuItem)
    {
        rssi->mode = RSSI_SOFTAP;
        rssi->submode = 0;
        wifi_set_opmode_current( SOFTAP_MODE );
        wifi_set_sleep_type(NONE_SLEEP_T);
        wifi_enable_signaling_measurement();
    }
    else if( fl_connlast == menuItem)
    {
        // Get last used params
        getSsidPw(rssi->connectssid, rssi->password);

        // Set wifi mode
        wifi_set_opmode_current( STATION_MODE );

        // Connect
        struct station_config sc;
        RSSI_PRINTF( "Connecting to \"%s\" password \"%s\"\n", rssi->connectssid, rssi->password );
        ets_memset( (char*)&sc, 0, sizeof(sc) );
        ets_strcpy( (char*)sc.password, rssi->password );
        ets_strcpy( (char*)sc.ssid, rssi->connectssid);
        sc.all_channel_scan = 1;
        wifi_station_set_config( &sc );
        wifi_station_connect();
        wifi_set_sleep_type(NONE_SLEEP_T);

        rssi->mode = RSSI_STATION;
    }
    else if (fl_quit == menuItem)
    {
        switchToSwadgeMode(0);
    }
    else
    {
        for(int16_t i = 0; i < MAX_SCAN; i++)
        {
            if(rssi->aps[i].ssid == menuItem)
            {
                textEntryStart( 32, rssi->password );
                ets_strcpy( rssi->connectssid, menuItem);
                rssi->mode = RSSI_PASSWORD_ENTER;
                break;
            }
        }
    }
}

/**
 * @brief called on a timer, updates the game state
 *
 * @param arg
 */
static void ICACHE_FLASH_ATTR rssiUpdate(void* arg __attribute__((unused)))
{
    char cts[128];

    switch(rssi->mode)
    {
        default:
        case RSSI_MENU:
        {
            drawMenu(rssi->menu);
            break;
        }
        case RSSI_PASSWORD_ENTER:
        {
            textEntryDraw();
            break;
        }
        case RSSI_SCAN:
        {
            clearDisplay();

            // Keep a rough timer in ms
            static uint32_t dotCnt = 0;
            dotCnt += RSSI_UPDATE_MS;
            if(dotCnt / 400 > 3)
            {
                dotCnt = 0;
            }

            // Start with this message
            char msg[] = "Scanning...";
            // Truncate some dots
            msg[8 + (dotCnt / 400)] = '\0';
            // Plot it
            plotText(16, (OLED_HEIGHT - FONT_HEIGHT_IBMVGA8) / 2, msg, IBM_VGA_8, WHITE);
            break;
        }
        case RSSI_SOFTAP:
        case RSSI_STATION:
        {
            switch( rssi->submode )
            {
                default:
                case RSSI_SUBMODE_MAX:
                case RSSI_SUBMODE_REGULAR:
                case RSSI_SUBMODE_CLOCK:
                {
                    // First clear the OLED
                    clearDisplay();

                    // helpers for drawing
                    int16_t textY = 0;
                    bool timePlotted = false;

#define VERT_SPACING 5

                    // If this is regular, not clock
                    if(RSSI_SUBMODE_REGULAR == rssi->submode)
                    {
                        // Plot the RSSI
                        os_sprintf( cts, "RSSI:%d", wifi_station_get_rssi());
                        plotText(0, textY, cts, IBM_VGA_8, WHITE);
                        textY += (FONT_HEIGHT_IBMVGA8 + VERT_SPACING);
                    }

                    // Get the wifi info
                    struct ip_info ipi;
                    if(wifi_get_ip_info( (rssi->mode == RSSI_SOFTAP) ? SOFTAP_IF : STATION_IF, &ipi))
                    {
                        // If this is the normal mode
                        if(RSSI_SUBMODE_REGULAR == rssi->submode)
                        {
                            // Print all the wifi info
                            os_sprintf( cts, " IP %d.%d.%d.%d", IP2STR( &ipi.ip ) );
                            plotText(0, textY, cts, IBM_VGA_8, WHITE);
                            textY += (FONT_HEIGHT_IBMVGA8 + VERT_SPACING);

                            os_sprintf( cts, " NM %d.%d.%d.%d", IP2STR( &ipi.netmask ) );
                            plotText(0, textY, cts, IBM_VGA_8, WHITE);
                            textY += (FONT_HEIGHT_IBMVGA8 + VERT_SPACING);

                            os_sprintf( cts, " GW %d.%d.%d.%d", IP2STR( &ipi.gw ) );
                            plotText(0, textY, cts, IBM_VGA_8, WHITE);
                            textY += (FONT_HEIGHT_IBMVGA8 + VERT_SPACING);
                        }

                        // Start NTP if we have an IP address and it hasn't been started yet
                        if(ipi.ip.addr != 0 && false == rssi->sntpInit)
                        {
                            RSSI_PRINTF("Init SNTP\n");
                            rssi->sntpInit = true;
                            sntp_setservername(0, "time.google.com");
                            sntp_setservername(1, "time.cloudflare.com");
                            sntp_setservername(2, "time-a-g.nist.gov");
                            sntp_init();
                            sntp_set_timezone(rssi->tz); // Eastern
                        }
                        else if(rssi->sntpInit)
                        {
                            // if NTP is initialized, try to get a timestamp
                            time_t ts = sntp_get_current_timestamp();
                            if (0 != ts)
                            {
                                // Get the time struct
                                struct tm* tStruct = sntp_localtime(&ts);
                                // Print the hour, minute and second to a string
                                strTime(cts, tStruct->tm_hour, tStruct->tm_min, tStruct->tm_sec, (rssi->submode == RSSI_SUBMODE_CLOCK));

                                if(RSSI_SUBMODE_REGULAR == rssi->submode)
                                {
                                    // Plot the text in the upper right corner
                                    int16_t width = textWidth(cts, IBM_VGA_8);
                                    plotText(OLED_WIDTH - width, 0, cts, IBM_VGA_8, WHITE);
                                }
                                else
                                {
                                    // If this is clock mode, plot the entire clock
                                    textY = 4;

                                    int16_t width = textWidth(cts, RADIOSTARS);
                                    plotText((OLED_WIDTH - width) / 2, textY, cts, RADIOSTARS, WHITE);
                                    textY += FONT_HEIGHT_RADIOSTARS + VERT_SPACING;

                                    strDow(cts, tStruct->tm_wday);
                                    width = textWidth(cts, IBM_VGA_8);
                                    plotText((OLED_WIDTH - width) / 2, textY, cts, IBM_VGA_8, WHITE);
                                    textY += FONT_HEIGHT_IBMVGA8 + VERT_SPACING;

                                    strMon(cts, tStruct->tm_mday, tStruct->tm_mon);
                                    width = textWidth(cts, IBM_VGA_8);
                                    plotText((OLED_WIDTH - width) / 2, textY, cts, IBM_VGA_8, WHITE);
                                    textY += FONT_HEIGHT_IBMVGA8 + VERT_SPACING;

                                    os_sprintf(cts, "%d", 1900 + tStruct->tm_year);
                                    width = textWidth(cts, IBM_VGA_8);
                                    plotText((OLED_WIDTH - width) / 2, textY, cts, IBM_VGA_8, WHITE);
                                    textY += FONT_HEIGHT_IBMVGA8 + VERT_SPACING;

                                    timePlotted = true;

                                    // Draw an analog clock to the LEDs
                                    led_t leds[NUM_LIN_LEDS] = {{0}};
                                    drawClockArm(leds, 0,   tStruct->tm_sec * 6);
                                    drawClockArm(leds, 85,  tStruct->tm_min * 6);
                                    drawClockArm(leds, 170, (tStruct->tm_hour % 12) * 30);
                                    setLeds(leds, sizeof(leds));
                                }
                            }
                        }
                    }

                    // If this is clock mode, and no time was printed
                    if(RSSI_SUBMODE_CLOCK == rssi->submode && false == timePlotted)
                    {
                        // Let the user know
                        const char* noTime = "No Time";
                        int16_t width = textWidth(noTime, RADIOSTARS);
                        plotText((OLED_WIDTH - width) / 2, (OLED_HEIGHT - FONT_HEIGHT_RADIOSTARS) / 2, noTime, RADIOSTARS, WHITE);
                    }

                    // Draw arrows to scroll the mode
                    plotText(0, OLED_HEIGHT - FONT_HEIGHT_TOMTHUMB, "<", TOM_THUMB, WHITE);
                    plotText(OLED_WIDTH - 3, OLED_HEIGHT - FONT_HEIGHT_TOMTHUMB, ">", TOM_THUMB, WHITE);
                    break;
                }
                case RSSI_SUBMODE_ALTERNATE2:
                case RSSI_SUBMODE_ALTERNATE:
                {
                    clearDisplay();
                    if( rssi->mode == RSSI_SOFTAP )
                    {
                        rssi->submode = RSSI_SUBMODE_REGULAR;
                    }
                    else
                    {
                        int8_t this_rssi = wifi_station_get_rssi();

                        if( rssi->submode == RSSI_SUBMODE_ALTERNATE2 )
                        {
                            int selcol = this_rssi;
                            selcol += 90;
                            selcol *= 8;
                            uint32_t rcolor = EHSVtoHEX(selcol, 0xFF, 0xFF);
                            led_t leds[NUM_LIN_LEDS] = {{0}};
                            int i;
                            for( i = 0; i < NUM_LIN_LEDS; i++ )
                            {
                                leds[i].r = rcolor & 0xff;
                                leds[i].g = (rcolor >> 8) & 0xff;
                                leds[i].b = (rcolor >> 16) & 0xff;
                            }
                            setLeds(leds, sizeof(leds));
                        }
                        else
                        {
                            led_t leds[NUM_LIN_LEDS] = {{0}};
                            setLeds(leds, sizeof(leds));
                        }

                        os_sprintf( cts, "%ddb", this_rssi );
                        plotText(0, 0, cts, IBM_VGA_8, WHITE);
                        rssi->rssi_history[rssi->rssi_head] = this_rssi;
                        int rpl = rssi->rssi_head;
                        rssi->rssi_head = ( rssi->rssi_head + 1 ) % sizeof( rssi->rssi_history );
                        int x;
                        for( x = 0; x < OLED_WIDTH; x++ )
                        {
                            int8_t thisrssi = rssi->rssi_history[rpl];
                            rpl--;
                            if( rpl < 0 )
                            {
                                rpl += sizeof( rssi->rssi_history );
                            }
                            int y = -thisrssi - 30; //Bottom out at -94dbm
                            if( y < 0 )
                            {
                                y = 0;
                            }
                            if( y >= OLED_HEIGHT )
                            {
                                y = OLED_HEIGHT - 1;
                            }
                            plotLine(x, OLED_HEIGHT - 1, x, y, WHITE);
                        }
                        plotText(0, OLED_HEIGHT - FONT_HEIGHT_TOMTHUMB, "<", TOM_THUMB, INVERSE);
                        plotText(OLED_WIDTH - 3, OLED_HEIGHT - FONT_HEIGHT_TOMTHUMB, ">", TOM_THUMB, INVERSE);
                    }
                    break;
                }
            }
            break;
        }
    }
}

static void ICACHE_FLASH_ATTR HandleEnterPressOnSubmode( rssiSubmode sm )
{
    switch( rssi->mode )
    {
        default:
        case RSSI_SOFTAP:
        case RSSI_STATION:
        {
            if( sm == RSSI_SUBMODE_REGULAR )
            {
                rssi->mode = RSSI_MENU;
            }
            break;
        }
        case RSSI_PASSWORD_ENTER:
        case RSSI_MENU:
        case RSSI_SCAN:
        {
            break;
        }
    }
}

/**
 * TODO
 *
 * @param state  A bitmask of all button states, unused
 * @param button The button which triggered this event
 * @param down   true if the button was pressed, false if it was released
 */
void ICACHE_FLASH_ATTR rssiButtonCallback( uint8_t state,
        int button __attribute__((unused)), int down __attribute__((unused)))
{
    switch (rssi->mode)
    {
        default:
        case RSSI_MENU:
        {
            if(down)
            {
                RSSI_PRINTF( "DOWN BUTTON: %d\n", button );
                menuButton(rssi->menu, button);
            }
            break;
        }
        case RSSI_PASSWORD_ENTER:
        {
            if( !textEntryInput( down, button ) )
            {
                textEntryEnd();
                rssi->mode = RSSI_STATION;
                wifi_set_opmode_current( STATION_MODE );
                struct station_config sc;
                RSSI_PRINTF( "Connecting to \"%s\" password \"%s\"\n", rssi->connectssid, rssi->password );
                ets_memset( (char*)&sc, 0, sizeof(sc) );
                ets_strcpy( (char*)sc.password, rssi->password );
                ets_strcpy( (char*)sc.ssid, rssi->connectssid);
                sc.all_channel_scan = 1;
                wifi_station_set_config( &sc );
                wifi_station_connect();
                wifi_set_sleep_type(NONE_SLEEP_T);

                // Save these as the last used params
                setSsidPw(rssi->connectssid, rssi->password);
            }
            break;
        }
        case RSSI_SOFTAP:
        case RSSI_SCAN:
        case RSSI_STATION:
        {
            if( down )
            {
                if( button == LEFT )
                {
                    rssiSubmode m = rssi->submode;
                    m++;
                    if( m == RSSI_SUBMODE_MAX )
                    {
                        rssi->submode = 0;
                    }
                    else
                    {
                        rssi->submode = m;
                    }
                }
                else if( button == RIGHT )
                {
                    rssiSubmode m = rssi->submode;
                    if( m == 0 )
                    {
                        rssi->submode = RSSI_SUBMODE_MAX - 1;
                    }
                    else
                    {
                        rssi->submode = m - 1;
                    }
                }
                else if( button == UP )
                {
                    if(rssi->sntpInit)
                    {
                        rssi->tz++;
                        if(14 == rssi->tz)
                        {
                            rssi->tz = -11;
                        }
                        sntp_stop();
                        sntp_set_timezone(rssi->tz);
                        sntp_init();
                    }
                    break;
                }
                else if( button == DOWN )
                {
                    if(rssi->sntpInit)
                    {
                        rssi->tz--;
                        if(-12 == rssi->tz)
                        {
                            rssi->tz = 13;
                        }
                        sntp_stop();
                        sntp_set_timezone(rssi->tz);
                        sntp_init();
                    }
                    break;
                }
                else if( button == ACTION )
                {
                    HandleEnterPressOnSubmode( rssi->submode );
                }
            }
            rssi->buttonState = state;
            break;
        }
    }
}

/**
 * Write the day of the week to a string
 *
 * @param str The string to write to
 * @param dow The day of the week
 */
void ICACHE_FLASH_ATTR strDow(char* str, int dow)
{
    switch(dow)
    {
        default:
        case 0:
        {
            ets_strcpy(str, "Sunday");
            break;
        }
        case 1:
        {
            ets_strcpy(str, "Monday");
            break;
        }
        case 2:
        {
            ets_strcpy(str, "Tuesday");
            break;
        }
        case 3:
        {
            ets_strcpy(str, "Wednesday");
            break;
        }
        case 4:
        {
            ets_strcpy(str, "Thursday");
            break;
        }
        case 5:
        {
            ets_strcpy(str, "Friday");
            break;
        }
        case 6:
        {
            ets_strcpy(str, "Saturday");
            break;
        }
    }
}

/**
 * Write the month and day of the month to a string
 *
 * @param str The string to write to
 * @param mDay The day of the month, 1-31
 * @param mon The month, 0-11
 */
void ICACHE_FLASH_ATTR strMon(char* str, int mDay, int mon)
{
    switch(mon)
    {
        default:
        case 0:
        {
            ets_sprintf(str, "January %d", mDay);
            break;
        }
        case 1:
        {
            ets_sprintf(str, "February %d", mDay);
            break;
        }
        case 2:
        {
            ets_sprintf(str, "March %d", mDay);
            break;
        }
        case 3:
        {
            ets_sprintf(str, "April %d", mDay);
            break;
        }
        case 4:
        {
            ets_sprintf(str, "May %d", mDay);
            break;
        }
        case 5:
        {
            ets_sprintf(str, "June %d", mDay);
            break;
        }
        case 6:
        {
            ets_sprintf(str, "July %d", mDay);
            break;
        }
        case 7:
        {
            ets_sprintf(str, "August %d", mDay);
            break;
        }
        case 8:
        {
            ets_sprintf(str, "September %d", mDay);
            break;
        }
        case 9:
        {
            ets_sprintf(str, "October %d", mDay);
            break;
        }
        case 10:
        {
            ets_sprintf(str, "November %d", mDay);
            break;
        }
        case 11:
        {
            ets_sprintf(str, "December %d", mDay);
            break;
        }
    }
}

/**
 * Write the hour, minute, and second to a string
 *
 * @param str The string to write to
 * @param h The hour, 1-24
 * @param m The minute, 0-59
 * @param s The second, 0-59
 * @param space true to use 12 hour time and AM/PM, false to use 24 hour time
 */
void ICACHE_FLASH_ATTR strTime(char* str, int h, int m, int s, bool space)
{
    if(space)
    {
        bool am = true;
        if(h > 12)
        {
            h -= 12;
            am = false;
        }
        ets_sprintf(str, "%d:%02d:%02d %s", h, m, s, am ? "am" : "pm");
    }
    else
    {
        ets_sprintf(str, "%02d:%02d:%02d", h, m, s);
    }
}

/**
 * Given a hue and an angle, write the appropriate color to the LEDs.
 * The brightness is related to how close the angle is to each LED
 *
 * @param leds The LEDs to write to
 * @param hue The hue for this clock arm
 * @param handAngle The angle for this clock arm
 */
void ICACHE_FLASH_ATTR drawClockArm(led_t* leds, uint8_t hue, int16_t handAngle)
{
    // Each LED is positioned at this angle
    const int16_t degrees[NUM_LIN_LEDS] = {210, 270, 330, 30, 90, 150};

    // For each LED
    for(uint8_t idx = 0; idx < NUM_LIN_LEDS; idx++)
    {
        // Find the difference between the given angle and this LED's position
        int16_t diff = ABS(handAngle - degrees[idx]);
        // Account for wraparound
        if(diff > 180)
        {
            diff = ABS(diff - 360);
        }

        // If the handAngle is close enough to this LED
        if(diff < 60)
        {
            // Use the difference to calculate the brightness (never get fullbright)
            uint32_t lColor = EHSVtoHEX(hue, 0xFF, ((60 - diff) * 0xFF) / 90);
            // Sum this color to the LED
            leds[idx].r += ((lColor >>  0) & 0xFF);
            leds[idx].g += ((lColor >>  8) & 0xFF);
            leds[idx].b += ((lColor >> 16) & 0xFF);
        }
    }
}
