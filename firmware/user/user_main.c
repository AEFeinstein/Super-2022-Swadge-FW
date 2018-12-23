// Copyright 2015 <>< Charles Lohr Under the MIT/x11 License, NewBSD License or
// ColorChord License. You Choose.

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
#include "ssid.h"
#include "user_main.h"
#include "espnow.h"
#include "missingEspFnPrototypes.h"
#include "espNowUtils.h"
#include "mode_guitar_tuner.h"
#include "mode_colorchord.h"
#include "mode_reflector_game.h"
#include "mode_random_d6.h"
#include "mode_dance.h"
#include "mode_flashlight.h"

/*============================================================================
 * Defines
 *==========================================================================*/

// #define PROFILE

#define PORT 7777
#define SERVER_TIMEOUT 1500
#define MAX_CONNS 5
#define MAX_FRAME 2000
#define TICKER_TIMEOUT 100

#define PROC_TASK_PRIO 0
#define PROC_TASK_QUEUE_LEN 1

#define REMOTE_IP_CODE 0x0a00c90a // = 10.201.0.10

#define UDP_TIMEOUT 50

#define NUM_BUTTON_EVTS 10

#define RTC_MEM_ADDR 64

#define DEBOUNCE_US      200000
#define DEBOUNCE_US_FAST  10000

/*============================================================================
 * Structs
 *==========================================================================*/

typedef struct
{
    uint8_t stat;
    int btn;
    int down;
    uint32_t time;
} buttonEvt;

/*============================================================================
 * Variables
 *==========================================================================*/

static os_timer_t some_timer = {0};
static struct espconn* pUdpServer = NULL;

static bool hpa_is_paused_for_wifi = false;

os_event_t procTaskQueue[PROC_TASK_QUEUE_LEN] = {{0}};
uint32_t samp_iir = 0;

int send_back_on_ip = 0;
int send_back_on_port = 0;
int udp_pending = 0;
int status_update_count = 0;
int got_an_ip = 0;
int soft_ap_mode = 0;
int wifi_fails = 0;
int ticks_since_override = 1000000;
uint8_t mymac[6] = {0};

swadgeMode* swadgeModes[] =
{
    &colorchordMode,
    &reflectorGameMode,
    &dancesMode,
    &randomD6Mode,
    &flashlightMode,
    &guitarTunerMode,
};
bool swadgeModeInit = false;

buttonEvt buttonQueue[NUM_BUTTON_EVTS] = {{0}};
uint8_t buttonEvtHead = 0;
uint8_t buttonEvtTail = 0;
bool pendingNextSwadgeMode = false;
uint32_t lastButtonPress[3] = {0};
bool debounceEnabled = true;

rtcMem_t rtcMem = {0};
os_timer_t modeSwitchTimer = {0};

uint8_t modeLedBrightness = 0;

/*============================================================================
 * Prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR HandleButtonEventIRQ( uint8_t stat, int btn, int down );
void ICACHE_FLASH_ATTR HandleButtonEventSynchronous(void);

static int ICACHE_FLASH_ATTR SwitchToSoftAP(void);
void ICACHE_FLASH_ATTR TransmitGenericEvent(void);

void ICACHE_FLASH_ATTR RETick(void);
static void ICACHE_FLASH_ATTR procTask(os_event_t* events);

void ICACHE_FLASH_ATTR user_init(void);
static void ICACHE_FLASH_ATTR timerFunc100ms(void* arg);
static void ICACHE_FLASH_ATTR udpserver_recv(void* arg, char* pusrdata, unsigned short len);

void ICACHE_FLASH_ATTR incrementSwadgeModeNoSleep(void);
void ICACHE_FLASH_ATTR modeSwitchTimerFn(void* arg);
void ICACHE_FLASH_ATTR DeepSleepChangeSwadgeMode(void);

/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * This deinitializes the current mode if it is initialized, displays the next
 * mode's LED pattern, and starts a timer to reboot into the next mode.
 * If the reboot timer is running, it will be reset
 */
