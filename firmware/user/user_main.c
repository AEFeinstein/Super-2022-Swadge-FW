// Copyright 2015 <>< Charles Lohr Under the MIT/x11 License, NewBSD License or
// ColorChord License. You Choose.

/*============================================================================
 * Includes
 *==========================================================================*/

#include "osapi.h"

#include "uart.h"
#include "ws2812_i2s.h"
#include "hpatimer.h"
#include "ccconfig.h"
#include <embeddedout.h>
#include <commonservices.h>
#include "gpio_user.h"
#include "buttons.h"
#include "custom_commands.h"
#include "user_main.h"
#include "espNowUtils.h"
#include "brzo_i2c.h"
#include "oled.h"
#include "mode_guitar_tuner.h"
#include "mode_colorchord.h"
#include "mode_reflector_game.h"
#include "mode_random_d6.h"
#include "mode_dance.h"
#include "mode_flashlight.h"
#include "mode_demo.h"

/*============================================================================
 * Defines
 *==========================================================================*/

#define PROC_TASK_PRIO 0
#define PROC_TASK_QUEUE_LEN 1

#define RTC_MEM_ADDR 64

/*============================================================================
 * Variables
 *==========================================================================*/

static os_timer_t timerHandle100ms = {0};

os_event_t procTaskQueue[PROC_TASK_QUEUE_LEN] = {{0}};
uint32_t samp_iir = 0;

swadgeMode* swadgeModes[] =
{
    &demoMode,
    &colorchordMode,
    &reflectorGameMode,
    &dancesMode,
    &randomD6Mode,
    &flashlightMode,
    &guitarTunerMode,
};
bool swadgeModeInit = false;

rtcMem_t rtcMem = {0};
os_timer_t modeSwitchTimer = {0};

uint8_t modeLedBrightness = 0;

/*============================================================================
 * Prototypes
 *==========================================================================*/

static void ICACHE_FLASH_ATTR procTask(os_event_t* events);

void ICACHE_FLASH_ATTR user_pre_init(void);
void ICACHE_FLASH_ATTR user_init(void);
static void ICACHE_FLASH_ATTR timerFunc100ms(void* arg);

void ICACHE_FLASH_ATTR incrementSwadgeModeNoSleep(void);
void ICACHE_FLASH_ATTR modeSwitchTimerFn(void* arg);
void ICACHE_FLASH_ATTR DeepSleepChangeSwadgeMode(void);

/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * Pass a button event from the logic in button.h to the active swadge mode
 *
 * @param state A bitmask of all button statuses
 * @param button  The button number which was pressed
 * @param down 1 if the button was pressed, 0 if it was released
 */
void ICACHE_FLASH_ATTR swadgeModeButtonCallback(uint8_t state, int button, int down)
{
    if(0 == button)
    {
        // TODO switch the mode
    }
    else if(swadgeModeInit && NULL != swadgeModes[rtcMem.currentSwadgeMode]->fnButtonCallback)
    {
        // Pass the button event to the mode
        swadgeModes[rtcMem.currentSwadgeMode]->fnButtonCallback(state, button, down);
    }
}

/**
 * TODO
 */
void ICACHE_FLASH_ATTR swadgeModeEspNowRecvCb(uint8_t* mac_addr, uint8_t* data, uint8_t len, uint8_t rssi)
{
    if(swadgeModeInit && NULL != swadgeModes[rtcMem.currentSwadgeMode]->fnEspNowRecvCb)
    {
        swadgeModes[rtcMem.currentSwadgeMode]->fnEspNowRecvCb(mac_addr, data, len, rssi);
    }
}

/**
 * TODO
 */
void ICACHE_FLASH_ATTR swadgeModeEspNowSendCb(uint8_t* mac_addr, mt_tx_status status)
{
    if(swadgeModeInit && NULL != swadgeModes[rtcMem.currentSwadgeMode]->fnEspNowSendCb)
    {
        swadgeModes[rtcMem.currentSwadgeMode]->fnEspNowSendCb(mac_addr, (mt_tx_status)status);
    }
}

/**
 * This deinitializes the current mode if it is initialized, displays the next
 * mode's LED pattern, and starts a timer to reboot into the next mode.
 * If the reboot timer is running, it will be reset
 */
