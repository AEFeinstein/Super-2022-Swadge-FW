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
#include <stdlib.h>

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
#include "espconn.h"

/*============================================================================
 * Defines, Structs, Enums
 *==========================================================================*/

#define RSSI_UPDATE_MS    20
#define NUM_PF_CLIENTS    10
#define LED_PORT        7777
#define PF_PORT            1337
#define ARTNET_PORT        6454

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
    RSSI_SUBMODE_PIXELFLUT,
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

    bool netInit; // time init
    int8_t tz; // timezone
    bool bEnableLEDsOnTime;
    bool bGotNetworkLED;
    bool bAlreadyUpdated; //Right now, only used on pixelflut mode.
    bool bEnableBufferMode;
    uint16_t artnetOffset;
    uint8_t iLastButtonState;
    bool bForcePixelFlutBuffer; //happens after buffer is enabled and received a packet.  Can no longer exit mode.

    struct espconn server_pf_socket;
    esp_tcp server_pf_socket_tcp;
    uint32_t time_of_pf_last[NUM_PF_CLIENTS];
    esp_tcp server_pf_clients_tcp[NUM_PF_CLIENTS];
    struct espconn server_pf_clients[NUM_PF_CLIENTS];

    struct espconn server_udp_leds;
    esp_udp server_udp_leds_udp;

    struct espconn server_udp_artnet;
    esp_udp server_udp_artnet_udp;

    struct espconn server_udp_pf;
    esp_udp server_udp_pf_udp;

    led_t set_leds[NUM_LIN_LEDS];
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

static void ICACHE_FLASH_ATTR RSSIConnectPIXELFLUTCB(void *pArg);
static void ICACHE_FLASH_ATTR RSSIRecvPIXELFLUTData(void *pArg, char *pData, unsigned short len);
static void ICACHE_FLASH_ATTR RSSIRecvLEDData(void *pArg, char *pData, unsigned short len);
static void ICACHE_FLASH_ATTR RSSIRecvARTNETData(void *pArg, char *pData, unsigned short len);
static void ICACHE_FLASH_ATTR ProcessPixelFlutCommand( struct espconn *pConn, const char * command, int len );

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

    rssi->netInit = false;
    rssi->bEnableLEDsOnTime = true;
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

    memset( rssi->set_leds, 0, sizeof( rssi->set_leds ) );
    setLeds( rssi->set_leds, sizeof(rssi->set_leds));
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
        rssi->bAlreadyUpdated = false;
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




static void ICACHE_FLASH_ATTR RSSIConnectPIXELFLUTCB(void *pArg) {
    struct espconn *pConn = (struct espconn *)pArg;
    RSSI_PRINTF("connection from %d.%d.%d.%d port %d\n",
              IP2STR(pConn->proto.tcp->remote_ip),
              pConn->proto.tcp->remote_port);
     
    espconn_regist_recvcb(pConn, &RSSIRecvPIXELFLUTData);
}