void ICACHE_FLASH_ATTR incrementSwadgeModeNoSleep(void)
{
    // Call the exit callback for the current mode
    if(swadgeModeInit && NULL != swadgeModes[rtcMem.currentSwadgeMode]->fnExitMode)
    {
        swadgeModes[rtcMem.currentSwadgeMode]->fnExitMode();

        // Clean up ESP NOW if that's where we were at
        switch(swadgeModes[rtcMem.currentSwadgeMode]->wifiMode)
        {
            case ESP_NOW:
            {
                espNowDeinit();
                break;
            }
            case SOFT_AP:
            case NO_WIFI:
            {
                break;
            }
        }
        swadgeModeInit = false;
    }

    // Switch to the next mode, or start from the beginning if we're at the end
    rtcMem.currentSwadgeMode = (rtcMem.currentSwadgeMode + 1) % (sizeof(swadgeModes) / sizeof(swadgeModes[0]));

    // Start a timer to reboot into this mode
    modeLedBrightness = 0xFF;
    os_timer_disarm(&modeSwitchTimer);
    os_timer_arm(&modeSwitchTimer, 3, true);
}

/**
 * TODO
 *
 * @param arg
 */
void ICACHE_FLASH_ATTR modeSwitchTimerFn(void* arg __attribute__((unused)))
{
    if(0 == modeLedBrightness)
    {
        os_timer_disarm(&modeSwitchTimer);
        DeepSleepChangeSwadgeMode();
    }
    else
    {
        // Show the LEDs for this mode before rebooting into it
        showLedCount(1 + rtcMem.currentSwadgeMode,
                     getLedColorPerNumber(rtcMem.currentSwadgeMode, modeLedBrightness));

        modeLedBrightness--;
    }
}

/**
 * Switch to the next registered swadge mode. This will wait until the mode
 * switch / programming mode button isn't pressed, then deep sleep into the next
 * swadge mode.
 *
 * Calling wifi_set_opmode_current() and wifi_set_opmode() in here causes crashes
 */
void ICACHE_FLASH_ATTR DeepSleepChangeSwadgeMode(void)
{
    // If the mode switch button is held down
    if(GetButtons() & 0x01)
    {
        // Try rebooting again in 1ms
        os_timer_disarm(&modeSwitchTimer);
        os_timer_arm(&modeSwitchTimer, 1, false);
        return;
    }

    // Check if the next mode wants wifi or not
    switch(swadgeModes[rtcMem.currentSwadgeMode]->wifiMode)
    {
        case SOFT_AP:
        case ESP_NOW:
        {
            // Sleeeeep. It calls user_init on wake, which will use the new mode
            enterDeepSleep(false, 1000);
            break;
        }
        case NO_WIFI:
        {
            // Sleeeeep. It calls user_init on wake, which will use the new mode
            enterDeepSleep(true, 1000);
            break;
        }
    }
}

/**
 * Enter deep sleep mode for some number of microseconds. This also
 * controls whether or not WiFi will be enabled when the ESP wakes.
 *
 * @param disableWifi true to disable wifi, false to enable wifi
 * @param sleepUs     The duration of time (us) when the device is in Deep-sleep.
 */
void ICACHE_FLASH_ATTR enterDeepSleep(bool disableWifi, uint64_t sleepUs)
{
    system_rtc_mem_write(RTC_MEM_ADDR, &rtcMem, sizeof(rtcMem));
    os_printf("rtc mem written\r\n");

    if(disableWifi)
    {
        // Disable RF after deep-sleep wake up, just like modem sleep; this
        // has the least current consumption; the device is not able to
        // transmit or receive data after wake up.
        system_deep_sleep_set_option(4);
        os_printf("deep sleep option set 4\r\n");
    }
    else
    {
        // Radio calibration is done after deep-sleep wake up; this increases
        // the current consumption.
        system_deep_sleep_set_option(1);
        os_printf("deep sleep option set 1\r\n");
    }
    system_deep_sleep(sleepUs);
}

/**
 * Configure and enable SoftAP mode. This will have the ESP broadcast a SSID
 * of the form MAGBADGE_XXXXXX, where XXXXXX is the tail of the MAC address
 *
 * @return The Wifi channel, 1-13, or -1 for a failure
 */
