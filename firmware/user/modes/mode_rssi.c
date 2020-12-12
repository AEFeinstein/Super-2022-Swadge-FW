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
    RSSI_SUBMODE_MAX,
} rssiSubmode;

#define MAX_SCAN 20

typedef struct
{
    char ssid[64];
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
    char    password[64];
    char    connectssid[64];
    int8_t  rssi_history[128];
    int     rssi_head;
    int num_scan;

    bool sntpInit; // time init
    int8_t tz; // timezone
} rssi_t;

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
        wifi_set_opmode_current( STATION_MODE );
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
                // if(rssi->aps[i].auth != AUTH_OPEN)
                // {
                textEntryStart( 32, rssi->password );
                ets_strcpy( rssi->connectssid, menuItem);
                rssi->mode = RSSI_PASSWORD_ENTER;
                // }
                // else
                // {
                //  RSSI_PRINTF( "Connect to SSID %s\n", menuItem );
                //  rssi->mode = RSSI_STATION;
                //  wifi_set_opmode_current( STATION_MODE );
                //  struct station_config sc;
                //  ets_memset( (char*)&sc, 0, sizeof(sc) );
                //  os_memcpy( (char*)sc.ssid, menuItem, ets_strlen(menuItem) );
                //  sc.all_channel_scan = 1;
                //  wifi_station_set_config( &sc );
                //  wifi_station_connect();
                //  wifi_set_sleep_type(NONE_SLEEP_T);
                // }
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
            textEntryDraw();
            break;
        case RSSI_SCAN:
        {
            clearDisplay();
            plotText(0, 0, "Scanning...", IBM_VGA_8, WHITE);
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
                {
                    // First clear the OLED
                    clearDisplay();
                    int16_t textY = 0;
                    struct ip_info ipi;

#define VERT_SPACING 5

                    os_sprintf( cts, "RSSI:%d", wifi_station_get_rssi());
                    plotText(0, textY, cts, IBM_VGA_8, WHITE);
                    textY += (FONT_HEIGHT_IBMVGA8 + VERT_SPACING);

                    if(wifi_get_ip_info( (rssi->mode == RSSI_SOFTAP) ? SOFTAP_IF : STATION_IF, &ipi))
                    {
                        os_sprintf( cts, " IP %d.%d.%d.%d", IP2STR( &ipi.ip ) );
                        plotText(0, textY, cts, IBM_VGA_8, WHITE);
                        textY += (FONT_HEIGHT_IBMVGA8 + VERT_SPACING);

                        os_sprintf( cts, " NM %d.%d.%d.%d", IP2STR( &ipi.netmask ) );
                        plotText(0, textY, cts, IBM_VGA_8, WHITE);
                        textY += (FONT_HEIGHT_IBMVGA8 + VERT_SPACING);

                        os_sprintf( cts, " GW %d.%d.%d.%d", IP2STR( &ipi.gw ) );
                        plotText(0, textY, cts, IBM_VGA_8, WHITE);
                        textY += (FONT_HEIGHT_IBMVGA8 + VERT_SPACING);

                        // Start NTP if we have an IP address and it hasn't been started yet
                        if(ipi.ip.addr != 0 && false == rssi->sntpInit)
                        {
                            os_printf("Init SNTP\n");
                            rssi->sntpInit = true;
                            sntp_setservername(0, "time.google.com");
                            sntp_setservername(1, "time.cloudflare.com");
                            sntp_setservername(2, "time-a-g.nist.gov");
                            sntp_init();
                            sntp_set_timezone(rssi->tz); // Eastern
                        }
                        else if(rssi->sntpInit)
                        {
                            time_t ts = sntp_get_current_timestamp();
                            if (0 != ts)
                            {
                                struct tm* tStruct = sntp_localtime(&ts);
                                char* am = "am";
                                char* pm = "pm";
                                char* suffix = am;
                                if(tStruct->tm_hour > 12)
                                {
                                    tStruct->tm_hour -= 12;
                                    suffix = pm;
                                }
                                os_sprintf(cts, "%d:%02d:%02d%s", tStruct->tm_hour, tStruct->tm_min, tStruct->tm_sec, suffix);
                                int16_t width = textWidth(cts, IBM_VGA_8);
                                plotText(OLED_WIDTH - width - 1, 0, cts, IBM_VGA_8, WHITE);
                            }
                        }
                    }

                    plotText(0, OLED_HEIGHT - FONT_HEIGHT_TOMTHUMB, "<", TOM_THUMB, WHITE);
                    plotText(OLED_WIDTH - 3, OLED_HEIGHT - FONT_HEIGHT_TOMTHUMB, ">", TOM_THUMB, WHITE);
                    break;
                }
                case RSSI_SUBMODE_ALTERNATE2:
                case RSSI_SUBMODE_ALTERNATE:
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

                        os_sprintf( cts, "RSSI:%d", this_rssi );
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
            break;
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
                INIT_PRINTF( "DOWN BUTTON: %d\n", button );
                menuButton(rssi->menu, button);
            }
            break;
        }
        case RSSI_PASSWORD_ENTER:
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
            }
            break;
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
