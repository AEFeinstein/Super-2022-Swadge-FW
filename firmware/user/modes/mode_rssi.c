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
    rssiModeScreen mode;
    timer_t updateTimer;
    uint8_t buttonState;
    rssiSubmode submode;
    menu_t* menu;

	char    scanssids[MAX_SCAN][32+5];
	char    password[33];
	char    connectssid[33];
	int8_t  rssi_history[128];
	int     rssi_head;
	//int8_t  scanrssi[MAX_SCAN];
	//AUTH_MODE  scansecure[MAX_SCAN];
	int num_scan;
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
static void ICACHE_FLASH_ATTR rssi_scan_done_cb(void *arg, STATUS status);
static void ICACHE_FLASH_ATTR rssiSetupMenu(void);

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
	if( rssi->menu )     deinitMenu(rssi->menu);
    rssi->menu = initMenu(fl_title, rssiMenuCb);
    addRowToMenu(rssi->menu);
	addItemToRow(rssi->menu, fl_scan);
	int i;
	for( i = 0; i < rssi->num_scan; i++ )
	{
	    addRowToMenu(rssi->menu);
		addItemToRow( rssi->menu, rssi->scanssids[i]);
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


static void ICACHE_FLASH_ATTR rssi_scan_done_cb(void *bss_struct, STATUS status)
{
	if (status == OK) {
		os_printf("Scan OK [%u]\n", status);
		struct bss_info *bss = (struct bss_info *) bss_struct;
		int i = 0;
		while (bss != 0 && i < MAX_SCAN){
			ets_sprintf( rssi->scanssids[i], "%2d%c%d ", bss->channel, (bss->authmode==AUTH_OPEN)?'S':'*', bss->rssi );
			memcpy( rssi->scanssids[i]+7, bss->ssid, bss->ssid_len );
			rssi->scanssids[i][bss->ssid_len+7] = 0;
			bss = STAILQ_NEXT(bss, next);
			i++;
		}
		rssi->num_scan = i;
		rssi->mode = RSSI_MENU;
		rssiSetupMenu();
	}
	else
	{
		os_printf( "Scan Fail\n" );
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
		if( menuItem[2] == 'S' )
		{
			textEntryStart( 32, rssi->password );
			strcpy( rssi->connectssid, menuItem + 7 );
			rssi->mode = RSSI_PASSWORD_ENTER;
		}
		if( menuItem[2] == ' ' )
		{
			os_printf( "Connect to SSID %s\n", menuItem+4 );
	        rssi->mode = RSSI_STATION;
		    wifi_set_opmode_current( STATION_MODE );
		    struct station_config sc;
		    memset( (char*)&sc, 0, sizeof(sc) );
		    os_memcpy( (char*)sc.ssid, menuItem+7, strlen(menuItem+7) );
		    sc.all_channel_scan = 1;
		    wifi_station_set_config( &sc );
		    wifi_station_connect();
		    wifi_set_sleep_type(NONE_SLEEP_T);
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
                struct ip_info ipi;

               // if( rssi->mode == RSSI_STATION )
                {
                    sint8 sr = wifi_station_get_rssi();
                    os_sprintf( cts, "RSSI:%d", sr );
                    plotText(0, 0, cts, IBM_VGA_8, WHITE);
                }

                bool b = wifi_get_ip_info( (rssi->mode==RSSI_SOFTAP)?SOFTAP_IF:STATION_IF, &ipi);
                os_sprintf( cts, " LOAD %d", b );
                plotText(64, 0, cts, IBM_VGA_8, WHITE);
                os_sprintf( cts, " IP %d.%d.%d.%d", IP2STR( &ipi.ip ) );
                plotText(0, 16, cts, IBM_VGA_8, WHITE);
                os_sprintf( cts, " NM %d.%d.%d.%d", IP2STR( &ipi.netmask ) );
                plotText(0, 32, cts, IBM_VGA_8, WHITE);
                os_sprintf( cts, "<GW %d.%d.%d.%d", IP2STR( &ipi.gw ) );
                plotText(0, 48, cts, IBM_VGA_8, WHITE);
                plotText(120, 48, ">", IBM_VGA_8, WHITE);
                led_t leds[NUM_LIN_LEDS] = {{0}};
                setLeds(leds, sizeof(leds));
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
						for( i = 3; i < NUM_LIN_LEDS; i++ )
						{
							leds[i].r = rcolor & 0xff;
							leds[i].g = (rcolor>>8) & 0xff;
							leds[i].b = (rcolor>>16) & 0xff;
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
						if( rpl < 0 ) rpl += sizeof( rssi->rssi_history );
						int y = -thisrssi-30; //Bottom out at -94dbm
						if( y < 0 ) y = 0;
						if( y >= OLED_HEIGHT ) y = OLED_HEIGHT-1;
						plotLine(x,OLED_HEIGHT-1,x,y, WHITE);
					}
	                plotText(0, 48, "<", IBM_VGA_8, BLACK);
    	            plotText(120, 48, ">", IBM_VGA_8, BLACK);
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
				rssi->mode = RSSI_MENU;
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
			    rssi->mode = RSSI_STATION;
				wifi_set_opmode_current( STATION_MODE );
				struct station_config sc;
				os_printf( "Connectiong to \"%s\" password \"%s\"\n", rssi->connectssid, rssi->password );
				memset( (char*)&sc, 0, sizeof(sc) );
				strcpy( (char*)sc.password, rssi->password );
				strcpy( (char*)sc.ssid, rssi->connectssid);
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
                if( button == 0 )
                {
                    rssiSubmode m = rssi->submode;
                    m++;
                    if( m == RSSI_SUBMODE_MAX )
                        rssi->submode = 0;
                    else
                        rssi->submode = m;
                }
                else if( button == 2 )
                {
                    rssiSubmode m = rssi->submode;
                    if( m == 0 )
                        rssi->submode = RSSI_SUBMODE_MAX-1;
                    else
                        rssi->submode = m-1;
                }
				else if( button == 4 )
				{
					HandleEnterPressOnSubmode( rssi->submode );
				}
            }
            rssi->buttonState = state;
            break;
        }
    }
}