static int ICACHE_FLASH_ATTR SwitchToSoftAP(void)
{
    // Get default SSID parameters
    struct softap_config c;
    wifi_softap_get_config_default(&c);

    // Build the SSID name
    char ssidPrefix[] = "MAGBADGE_";
    ets_memcpy( c.ssid, ssidPrefix, ets_strlen(ssidPrefix) );
    if(false == wifi_get_macaddr(SOFTAP_IF, mymac))
    {
        return -1;
    }
    ets_sprintf( (char*)(&c.ssid[9]), "%02x%02x%02x", mymac[3], mymac[4], mymac[5] );

    // Set the SSID parameters, no authentication
    c.password[0] = 0;
    c.ssid_len = ets_strlen( (char*)c.ssid );
    c.channel = SOFTAP_CHANNEL;
    c.authmode = NULL_MODE;
    c.ssid_hidden = 0;
    c.max_connection = 4;
    c.beacon_interval = 1000;

    // Apply SSID parameters
    // 0x01: Station mode
    // 0x02: SoftAP mode
    // 0x03: Station + SoftAP
    if(false == wifi_set_opmode_current( SOFTAP_MODE ))
    {
        return -1;
    }

    // Save current configs
    if(false == wifi_softap_set_config(&c))
    {
        return -1;
    }
    if(false == wifi_softap_set_config_current(&c))
    {
        return -1;
    }

    // Set the channel
    if(false == wifi_set_channel( c.channel ))
    {
        return -1;
    }

    // Note the connection and return the channel
    os_printf( "Making it a softap, channel %d\n", c.channel );
    got_an_ip = 1;
    soft_ap_mode = 1;
    return c.channel;
}

/**
 * Build a generic packet with diagnostic information and send it to
 * send_back_on_ip, send_back_on_port if those values are set, or
 * REMOTE_IP_CODE:8000 if they are not
 */
void ICACHE_FLASH_ATTR TransmitGenericEvent(void)
{
    uint8_t packetIdx = 0;
    uint8_t sendpack[32];

    // Copy the tail of the MAC address
    ets_memcpy( sendpack, mymac, sizeof(mymac) );
    packetIdx += sizeof(mymac);

    // Magic bytes
    sendpack[packetIdx++] = 0x01;
    sendpack[packetIdx++] = 0x03;

    // RSSI (signal strength)
    sendpack[packetIdx++] = wifi_station_get_rssi();

    // Copy the bssid
    struct station_config stationConf;
    wifi_station_get_config(&stationConf);
    ets_memcpy( &sendpack[packetIdx], stationConf.bssid, sizeof(stationConf.bssid) );
    packetIdx += sizeof(stationConf.bssid);

    // GPIO & button info
    sendpack[packetIdx++] = getLastGPIOState();
    sendpack[packetIdx++] = buttonEvtHead;
    sendpack[packetIdx++] = buttonEvtTail;

    // Two bytes voltage info, guess it can't be read
    sendpack[packetIdx++] = 0;
    sendpack[packetIdx++] = 0;

    // Colorchord LED count
    sendpack[packetIdx++] = USE_NUM_LIN_LEDS;

    // Number of times this status packet has been sent
    sendpack[packetIdx++] = status_update_count >> 8;
    sendpack[packetIdx++] = status_update_count & 0xff;
    status_update_count++;

    // Free heap space
    uint16_t heapfree = system_get_free_heap_size();
    sendpack[packetIdx++] = heapfree >> 8;
    sendpack[packetIdx++] = heapfree & 0xff;
    sendpack[packetIdx++] = 0;
    sendpack[packetIdx++] = 0;

    // Something to do with elapsed time
    uint32_t cc = xthal_get_ccount();
    sendpack[packetIdx++] = cc >> 24;
    sendpack[packetIdx++] = cc >> 16;
    sendpack[packetIdx++] = cc >> 8;
    sendpack[packetIdx++] = cc;

    // If we're connected to something
    if( got_an_ip )
    {
        // And there's a specific server to reply to
        if( send_back_on_ip && send_back_on_port )
        {
            // Send the packet to the server
            pUdpServer->proto.udp->remote_port = send_back_on_port;
            uint32_to_IP4(send_back_on_ip, pUdpServer->proto.udp->remote_ip);
            send_back_on_ip = 0;
            send_back_on_port = 0;
            espconn_sendto( (struct espconn*)pUdpServer, sendpack, sizeof( sendpack ));
        }
        else
        {
            // Otherwise send it to REMOTE_IP_CODE:8000
            pUdpServer->proto.udp->remote_port = 8000;
            uint32_to_IP4(REMOTE_IP_CODE, pUdpServer->proto.udp->remote_ip);
            udp_pending = UDP_TIMEOUT;
            espconn_sendto( (struct espconn*)pUdpServer, sendpack, sizeof( sendpack ));
        }

    }
}