void ICACHE_FLASH_ATTR incrementSwadgeModeNoSleep(void)
{
    // If the mode is initialized, tear it down
    if(swadgeModeInit)
    {
        // Call the exit callback for the current mode
        if(NULL != swadgeModes[rtcMem.currentSwadgeMode]->fnExitMode)
        {
            swadgeModes[rtcMem.currentSwadgeMode]->fnExitMode();
        }

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
 * Callback called when the swadge mode is changing. It displays and fades the
 * current mode number. When the mode fades all the way out, the swadge goes
 * into deep sleep and when it wakes it boots into the next mode
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
 * This task is constantly called by posting itself instead of being in an
 * infinite loop. ESP doesn't like infinite loops.
 *
 * It handles synchronous button events, audio samples which have been read
 * and are queued for processing, and calling CSTick()
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
}

/**
 * Timer handler for a software timer set to fire every 100ms, forever.
 * Calls CSTick() every 100ms.
 *
 * If the hardware is in wifi station mode, this Enables the hardware timer
 * to sample the ADC once the IP address has been received and printed
 *
 * Also handles logic for infrastructure wifi mode, which isn't being used
 *
 * TODO call this faster, but have each action still happen at 100ms (round robin)
 *
 * @param arg unused
 */
static void ICACHE_FLASH_ATTR timerFunc100ms(void* arg __attribute__((unused)))
{
    CSTick( 1 );

    if(swadgeModeInit && NULL != swadgeModes[rtcMem.currentSwadgeMode]->fnAccelerometerCallback)
    {
        accel_t accel = {0};
        MMA8452Q_poll(&accel);
        swadgeModes[rtcMem.currentSwadgeMode]->fnAccelerometerCallback(&accel);
    }

    // Tick the current mode every 100ms
    if(swadgeModeInit && NULL != swadgeModes[rtcMem.currentSwadgeMode]->fnTimerCallback)
    {
        swadgeModes[rtcMem.currentSwadgeMode]->fnTimerCallback();
    }

    StartHPATimer(); // Init the high speed ADC timer.
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
    SetupGPIO(NULL != swadgeModes[rtcMem.currentSwadgeMode]->fnAudioCallback);

    // Set up a timer to switch the swadge mode
    os_timer_disarm(&modeSwitchTimer);
    os_timer_setfn(&modeSwitchTimer, modeSwitchTimerFn, NULL);

    // Set the current WiFi mode based on what the swadge mode wants
    switch(swadgeModes[rtcMem.currentSwadgeMode]->wifiMode)
    {
        case ESP_NOW:
        {
            if(!(wifi_set_opmode_current( SOFTAP_MODE ) &&
                    wifi_set_opmode( SOFTAP_MODE )))
            {
                os_printf("Set SOFTAP_MODE before boot failed\r\n");
            }
            break;
        }
        case SOFT_AP:
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
        case ESP_NOW:
        {
            espNowInit();
            os_printf( "Booting in ESP-NOW\n" );
            break;
        }
        case SOFT_AP:
        case NO_WIFI:
        {
            os_printf( "Booting with no wifi\n" );
            break;
        }
    }

    // Common services pre-init
    CSPreInit();

    // Common services (wifi) init. Sets up another UDP server to receive
    // commands (issue_command)and an HTTP server
    CSInit(false);

    // Start a software timer to call CSTick() every 100ms and start the hw timer eventually
    os_timer_disarm(&timerHandle100ms);
    os_timer_setfn(&timerHandle100ms, (os_timer_func_t*)timerFunc100ms, NULL);
    os_timer_arm(&timerHandle100ms, 100, 1);

    // Only start the HPA timer if there's an audio callback
    if(NULL != swadgeModes[rtcMem.currentSwadgeMode]->fnAudioCallback)
    {
        StartHPATimer();
    }

    // Initialize LEDs
    ws2812_init();

    // Initialize i2c
    brzo_i2c_setup(100);

    // Initialize accel
    if(NULL != swadgeModes[rtcMem.currentSwadgeMode]->fnAccelerometerCallback)
    {
        MMA8452Q_setup();
        os_printf("MMA8452Q initialized\n");
    }

    // Initialize display
    begin(true);
    os_printf("OLED initialized\n");

    // Attempt to make ADC more stable
    // https:// github.com/esp8266/Arduino/issues/2070
    // see peripherals https:// espressif.com/en/support/explore/faq
    // wifi_set_sleep_type(NONE_SLEEP_T); // on its own stopped wifi working
    // wifi_fpm_set_sleep_type(NONE_SLEEP_T); // with this seemed no difference

    // Initialize the current mode
    if(NULL != swadgeModes[rtcMem.currentSwadgeMode]->modeName)
    {
        os_printf("mode: %d: %s\r\n", rtcMem.currentSwadgeMode, swadgeModes[rtcMem.currentSwadgeMode]->modeName);
    }
    else
    {
        os_printf("mode: %d: no name\r\n", rtcMem.currentSwadgeMode);
    }

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
    // Otherwise send out the LED data
    ws2812_push( (uint8_t*) ledData, ledDataLen );
    //os_printf("%s, %d LEDs\r\n", __func__, ledDataLen / 3);
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
