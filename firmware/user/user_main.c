//Copyright 2015 <>< Charles Lohr Under the MIT/x11 License, NewBSD License or
// ColorChord License.  You Choose.

/*============================================================================
 * Includes
 *==========================================================================*/

#include "mem.h"
#include "c_types.h"
#include "user_interface.h"
#include "ets_sys.h"
#include "uart.h"
#include "osapi.h"
#include "espconn.h"
#include "esp82xxutil.h"
#include "ws2812_i2s.h"
#include "hpatimer.h"
#include "DFT32.h"
#include "ccconfig.h"
#include <embeddednf.h>
#include <embeddedout.h>
#include <commonservices.h>
#include "ets_sys.h"
#include "gpio.h"
#include "gpio_buttons.h"
#include "custom_commands.h"

/*============================================================================
 * Defines
 *==========================================================================*/

//#define PROFILE

#define PORT 7777
#define SERVER_TIMEOUT 1500
#define MAX_CONNS 5
#define MAX_FRAME 2000
#define TICKER_TIMEOUT 100

#define PROC_TASK_PRIO        0
#define PROC_TASK_QUEUE_LEN    1

#define REMOTE_IP_CODE 0x0a00c90a // = 10.201.0.10

#define UDP_TIMEOUT 50

/*============================================================================
 * Variables
 *==========================================================================*/

static volatile os_timer_t some_timer = {0};
static struct espconn *pUdpServer = NULL;

static bool hpa_is_paused_for_wifi = false;

os_event_t    procTaskQueue[PROC_TASK_QUEUE_LEN] = {0};
uint32_t samp_iir = 0;
int samplesProcessed = 0;

int send_back_on_ip = 0;
int send_back_on_port = 0;
int udp_pending;
int status_update_count;
int got_an_ip = 0;
int soft_ap_mode = 0;
int wifi_fails;
int ticks_since_override = 1000000;
uint8_t mymac[6];

uint8_t last_button_event_btn;
uint8_t last_button_event_dn;

void (*retick)();

/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * TODO doc
 *
 * @return
 */
static int ICACHE_FLASH_ATTR SwitchToSoftAP( )
{
	struct softap_config c;
	wifi_softap_get_config_default(&c);
	memcpy( c.ssid, "MAGBADGE_", 9 );
	wifi_get_macaddr(SOFTAP_IF, mymac);
	ets_sprintf( c.ssid+9, "%02x%02x%02x", mymac[3],mymac[4],mymac[5] ); 
	c.password[0] = 0;
	c.ssid_len = ets_strlen( c.ssid );
	c.channel = 1;
	c.authmode = NULL_MODE;
	c.ssid_hidden = 0;
	c.max_connection = 4;
	c.beacon_interval = 1000;
	wifi_softap_set_config(&c);
	wifi_softap_set_config_current(&c);
	wifi_set_opmode_current( 2 );
	wifi_softap_set_config_current(&c);
	wifi_set_channel( c.channel );	
	printf( "Making it a softap, channel %d\n", c.channel );
	got_an_ip = 1;
	soft_ap_mode = 1;
	return c.channel;
}

/**
 * TODO doc
 */
void ICACHE_FLASH_ATTR user_rf_pre_init()
{
}

/**
 * Call this once we've stacked together one full colorchord frame.
 */
static void NewFrame()
{
	if( !COLORCHORD_ACTIVE ) return;
	if( ticks_since_override < TICKER_TIMEOUT ) return;
	int i;
	HandleFrameInfo();

	switch( COLORCHORD_OUTPUT_DRIVER )
	{
	case 0:
		UpdateLinearLEDs();
		break;
	case 1:
		UpdateAllSameLEDs();
		break;
	};

	ws2812_push( ledOut, USE_NUM_LIN_LEDS * 3 );
}

/**
 * TODO doc
 */