/**
 * Tasks that happen all the time.
 * Called at the end of each procTask() loop
 */
void ICACHE_FLASH_ATTR RETick(void)
{
    // If colorchord is active and the HPA isn't running, start it
    if( COLORCHORD_ACTIVE && !isHpaRunning() )
    {
        ExitCritical();
    }

    // If colorchord isn't running and the HPA is running, stop it
    if( !COLORCHORD_ACTIVE && isHpaRunning() )
    {
        EnterCritical();
    }

    // Common services tick, fast mode
    CSTick( 0 );
}

/**
 * This task is constantly called by posting itself instead of being in an
 * infinite loop. ESP doesn't like infinite loops.
 *
 * It handles synchronous button events, audio samples which have been read
 * and are queued for processing, and calling RETick()
 *
 * @param events Checked before posting this task again
 */
static void ICACHE_FLASH_ATTR procTask(os_event_t* events)
{
    // Post another task to this thread
    system_os_post(PROC_TASK_PRIO, 0, 0 );

    // For profiling so we can see how much CPU is spent in this loop.
#ifdef PROFILE
    WRITE_PERI_REG( PERIPHS_GPIO_BASEADDR + GPIO_ID_PIN(0), 1 );
#endif

    // Process queued button presses synchronously
    HandleButtonEventSynchronous();

    // While there are samples available from the ADC
    while( sampleAvailable() )
    {
        // Get the sample
        int32_t samp = getSample();
        // Run the sample through an IIR filter
        samp_iir = samp_iir - (samp_iir >> 10) + samp;
        samp = (samp - (samp_iir >> 10)) * 16;
        // Amplify the sample
        samp = (samp * CCS.gINITIAL_AMP) >> 4;
        // Push the sample to colorchord

        // Pass the button to the mode
        if(swadgeModeInit && NULL != swadgeModes[rtcMem.currentSwadgeMode]->fnAudioCallback)
        {
            swadgeModes[rtcMem.currentSwadgeMode]->fnAudioCallback(samp);
        }
    }

#ifdef PROFILE
    WRITE_PERI_REG( PERIPHS_GPIO_BASEADDR + GPIO_ID_PIN(0), 0 );
#endif

    if( events->sig == 0 && events->par == 0 )
    {
        RETick();
    }
}

/**
 * Timer handler for a software timer set to fire every 100ms, forever.
 * Calls CSTick() every 100ms.
 *
 * If the hardware is in wifi station mode, this Enables the hardware timer
 * to sample the ADC once the IP address has been received and printed
 *
 * Also handles logic for infrastrucure wifi mode, which isn't being used
 *
 * @param arg unused
 */
static void ICACHE_FLASH_ATTR timerFunc100ms(void* arg __attribute__((unused)))
{
    CSTick( 1 );

    // Tick the current mode every 100ms
    if(swadgeModeInit && NULL != swadgeModes[rtcMem.currentSwadgeMode]->fnTimerCallback)
    {
        swadgeModes[rtcMem.currentSwadgeMode]->fnTimerCallback();
    }

    if( udp_pending )
    {
        udp_pending--;
    }

    if( ticks_since_override < TICKER_TIMEOUT )
    {
        // Color override?
        ticks_since_override++;
    }
    else if( hpa_is_paused_for_wifi && printed_ip )
    {
        StartHPATimer(); // Init the high speed ADC timer.
        hpa_is_paused_for_wifi = 0; // only need to do once prevents unstable ADC
    }

    struct station_config wcfg;
    struct ip_info ipi;
    int stat = wifi_station_get_connect_status();
    if( stat == STATION_WRONG_PASSWORD || stat == STATION_NO_AP_FOUND || stat == STATION_CONNECT_FAIL )
    {
        wifi_station_disconnect();
        wifi_fails++;
        os_printf( "Connection failed with code %d... Retrying, try: %d", stat, wifi_fails );
        os_printf("\n");
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
        ets_memset( ledOut + wifi_fails * 3, 255, 3 );
        setLeds( (led_t*)ledOut, USE_NUM_LIN_LEDS * 3 );
    }
    else if( stat == STATION_GOT_IP && !got_an_ip )
    {
        wifi_station_get_config( &wcfg );
        wifi_get_ip_info(0, &ipi);
        os_printf( "STAT: %d\n", stat );
#define chop_ip(x) (((x)>>0)&0xff), (((x)>>8)&0xff), (((x)>>16)&0xff), (((x)>>24)&0xff)
        os_printf( "IP: %d.%d.%d.%d\n", chop_ip(ipi.ip.addr) );
        os_printf( "NM: %d.%d.%d.%d\n", chop_ip(ipi.netmask.addr) );
        os_printf( "GW: %d.%d.%d.%d\n", chop_ip(ipi.gw.addr) );
        os_printf( "Connected to: /%s/\n", wcfg.ssid );
        got_an_ip = 1;
        wifi_fails = 0;
    }
}