static void ICACHE_FLASH_ATTR ProcessPixelFlutCommand( struct espconn *pConn, const char * command, int len )
{
    if( len >= 4 && memcmp( command, "SIZE", 4 ) == 0 )
    {
        char st[64];
        int slen = os_sprintf( st, "SIZE %d %d\n", OLED_WIDTH, OLED_HEIGHT );
        espconn_send( pConn, (uint8_t*)st, slen );
    }
    else if( len >= 3 && memcmp( command, "PX ", 3 ) == 0 )
    {
        int sp1, sp2;
        for( sp1 = 3; sp1 < len && command[sp1] != ' '; sp1++ );
        for( sp2 = sp1+1; sp2 < len && command[sp2] != ' '; sp2++ );
        if( sp1 >= len ) goto help;

        int x = atoi( command + 2 );
        int y = atoi( command + sp1 );
        if( sp2 >= len )
        {
            //No 3rd parameter.  This is a get.
            char st[64];
            int slen = os_sprintf( st, "PX %d %d %06x\n", x, y, ( getPixel( x, y ) == WHITE )?0xffffff:0x000000 );
            espconn_send( pConn, (uint8_t*)st, slen );
        }
        else
        {
            //3rd parameter present, interpret as a color request.
            int value = strtol( command+sp2, 0, 16 );
            int r = (value>>24)&0xff;
            int g = (value>>16)&0xff;
            int b = (value>>8)&0xff;
            int a = (value>>0)&0xff;
            int avg = (r+g+b);
            RSSI_PRINTF( "DRAW AT %d, %d, %d %d\n", x, y, avg,a );
            if( a > 0x7f )
            {
                drawPixel( x, y, (avg > 0x17D)?WHITE:BLACK );
            }
        }
    }
    else
    {
        goto help;
    }
    return;
help:
    {
        const char * swadgpf = "Swadge PF Server; Usage:\nHELP: Display this message\nSIZE: Return size of target screen\nPX <x> <y>: Get value of pixel (0xFFFFFF or 0x000000 only)\nPX <x> <y> <value>: Set value of pixel (>0x7f average brightness = on, otherwise off)\nBUFFER <swadge-full display, 1kB data>";
        espconn_send( pConn, (uint8_t*)swadgpf, strlen( swadgpf ) );
    }
}

static void ICACHE_FLASH_ATTR RSSIRecvPIXELFLUTData(void *pArg, char *pData, unsigned short len) {
    struct espconn *pConn = (struct espconn *)pArg;
    //RSSI_PRINTF("PF received %d bytes from %d.%d.%d.%d port %d: \"%s\"\n",
    //          len,
    //          IP2STR(pConn->proto.tcp->remote_ip),
    //          pConn->proto.tcp->remote_port,
    //          pData);

    do
    {
        if( len >= 7+(OLED_WIDTH * (OLED_HEIGHT / 8)) )
        {
            if( memcmp( pData, "BUFFER ", 7 ) == 0 )
            {
                if( !rssi->bEnableBufferMode )
                {
                    const char * swadgpf = "Press Action to Enable Buffer Mode";
                    espconn_send( pConn, (uint8_t*)swadgpf, strlen( swadgpf ) );
                }
                else
                {
                    //Update screen
                    extern uint8_t currentFb[(OLED_WIDTH * (OLED_HEIGHT / 8))];
                    memcpy( currentFb, pData+7, (OLED_WIDTH * (OLED_HEIGHT / 8)) );
                    //currentFb[0] = 0;
                    extern bool fbChanges;
                    fbChanges = true;

                    //Reply with button states
                    char cts[16];
                    int sl = os_sprintf( cts, "BTN %d\n", rssi->iLastButtonState );

                    if( pConn->type == ESPCONN_UDP )
                    {
                        remot_info * ri = 0;
                        espconn_get_connection_info( pConn, &ri, 0);
                        ets_memcpy( pConn->proto.udp->remote_ip, ri->remote_ip, 4 );
                        pConn->proto.udp->remote_port = ri->remote_port;
                    }

                    espconn_send( pConn, (uint8_t*)cts, sl );

                    //Force buffer mode.
                    rssi->bForcePixelFlutBuffer = true;
                }
                return;
            }
        }
        //Scan line for newline.
        int epl = 0;
        for( epl = 0; epl < len && pData[epl] != '\n'; epl++ );
        if( epl < len ) pData[epl] = 0;    //Force null termination.
        if( epl ) ProcessPixelFlutCommand( pConn, pData, epl );
        if( epl < len ) epl++; //Advance past null.
        pData += epl;
        len -= epl;
    } while( len > 0 );

    return;
/*
abort:
    RSSI_PRINTF( "invalid command %s\n", pData );
    if( pConn->type == ESPCONN_TCP )
    {
        espconn_disconnect( pConn );
        espconn_delete( pConn );
    }
    return;
*/
}