void ICACHE_FLASH_ATTR TransmitGenericEvent()
{
	uint8_t sendpack[32];
	ets_memcpy( sendpack, mymac, 6 );
	sendpack[6] = 0x01;
	sendpack[7] = 0x03;
	sendpack[8] = wifi_station_get_rssi();
	{
		struct station_config stationConf;
		wifi_station_get_config(&stationConf);
		ets_memcpy( sendpack+9, stationConf.bssid, 6 );
	}
	sendpack[15] = LastGPIOState;
	sendpack[16] = last_button_event_btn;
	sendpack[17] = last_button_event_dn;
	sendpack[18] = 0; //voltage
	sendpack[19] = 0; //voltage
	sendpack[20] = USE_NUM_LIN_LEDS; //sum power
	sendpack[21] = status_update_count>>8;
	sendpack[22] = status_update_count&0xff;
	status_update_count++;
	uint16_t heapfree = system_get_free_heap_size();
	sendpack[23] = heapfree>>8;
	sendpack[24] = heapfree&0xff;
	sendpack[25] = 0;
	sendpack[26] = 0;
	uint32_t cc = xthal_get_ccount();
	sendpack[27] = cc>>24;
	sendpack[28] = cc>>16;
	sendpack[29] = cc>>8;
	sendpack[30] = cc;

	if( got_an_ip )
	{
		if( send_back_on_ip && send_back_on_port )
		{
			pUdpServer->proto.udp->remote_port = send_back_on_port;
			uint32_to_IP4(send_back_on_ip,pUdpServer->proto.udp->remote_ip);
			send_back_on_ip = 0; send_back_on_port = 0;
			espconn_sendto( (struct espconn *)pUdpServer, sendpack, sizeof( sendpack ));
		}
		else
		{
			pUdpServer->proto.udp->remote_port = 8000;
			uint32_to_IP4(REMOTE_IP_CODE,pUdpServer->proto.udp->remote_ip);  
			udp_pending = UDP_TIMEOUT;
			espconn_sendto( (struct espconn *)pUdpServer, sendpack, sizeof( sendpack ));
		}

	}

	last_button_event_btn = 0;
	last_button_event_dn = 0;
}

/**
 * TODO doc
 * Tasks that happen all the time.
 */
void  ICACHE_FLASH_ATTR RETick()
{

	if( COLORCHORD_ACTIVE && !isHpaRunning() )
	{
		ExitCritical();
	}

	if( !COLORCHORD_ACTIVE && isHpaRunning() )
	{
		EnterCritical();
	}

	CSTick( 0 );

	//Send button press events.
	if( last_button_event_btn || udp_pending == 0 )
	{
		TransmitGenericEvent();
	}

}

/**
 * TODO doc
 * @param events
 */
static void procTask(os_event_t *events)
{
	system_os_post(PROC_TASK_PRIO, 0, 0 );

	//For profiling so we can see how much CPU is spent in this loop.
#ifdef PROFILE
	WRITE_PERI_REG( PERIPHS_GPIO_BASEADDR + GPIO_ID_PIN(0), 1 );
#endif
	while( sampleAvailable() )
	{
		int32_t samp = getSample();
		samp_iir = samp_iir - (samp_iir>>10) + samp;
		samp = (samp - (samp_iir>>10))*16;
		samp = (samp * CCS.gINITIAL_AMP) >> 4;
		PushSample32( samp );

		samplesProcessed++;
		if( samplesProcessed == 128 )
		{
			NewFrame();
			samplesProcessed = 0; 
		}
	}
#ifdef PROFILE
	WRITE_PERI_REG( PERIPHS_GPIO_BASEADDR + GPIO_ID_PIN(0), 0 );
#endif

	if( events->sig == 0 && events->par == 0 )
	{
		if( !retick ) retick = RETick;
		retick();
	}

}

/**
 * Timer handler for a software timer set to fire every 100ms, forever.
 * Calls CSTick() every 100ms.
 * If the hardware is in wifi station mode, this Enables the hardware timer
 * to sample the ADC once the IP address has been received and printed
 *
 * @param arg unused
 */
static void ICACHE_FLASH_ATTR myTimer(void *arg)
{
	CSTick( 1 );

	if( udp_pending ) udp_pending --;

	if( ticks_since_override < TICKER_TIMEOUT )
	{
		//Color override?
		ticks_since_override++;
	}
	else if( hpa_is_paused_for_wifi && printed_ip )
	{
		StartHPATimer(); //Init the high speed  ADC timer.
		hpa_is_paused_for_wifi = 0; // only need to do once prevents unstable ADC
	}

	{
		struct station_config wcfg;
		struct ip_info ipi;
		int stat = wifi_station_get_connect_status();
		if( stat == STATION_WRONG_PASSWORD || stat == STATION_NO_AP_FOUND || stat == STATION_CONNECT_FAIL ) {
			wifi_station_disconnect();
			wifi_fails++;
			printf( "Connection failed with code %d... Retrying, try: %d", stat, wifi_fails );
			printf("\n");
			if( wifi_fails == 2 )
			{
				SwitchToSoftAP();
				got_an_ip = 1;
				printed_ip = 1;
			}
			else
			{
				wifi_station_connect();
				got_an_ip = 0;
			}
			memset(  ledOut + wifi_fails*3, 255, 3 );
			ws2812_push( ledOut, USE_NUM_LIN_LEDS * 3 );
		} else if( stat == STATION_GOT_IP && !got_an_ip ) {
			wifi_station_get_config( &wcfg );
			wifi_get_ip_info(0, &ipi);
			printf( "STAT: %d\n", stat );
			#define chop_ip(x) (((x)>>0)&0xff), (((x)>>8)&0xff), (((x)>>16)&0xff), (((x)>>24)&0xff)
			printf( "IP: %d.%d.%d.%d\n", chop_ip(ipi.ip.addr)      );
			printf( "NM: %d.%d.%d.%d\n", chop_ip(ipi.netmask.addr) );
			printf( "GW: %d.%d.%d.%d\n", chop_ip(ipi.gw.addr)      );
			printf( "Connected to: /%s/\n"  , wcfg.ssid );
			got_an_ip = 1;
			wifi_fails = 0;
		}
	}

//	uart0_sendStr(".");
//	printf( "%d/%d\n",soundtail,soundhead );
//	printf( "%d/%d\n",soundtail,soundhead );
//	uint8_t ledout[] = { 0x00, 0xff, 0xaa, 0x00, 0xff, 0xaa, };
//	ws2812_push( ledout, 6 );
}