/**
 * UDP Packet handler, registered with espconn_regist_recvcb().
 * It mostly replies with debug packets from TransmitGenericEvent()
 *
 * @param arg      The espconn struct for this packet
 * @param pusrdata The data which was received
 * @param len      The length of the data received
 */
static void ICACHE_FLASH_ATTR udpserver_recv(void* arg, char* pusrdata, unsigned short len)
{
    struct espconn* pespconn = (struct espconn*)arg;

    remot_info* ri = 0;
    espconn_get_connection_info( pespconn, &ri, 0);

    // uint8_t buffer[MAX_FRAME];
    // uint8_t ledout[] = { 0x00, 0xff, 0xaa, 0x00, 0xff, 0xaa, };
    //os_printf("X");
    // ws2812_push( pusrdata+3, len );
    //os_printf( "%02x\n", pusrdata[6] );
    if( pusrdata[6] == 0x11 )
    {

        send_back_on_ip = IP4_to_uint32(ri->remote_ip);
        send_back_on_port = ri->remote_port;

        TransmitGenericEvent();
    }
    if( pusrdata[6] == 0x13 )
    {
        uint8_t ledret[USE_NUM_LIN_LEDS * 3 + 6 + 2];
        ets_memcpy( ledret, mymac, 6 );
        ledret[6] = 0x14;
        ledret[7] = USE_NUM_LIN_LEDS;
        ets_memcpy( ledret, ledOut, USE_NUM_LIN_LEDS * 3 );

        // Request LEDs
        send_back_on_ip = IP4_to_uint32(ri->remote_ip);
        send_back_on_port = ri->remote_port;

        pUdpServer->proto.udp->remote_port = send_back_on_port;
        uint32_to_IP4(send_back_on_ip, pUdpServer->proto.udp->remote_ip);
        send_back_on_ip = 0;
        send_back_on_port = 0;
        espconn_sendto( (struct espconn*)pUdpServer, ledret, sizeof( ledret ));

        TransmitGenericEvent();
    }
    else if( pusrdata[6] == 0x02 )
    {
        if (! ((mymac[5] ^ pusrdata[7])&pusrdata[8]) )
        {
            ets_memcpy( ledOut, pusrdata + 10, len - 10 );
            ticks_since_override = TICKER_TIMEOUT + 1;
            setLeds( (led_t*)ledOut, 255 * 3 );
            ticks_since_override = 0;
        }
    }
}

/**
 * UART RX handler, called by uart0_rx_intr_handler(). Currently does nothing
 * This is an interrupt, so it can't be ICACHE_FLASH_ATTR.
 *
 * @param c The char received on the UART
 */
void ICACHE_FLASH_ATTR charrx( uint8_t c __attribute__((unused)))
{
    ;
}

/**
 * This function is passed into SetupGPIO() as the callback for button interrupts.
 * It is called every time a button is pressed or released
 *
 * This is an interrupt, so it can't be ICACHE_FLASH_ATTR. It quickly queues
 * button events
 *
 * @param stat A bitmask of all button statuses
 * @param btn The button number which was pressed
 * @param down 1 if the button was pressed, 0 if it was released
 */
void HandleButtonEventIRQ( uint8_t stat, int btn, int down )
{
    // Queue up the button event
    buttonQueue[buttonEvtTail].stat = stat;
    buttonQueue[buttonEvtTail].btn = btn;
    buttonQueue[buttonEvtTail].down = down;
    buttonQueue[buttonEvtTail].time = system_get_time();
    buttonEvtTail = (buttonEvtTail + 1) % NUM_BUTTON_EVTS;
}

/**
 * Process queued button events synchronously
 */