static void ICACHE_FLASH_ATTR RSSIRecvLEDData(void *pArg, char *pData, unsigned short len)
{
    pArg = pArg;
//    struct espconn *pConn = (struct espconn *)pArg;

/*
    RSSI_PRINTF("LED received %d bytes from %d.%d.%d.%d port %d: \"%s\"\n",
              len,
              IP2STR(pConn->proto.tcp->remote_ip),
              pConn->proto.tcp->remote_port,
              pData);
*/

    int ll = len;
    ll -= 3;
    if( ll < 0 ) return;
    if( pData[0] || pData[1] || pData[2] ) return; //Not an LED packet.
    if( ll > 18 ) ll = 18; 

    //Skip 3 bytes.  Cause that's just how we do (From esp8266ws2812i2s)
    memcpy( rssi->set_leds, pData+3, ll );
}

static void ICACHE_FLASH_ATTR RSSIRecvARTNETData(void *pArg, char *pData, unsigned short len)
{
    struct espconn *pConn = (struct espconn *)pArg;

    RSSI_PRINTF("LED received %d bytes from %d.%d.%d.%d port %d: \"%s\"\n",
              len,
              IP2STR(pConn->proto.tcp->remote_ip),
              pConn->proto.tcp->remote_port,
              pData);


    int ll = len;
    if( ll <= 18 ) return;
    if( memcmp( pData, "Art-Net", 8 ) != 0 ) return; //Not an artnet packet.
    //Ignore protocol, universe, sequence, physical and length.
    ll -= 18; //18 bytes in header

    int offset = rssi->artnetOffset;
    ll -= offset;

    //3 colors x 6 LEDs
    if( ll > 18 ) ll = 18;

    memcpy( rssi->set_leds, pData+18+offset, ll );
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
                case RSSI_SUBMODE_PIXELFLUT:
                    if( !rssi->bAlreadyUpdated )
                    {
                        clearDisplay();
                        plotText(0, 0, "PIXELFLUT/ARTNET", IBM_VGA_8, WHITE);
                        char st[64];
                        os_sprintf( st, "ARTNET OFFSET %d\n", rssi->artnetOffset ); 
                        plotText(0, 20, st, IBM_VGA_8, WHITE);
                        os_sprintf( st, "BUFFER MODE %d\n", rssi->bEnableBufferMode ); 
                        plotText(0, 40, st, IBM_VGA_8, WHITE);
                        rssi->bAlreadyUpdated = true;
                    }
                    //Otherwise, don't touch screen, except on first frame..
                    break;
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
                        if(ipi.ip.addr != 0 && false == rssi->netInit)
                        {
                            RSSI_PRINTF("Init SNTP\n");
                            rssi->netInit = true;
                            sntp_setservername(0, "time.google.com");
                            sntp_setservername(1, "time.cloudflare.com");
                            sntp_setservername(2, "time-a-g.nist.gov");
                            sntp_init();
                            sntp_set_timezone(rssi->tz); // Eastern

                            //Set up other sockets.
                            memset( &rssi->server_udp_leds, 0, sizeof( rssi->server_udp_leds ) );
                            rssi->server_udp_leds.type = ESPCONN_UDP;
                            rssi->server_udp_leds.proto.udp = &rssi->server_udp_leds_udp;
                            rssi->server_udp_leds.proto.udp->local_port = LED_PORT;
                            espconn_regist_recvcb( &rssi->server_udp_leds, RSSIRecvLEDData);
                            espconn_create( &rssi->server_udp_leds );

                            memset( &rssi->server_udp_artnet, 0, sizeof( rssi->server_udp_artnet ) );
                            rssi->server_udp_artnet.type = ESPCONN_UDP;
                            rssi->server_udp_artnet.proto.udp = &rssi->server_udp_artnet_udp;
                            rssi->server_udp_artnet.proto.udp->local_port = ARTNET_PORT;
                            espconn_regist_recvcb( &rssi->server_udp_artnet, RSSIRecvARTNETData);
                            espconn_create( &rssi->server_udp_artnet );

                            memset( &rssi->server_udp_pf, 0, sizeof( rssi->server_udp_pf ) );
                            rssi->server_udp_pf.type = ESPCONN_UDP;
                            rssi->server_udp_pf.proto.udp = &rssi->server_udp_pf_udp;
                            rssi->server_udp_pf.proto.udp->local_port = PF_PORT;
                            espconn_regist_recvcb(&rssi->server_udp_pf, RSSIRecvPIXELFLUTData);
                            espconn_create( &rssi->server_udp_pf );

                            memset( &rssi->server_pf_socket, 0, sizeof( rssi->server_udp_pf ) );
                            rssi->server_pf_socket.type = ESPCONN_TCP;
                            rssi->server_pf_socket.proto.tcp = &rssi->server_pf_socket_tcp;
                            rssi->server_pf_socket.proto.tcp->local_port = PF_PORT;
                            espconn_regist_connectcb(&rssi->server_pf_socket, RSSIConnectPIXELFLUTCB);
                            espconn_create( &rssi->server_pf_socket );
                            espconn_accept( &rssi->server_pf_socket );
                        }
                        else if(rssi->netInit)
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
                                    memset( rssi->set_leds, 0, sizeof( rssi->set_leds ) );
                                    if( rssi->bEnableLEDsOnTime )
                                    {
                                        drawClockArm(rssi->set_leds, 0,   tStruct->tm_sec * 6);
                                        drawClockArm(rssi->set_leds, 85,  tStruct->tm_min * 6);
                                        drawClockArm(rssi->set_leds, 170, (tStruct->tm_hour % 12) * 30);
                                    }
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
                        rssi->bAlreadyUpdated = false;
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
                            int i;
                            for( i = 0; i < NUM_LIN_LEDS; i++ )
                            {
                                rssi->set_leds[i].r = rcolor & 0xff;
                                rssi->set_leds[i].g = (rcolor >> 8) & 0xff;
                                rssi->set_leds[i].b = (rcolor >> 16) & 0xff;
                            }
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
            setLeds(rssi->set_leds, sizeof(rssi->set_leds));
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
            switch( sm )
            {
            case RSSI_SUBMODE_REGULAR:
                rssi->mode = RSSI_MENU;
                break;
            case RSSI_SUBMODE_CLOCK:
                rssi->bEnableLEDsOnTime = !rssi->bEnableLEDsOnTime;
                break;
            case RSSI_SUBMODE_PIXELFLUT:
                rssi->bEnableBufferMode = !rssi->bEnableBufferMode;
                rssi->bAlreadyUpdated = 0;
                break;
            case RSSI_SUBMODE_ALTERNATE2:
            case RSSI_SUBMODE_MAX:
            case RSSI_SUBMODE_ALTERNATE:
            default:
                break;
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
    rssi->iLastButtonState = state&0x1f;
    if( rssi->bForcePixelFlutBuffer ) return;

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
                    rssi->bAlreadyUpdated = false;
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
                    rssi->bAlreadyUpdated = false;
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

                    if( rssi->submode == RSSI_SUBMODE_PIXELFLUT )
                    {
                        if( rssi->artnetOffset < 512 )
                        {
                            rssi->artnetOffset++;
                            rssi->bAlreadyUpdated = false;
                        }
                    }
                    else if(rssi->netInit)
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
                    if( rssi->submode == RSSI_SUBMODE_PIXELFLUT )
                    {
                        if( rssi->artnetOffset > 0 )
                        {
                            rssi->artnetOffset--;
                            rssi->bAlreadyUpdated = false;
                        }
                    }
                    else if(rssi->netInit)
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
        else if (h == 0)
        {
            h = 12;
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