/**
 * UDP Packet handler, registered with espconn_regist_recvcb().
 * It takes the UDP data and jams it right into the LEDs
 *
 * @param arg The espconn struct for this packet
 * @param pusrdata
 * @param len
 */
static void ICACHE_FLASH_ATTR udpserver_recv(void *arg, char *pusrdata, unsigned short len)
{
	struct espconn *pespconn = (struct espconn *)arg;

	remot_info * ri = 0;
	espconn_get_connection_info( pespconn, &ri, 0);

//	uint8_t buffer[MAX_FRAME];
//	uint8_t ledout[] = { 0x00, 0xff, 0xaa, 0x00, 0xff, 0xaa, };
	//uart0_sendStr("X");
	//ws2812_push( pusrdata+3, len );
	//printf( "%02x\n", pusrdata[6] );
	if( pusrdata[6] == 0x11 )
	{

		send_back_on_ip = IP4_to_uint32(ri->remote_ip);
		send_back_on_port = ri->remote_port;

		TransmitGenericEvent();
	}
	if( pusrdata[6] == 0x13 )
	{
		uint8_t ledret[USE_NUM_LIN_LEDS*3+6+2];
		ets_memcpy( ledret, mymac, 6 );
		ledret[6] = 0x14;
		ledret[7] = USE_NUM_LIN_LEDS;
		ets_memcpy( ledret, ledOut, USE_NUM_LIN_LEDS*3 );

		//Request LEDs
		send_back_on_ip = IP4_to_uint32(ri->remote_ip);
		send_back_on_port = ri->remote_port;

		pUdpServer->proto.udp->remote_port = send_back_on_port;
		uint32_to_IP4(send_back_on_ip,pUdpServer->proto.udp->remote_ip);
		send_back_on_ip = 0; send_back_on_port = 0;
		espconn_sendto( (struct espconn *)pUdpServer, ledret, sizeof( ledret ));

		TransmitGenericEvent();
	}
	else if( pusrdata[6] == 0x02 )
	{
		if (! ((mymac[5] ^ pusrdata[7])&pusrdata[8]) )
		{
			ets_memcpy( ledOut, pusrdata + 10, len - 10 );
			ws2812_push( ledOut, 255 * 3 );
			ticks_since_override = 0;
		}

	}
}

/**
 * UART RX handler, called by uart0_rx_intr_handler(). Currently does nothing
 *
 * @param c The char received on the UART
 */
void ICACHE_FLASH_ATTR charrx( uint8_t c )
{
	//Called from UART.
}

/**
 * TODO doc
 *
 * @param stat
 * @param btn
 * @param down
 */
void ICACHE_FLASH_ATTR HandleButtonEvent( uint8_t stat, int btn, int down )
{
	//XXX WOULD BE NICE: Implement some sort of event queue.
	last_button_event_btn = btn+1;
	last_button_event_dn = down;
	system_os_post(0, 0, 0 );
}

/**
 * The main initialization function
 * TODO doc more
 */