void ICACHE_FLASH_ATTR HandleButtonEventSynchronous(void)
{
    if(buttonEvtHead != buttonEvtTail)
    {
        // The 0th button is the mode switch button, don't pass that to the mode
        if(0 == buttonQueue[buttonEvtHead].btn)
        {
            // Make sure no two presses happen within 100ms of each other
            if(buttonQueue[buttonEvtHead].time - lastButtonPress[buttonQueue[buttonEvtHead].btn] < DEBOUNCE_US)
            {
                ; // Consume this event below, don't count it as a press
            }
            else if(buttonQueue[buttonEvtHead].down)
            {
                // Note the time of this button press
                lastButtonPress[buttonQueue[buttonEvtHead].btn] = buttonQueue[buttonEvtHead].time;
                incrementSwadgeModeNoSleep();
            }
        }
        // Pass the button to the mode
        else if(swadgeModeInit && NULL != swadgeModes[rtcMem.currentSwadgeMode]->fnButtonCallback)
        {
            if((debounceEnabled && buttonQueue[buttonEvtHead].time - lastButtonPress[buttonQueue[buttonEvtHead].btn] < DEBOUNCE_US)
                    ||
                    (buttonQueue[buttonEvtHead].time - lastButtonPress[buttonQueue[buttonEvtHead].btn] < DEBOUNCE_US_FAST))
            {
                ; // Consume this event below, don't count it as a press
            }
            else
            {
                // Pass the button event to the mode
                swadgeModes[rtcMem.currentSwadgeMode]->fnButtonCallback(
                    buttonQueue[buttonEvtHead].stat,
                    buttonQueue[buttonEvtHead].btn,
                    buttonQueue[buttonEvtHead].down);

                // Note the time of this button press
                lastButtonPress[buttonQueue[buttonEvtHead].btn] = buttonQueue[buttonEvtHead].time;
            }
        }

        // Increment the head
        buttonEvtHead = (buttonEvtHead + 1) % NUM_BUTTON_EVTS;
    }
}

/**
 * Enable or disable button debounce for non-mode switch buttons
 *
 * @param enable true to enable, false to disable
 */
void ICACHE_FLASH_ATTR enableDebounce(bool enable)
{
    debounceEnabled = enable;
}

/**
 * Required function, must call system_partition_table_regist()
 */
void ICACHE_FLASH_ATTR user_pre_init(void)
{
    LoadDefaultPartitionMap();
}

/**
 * The main initialization function
 */
void ICACHE_FLASH_ATTR user_init(void)
{
    // Initialize the UART
    uart_init(BIT_RATE_74880, BIT_RATE_74880);
    os_printf("\r\nSwadge 2019\r\n");

    // Read data fom RTC memory if we're waking from deep sleep
    struct rst_info* resetInfo = system_get_rst_info();
    if(REASON_DEEP_SLEEP_AWAKE == resetInfo->reason)
    {
        os_printf("read rtc mem\r\n");
        // Try to read from rtc memory
        if(!system_rtc_mem_read(RTC_MEM_ADDR, &rtcMem, sizeof(rtcMem)))
        {
            os_printf("rtc mem read fail\r\n");
            // if it fails, zero it out instead
            ets_memset(&rtcMem, 0, sizeof(rtcMem));
        }
    }
    else
    {
        // if it fails, zero it out instead
        ets_memset(&rtcMem, 0, sizeof(rtcMem));
    }

    // Initialize GPIOs
    SetupGPIO(HandleButtonEventIRQ,
              NULL != swadgeModes[rtcMem.currentSwadgeMode]->fnAudioCallback);

    // If SOFT_AP is requested, but the left button isnt held, override it with NO_WIFI
    if(SOFT_AP == swadgeModes[rtcMem.currentSwadgeMode]->wifiMode &&
            (GetButtons() & 0b10) != 0b10)
    {
        swadgeModes[rtcMem.currentSwadgeMode]->wifiMode = NO_WIFI;
    }

    // Set up a timer to switch the swadge mode
    os_timer_disarm(&modeSwitchTimer);
    os_timer_setfn(&modeSwitchTimer, modeSwitchTimerFn, NULL);

    // Set the current WiFi mode based on what the swadge mode wants
    switch(swadgeModes[rtcMem.currentSwadgeMode]->wifiMode)
    {
        case SOFT_AP:
        case ESP_NOW:
        {
            if(!(wifi_set_opmode_current( SOFTAP_MODE ) &&
                    wifi_set_opmode( SOFTAP_MODE )))
            {
                os_printf("Set SOFTAP_MODE before boot failed\r\n");
            }
            break;
        }
        case NO_WIFI:
        {
            if(!(wifi_set_opmode_current( NULL_MODE ) &&
                    wifi_set_opmode( NULL_MODE )))
            {
                os_printf("Set NULL_MODE before boot failed\r\n");
            }
            break;
        }
    }

    os_printf("swadge mode %d\r\n", rtcMem.currentSwadgeMode);

    // Uncomment this to force a system restore.
    // system_restore();

    // Load configurable parameters from SPI memory
    LoadSettings();

    os_printf("Wins: %d\r\n", getRefGameWins());

#ifdef PROFILE
    GPIO_OUTPUT_SET(GPIO_ID_PIN(0), 0);
#endif

    // Held buttons aren't used on boot for anything, but they could be
    /*
    int firstbuttons = GetButtons();
    if( (firstbuttons & 0x08) )
    {
    // Restore all settings to
    os_printf( "Restore and save defaults (except # of leds).\n" );
    RevertAndSaveAllSettingsExceptLEDs();
    }
    */

    switch(swadgeModes[rtcMem.currentSwadgeMode]->wifiMode)
    {
        case SOFT_AP:
        {
            SwitchToSoftAP( );
            os_printf( "Booting in SoftAP\n" );
            break;
        }
        case ESP_NOW:
        {
            espNowInit();
            os_printf( "Booting in ESP-NOW\n" );
            break;
        }
        case NO_WIFI:
        {
            os_printf( "Booting with no wifi\n" );
            break;
        }
    }

    // Common services pre-init
    CSPreInit();

    switch(swadgeModes[rtcMem.currentSwadgeMode]->wifiMode)
    {
        case SOFT_AP:
        {
            // Set up UDP server to process received packets with udpserver_recv()
            // The local port is 8001 and the remote port is 8000
            pUdpServer = (struct espconn*)os_zalloc(sizeof(struct espconn));
            ets_memset( pUdpServer, 0, sizeof( struct espconn ) );
            espconn_create( pUdpServer );
            pUdpServer->type = ESPCONN_UDP;
            pUdpServer->proto.udp = (esp_udp*)os_zalloc(sizeof(esp_udp));
            pUdpServer->proto.udp->local_port = 8001;
            pUdpServer->proto.udp->remote_port = 8000;
            uint32_to_IP4(REMOTE_IP_CODE, pUdpServer->proto.udp->remote_ip);
            espconn_regist_recvcb(pUdpServer, udpserver_recv);
            int error = 0;
            if( (error = espconn_create( pUdpServer )) )
            {
                while(1)
                {
                    os_printf( "\r\nCould not create UDP server %d\r\n", error );
                }
            }

            // Common services (wifi) init. Sets up another UDP server to receive
            // commands (issue_command)and an HTTP server
            CSInit(true);
            break;
        }
        case ESP_NOW:
        case NO_WIFI:
        {
            os_printf( "Don't start a server\n" );
            // Common services (wifi) init. Sets up another UDP server to receive
            // commands (issue_command)and an HTTP server
            CSInit(false);
            break;
        }
    }

    // Start a software timer to call CSTick() every 100ms and start the hw timer eventually
    os_timer_disarm(&some_timer);
    os_timer_setfn(&some_timer, (os_timer_func_t*)timerFunc100ms, NULL);
    os_timer_arm(&some_timer, 100, 1);

    // Only start the HPA timer if there's an audio callback
    if(NULL != swadgeModes[rtcMem.currentSwadgeMode]->fnAudioCallback)
    {
        // Tricky: If we are in station mode, wait for that to get resolved before enabling the high speed timer.
        if( wifi_get_opmode() == 1 )
        {
            hpa_is_paused_for_wifi = true;
        }
        else
        {
            // Init the high speed ADC timer.
            StartHPATimer();
        }
    }

    // Initialize LEDs
    ws2812_init();

    // Attempt to make ADC more stable
    // https:// github.com/esp8266/Arduino/issues/2070
    // see peripherals https:// espressif.com/en/support/explore/faq
    // wifi_set_sleep_type(NONE_SLEEP_T); // on its own stopped wifi working
    // wifi_fpm_set_sleep_type(NONE_SLEEP_T); // with this seemed no difference

    // Initialize the current mode
    os_printf("mode: %s\r\n", swadgeModes[rtcMem.currentSwadgeMode]->modeName);
    if(NULL != swadgeModes[rtcMem.currentSwadgeMode]->fnEnterMode)
    {
        swadgeModes[rtcMem.currentSwadgeMode]->fnEnterMode();
    }
    swadgeModeInit = true;

    // Add a process to filter queued ADC samples and output LED signals
    system_os_task(procTask, PROC_TASK_PRIO, procTaskQueue, PROC_TASK_QUEUE_LEN);
    // Kick off procTask()
    system_os_post(PROC_TASK_PRIO, 0, 0 );
}