void ICACHE_FLASH_ATTR user_init(void)
{
	uart_init(BIT_RATE_115200, BIT_RATE_115200);
	int wifiMode = wifi_get_opmode();

	uart0_sendStr("\r\nColorChord\r\n");

//Uncomment this to force a system restore.
//	system_restore();

	// Load configurable parameters from SPI memory
	CustomStart();

#ifdef PROFILE
	GPIO_OUTPUT_SET(GPIO_ID_PIN(0), 0);
#endif
	SetupGPIO();

	int firstbuttons = GetButtons();
	//if( firstbuttons & 0x20 ) disable_deep_sleep = 1;

	//Can't use buttons 0, 1, or 5 for startup options.
	//0x04 will be flashlight mode.
	//0x08 will be to restore default colorchord settings.

	if( (firstbuttons & 0x04) )
	{
		wifi_set_opmode_current( 0 );

		ws2812_init();
		memset( ledOut, 255, 3*20 );
		uart0_sendStr( "Flashlight mode.\n" );
		ws2812_push( ledOut, 3*20 ); //Buffersize = Nr LEDs * 3
		ets_delay_us(10000);

		stop_i2s();

		while(1)
		{
			ets_delay_us(100000);
			system_soft_wdt_feed();
		}
	}
	if( (firstbuttons & 0x10) )
	{
		SwitchToSoftAP( 0 );
		uart0_sendStr( "Booting in SoftAP\n" );
	}
	else
	{
		struct station_config stationConf;
		wifi_station_get_config(&stationConf);
		wifi_get_macaddr(STATION_IF, mymac);
		uart0_sendStr( "Connecting to infrastructure\n" );
		LoadSSIDAndPassword( stationConf.ssid, stationConf.password );
		stationConf.bssid_set = 0;
		wifi_set_opmode_current( 1 );
		wifi_set_opmode( 1 );
		wifi_station_set_config(&stationConf);
		wifi_station_connect();
		wifi_station_set_config(&stationConf);  //I don't know why, doing this twice seems to make it store more reliably.
		soft_ap_mode = 0;
	}

	CSPreInit();

	// Set up UDP server to receive LED data, udpserver_recv()
    pUdpServer = (struct espconn *)os_zalloc(sizeof(struct espconn));
	ets_memset( pUdpServer, 0, sizeof( struct espconn ) );
	espconn_create( pUdpServer );
	pUdpServer->type = ESPCONN_UDP;
	pUdpServer->proto.udp = (esp_udp *)os_zalloc(sizeof(esp_udp));
	pUdpServer->proto.udp->local_port = 8001;
	pUdpServer->proto.udp->remote_port = 8000;
	uint32_to_IP4(REMOTE_IP_CODE,pUdpServer->proto.udp->remote_ip); 
	espconn_regist_recvcb(pUdpServer, udpserver_recv);

	if( espconn_create( pUdpServer ) )
	{
		while(1) { uart0_sendStr( "\r\nFAULT\r\n" ); }
	}

	if( (firstbuttons & 0x08) )
	{
		//Restore all settings to 
		uart0_sendStr( "Restore and save defaults (except # of leds).\n" );
		RevertAndSaveAllSettingsExceptLEDs();
	}

	// Common services (wifi) init. Sets up another UDP server to receive
	// commands (issue_command)and an HTTP server
	CSInit();

	// Add a process to filter queued ADC samples and output LED signals
	system_os_task(procTask, PROC_TASK_PRIO, procTaskQueue, PROC_TASK_QUEUE_LEN);

	// Start a software timer to call CSTick() every 100ms and start the hw timer eventually
	os_timer_disarm(&some_timer);
	os_timer_setfn(&some_timer, (os_timer_func_t *)myTimer, NULL);
	os_timer_arm(&some_timer, 100, 1);

	//Set GPIO16 for Input
    WRITE_PERI_REG(PAD_XPD_DCDC_CONF,
                   (READ_PERI_REG(PAD_XPD_DCDC_CONF) & 0xffffffbc) | (uint32)0x1); 	// mux configuration for XPD_DCDC and rtc_gpio0 connection

    WRITE_PERI_REG(RTC_GPIO_CONF,
                   (READ_PERI_REG(RTC_GPIO_CONF) & (uint32)0xfffffffe) | (uint32)0x0);	//mux configuration for out enable

    WRITE_PERI_REG(RTC_GPIO_ENABLE,
                   READ_PERI_REG(RTC_GPIO_ENABLE) & (uint32)0xfffffffe);	//out disable

    // Init colorchord
	InitColorChord();

	// Tricky: If we are in station mode, wait for that to get resolved before enabling the high speed timer.
	if( wifi_get_opmode() == 1 )
	{
		hpa_is_paused_for_wifi = true;
	}
	else
	{
		// Init the high speed  ADC timer.
		StartHPATimer();
	}

	// Initialize LEDs
	ws2812_init();

	// Attempt to make ADC more stable
	// https://github.com/esp8266/Arduino/issues/2070
	// see peripherals https://espressif.com/en/support/explore/faq
	//wifi_set_sleep_type(NONE_SLEEP_T); // on its own stopped wifi working
	//wifi_fpm_set_sleep_type(NONE_SLEEP_T); // with this seemed no difference

	memset( ledOut, 255, 3 );
	ws2812_push( ledOut, USE_NUM_LIN_LEDS * 3 );

	system_os_post(PROC_TASK_PRIO, 0, 0 );
}

/**
 * If the firmware leaves a critical section, enable the hardware timer
 * used to sample the ADC. This allows the interrupt to fire.
 */
void EnterCritical()
{
	PauseHPATimer();
	//ets_intr_lock();
}

/**
 * If the firmware enters a critical section, disable the hardware timer
 * used to sample the ADC and the corresponding interrupt
 */
void ExitCritical()
{
	//ets_intr_unlock();
	ContinueHPATimer();
}