/**
 * If the firmware enters a critical section, disable the hardware timer
 * used to sample the ADC and the corresponding interrupt
 */
void EnterCritical(void)
{
    PauseHPATimer();
    ets_intr_lock();
}

/**
 * If the firmware leaves a critical section, enable the hardware timer
 * used to sample the ADC. This allows the interrupt to fire.
 */
void ExitCritical(void)
{
    ets_intr_unlock();
    ContinueHPATimer();
}

/**
 * Set the state of the six RGB LEDs, but don't overwrite if the LEDs were
 * set via UDP for at least TICKER_TIMEOUT increments of 100ms
 *
 * @param ledData Array of LED color data. Every three bytes corresponds to
 * one LED in RGB order. So index 0 is LED1_R, index 1 is
 * LED1_G, index 2 is LED1_B, index 3 is LED2_R, etc.
 * @param ledDataLen The length of buffer, most likely 6*3
 */
void ICACHE_FLASH_ATTR setLeds(led_t* ledData, uint16_t ledDataLen)
{
    // If the LEDs were overwritten with a UDP command, keep them that way for a while
    if( ticks_since_override < TICKER_TIMEOUT )
    {
        return;
    }
    else
    {
        // Otherwise send out the LED data
        ws2812_push( (uint8_t*) ledData, ledDataLen );
        //os_printf("%s, %d LEDs\r\n", __func__, ledDataLen / 3);
    }
}

/**
 * Get a color that corresponds to a number, 0-5
 *
 * @param num A number, 0-5
 * @return A color 0xBBGGRR
 */
uint32_t ICACHE_FLASH_ATTR getLedColorPerNumber(uint8_t num, uint8_t lightness)
{
    num = (num + 4) % 6;
    return EHSVtoHEX((num * 255) / 6, 0xFF, lightness);
}

/**
 * This displays the num of LEDs, all lit in the same color. The pattern is
 * nice for counting
 *
 * @param num   The number of LEDs to light
 * @param color The color to light the LEDs
 */
void ICACHE_FLASH_ATTR showLedCount(uint8_t num, uint32_t color)
{
    led_t leds[6] = {{0}};

    led_t rgb;
    rgb.r = (color >> 16) & 0xFF;
    rgb.g = (color >>  8) & 0xFF;
    rgb.b = (color >>  0) & 0xFF;

    // Set the LEDs
    switch(num)
    {
        case 6:
        {
            ets_memcpy(&leds[3], &rgb, sizeof(rgb));
        }
        // no break
        case 5:
        {
            ets_memcpy(&leds[0], &rgb, sizeof(rgb));
        }
        // no break
        case 4:
        {
            ets_memcpy(&leds[1], &rgb, sizeof(rgb));
            ets_memcpy(&leds[2], &rgb, sizeof(rgb));
            ets_memcpy(&leds[4], &rgb, sizeof(rgb));
            ets_memcpy(&leds[5], &rgb, sizeof(rgb));
            break;
        }

        case 3:
        {
            ets_memcpy(&leds[0], &rgb, sizeof(rgb));
            ets_memcpy(&leds[2], &rgb, sizeof(rgb));
            ets_memcpy(&leds[4], &rgb, sizeof(rgb));
            break;
        }

        case 2:
        {
            ets_memcpy(&leds[3], &rgb, sizeof(rgb));
        }
        // no break
        case 1:
        {
            ets_memcpy(&leds[0], &rgb, sizeof(rgb));
            break;
        }
    }

    // Draw the LEDs
    setLeds(leds, sizeof(leds));
}
