// Copyright 2015 <>< Charles Lohr Under the MIT/x11 License, NewBSD License or
// ColorChord License. You Choose.

/*============================================================================
 * Includes
 *==========================================================================*/

#include <osapi.h>
#include <user_interface.h>
#include <driver/uart.h>
#ifdef USE_ESP_GDB
    // To invoke gdb enter: xtensa-lx106-elf-gdb -x gdbstub/gdbcmds -b 115200
    // LEDs are disabled when debuging
    // Note to change mode, must first close term running the above, then push
    // button, then start up xtensa-lx106-elf-gdb -x gdbstub/gdbcmds -b 115200
    #include <../gdbstub/gdbstub.h>
#endif
#include "hsv_utils.h"

#include "ws2812_i2s.h"
#include "hpatimer.h"
#include "esp_niceness.h"
#include "gpio_user.h"
#include "buttons.h"
#include "nvm_interface.h"
#include "user_main.h"
#include "espNowUtils.h"
#include "cnlohr_i2c.h"
#include "oled.h"
#include "PartitionMap.h"
#include "QMA6981.h"
#include "synced_timer.h"
#include "printControl.h"

#include "mode_menu.h"
#include "mode_ddr.h"
#include "mode_colorchord.h"
#include "mode_personal_demon.h"
#include "mode_flappy.h"
#include "mode_flight.h"
#include "mode_raycaster.h"
#include "mode_rssi.h"

#include "ccconfig.h"

/*============================================================================
 * Defines
 *==========================================================================*/

#define PROC_TASK_PRIO 0
#define PROC_TASK_QUEUE_LEN 1

#define RTC_MEM_ADDR 64

/*============================================================================
 * Structs
 *==========================================================================*/

typedef struct __attribute__((aligned(4)))
{
    uint32_t currentSwadgeMode;
}
rtcMem_t;

/*============================================================================
 * Variables
 *==========================================================================*/

#if defined(FEATURE_ACCEL)
    static timer_t timerHandlePollAccel;
    bool QMA6981_init = false;
#endif

os_event_t procTaskQueue[PROC_TASK_QUEUE_LEN] = {{0}};

swadgeMode* swadgeModes[] =
{
    &menuMode,
    &raycasterMode,
    &flappyMode,
    &rssiMode,
    &flightMode,
    &personalDemonMode,
    &colorchordMode,
    &ddrMode,
};

bool swadgeModeInit = false;
rtcMem_t rtcMem = {0};

/*============================================================================
 * Prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR user_pre_init(void);
void ICACHE_FLASH_ATTR user_init(void);

static void ICACHE_FLASH_ATTR procTask(os_event_t* events);
#if defined(FEATURE_ACCEL)
    static void ICACHE_FLASH_ATTR pollAccel(void* arg);
    void ICACHE_FLASH_ATTR initializeAccelerometer(void);
#endif

#if !defined(FEATURE_OLED)
    void ICACHE_FLASH_ATTR incrementSwadgeMode(void);
#endif

/*============================================================================
 * Initialization Functions
 *==========================================================================*/

/**
 * Required function, must call system_partition_table_regist()
 */
void ICACHE_FLASH_ATTR user_pre_init(void)
{
#ifndef ALL_OS_PRINTF
    system_set_os_print(false);
#endif
    LoadDefaultPartitionMap();
}

/**
 * The main initialization function. This will be called when switching modes,
 * since mode switching is essentially a reboot
 */
void ICACHE_FLASH_ATTR user_init(void)
{
    // Initialize the UART
#ifdef USE_ESP_GDB
    // Only standard baud rates seem to be supported by xtensa gdb!
    // $ xtensa-lx106-elf-gdb -x gdbstub/gdbcmds -b 115200
    uart_init(BIT_RATE_115200, BIT_RATE_115200);
    gdbstub_init();
#else
    uart_init(BIT_RATE_74880, BIT_RATE_74880);
#endif

    INIT_PRINTF("\nSwadge 2021\n");

    // Read data fom RTC memory if we're waking from deep sleep
    if(REASON_DEEP_SLEEP_AWAKE == system_get_rst_info()->reason)
    {
        INIT_PRINTF("read rtc mem\n");
        // Try to read from rtc memory
        if(!system_rtc_mem_read(RTC_MEM_ADDR, &rtcMem, sizeof(rtcMem)))
        {
            // if it fails, zero it out instead
            ets_memset(&rtcMem, 0, sizeof(rtcMem));
            INIT_PRINTF("rtc mem read fail\n");
        }
    }
    else
    {
        // if it fails, zero it out instead
        ets_memset(&rtcMem, 0, sizeof(rtcMem));
        INIT_PRINTF("zero rtc mem\n");
    }

    // Set the current WiFi mode based on what the swadge mode wants
    switch(swadgeModes[rtcMem.currentSwadgeMode]->wifiMode)
    {
        case SWADGE_PASS:
        case ESP_NOW:
        {
            if(!(wifi_set_opmode_current( SOFTAP_MODE ) &&
                    wifi_set_opmode( SOFTAP_MODE )))
            {
                INIT_PRINTF("Set SOFTAP_MODE before boot failed\n");
            }
            espNowInit();
            INIT_PRINTF( "Booting in ESP-NOW\n" );
            break;
        }
        case WIFI_REGULAR:
            //App will control mode.
            break;
        default:
        case NO_WIFI:
        {
            if(!(wifi_set_opmode_current( NULL_MODE ) &&
                    wifi_set_opmode( NULL_MODE )))
            {
                INIT_PRINTF("Set NULL_MODE before boot failed\n");
            }
            INIT_PRINTF( "Booting with no wifi\n" );
            break;
        }
    }

    // Load configurable parameters from SPI memory
    LoadSettings();

    if(SWADGE_PASS != swadgeModes[rtcMem.currentSwadgeMode]->wifiMode)
    {
        // Initialize GPIOs
        SetupGPIO();

#ifndef USE_ESP_GDB
        // Initialize LEDs
        ws2812_init();
        // Turn LEDs off
        led_t leds[NUM_LIN_LEDS] = {{0}};
        setLeds(leds, sizeof(leds));
        INIT_PRINTF("LEDs initialized\n");
#endif

#if defined(FEATURE_OLED) || defined(FEATURE_ACCEL)
        // Initialize i2c
        cnlohr_i2c_setup(100);
        INIT_PRINTF("I2C initialized\n");
#endif

#if defined(FEATURE_ACCEL)
        // Initialize accel
        initializeAccelerometer();

        if(NULL != swadgeModes[rtcMem.currentSwadgeMode]->fnAccelerometerCallback)
        {
            // Start a software timer to run every 100ms
            timerDisarm(&timerHandlePollAccel);
            timerSetFn(&timerHandlePollAccel, pollAccel, NULL);
            timerArm(&timerHandlePollAccel, 100, 1);
        }
#endif

#if defined(FEATURE_OLED)
        // Initialize display
        if(true == initOLED(true))
        {
            INIT_PRINTF("OLED initialized\n");
        }
        else
        {
            INIT_PRINTF("OLED initialization failed\n");
        }
#endif

#if defined(FEATURE_MIC)
        // Initialize either the buzzer or the mic
        if(NULL != swadgeModes[rtcMem.currentSwadgeMode]->fnAudioCallback)
        {
            initMic();
        }
#endif

#if defined(FEATURE_BZR)
        INIT_PRINTF("Init Buzzer\n");
        // Initialize the buzzer
        initBuzzer();
        setBuzzerNote(SILENCE);
#endif
    }

    // Initialize the current mode
    if(NULL != swadgeModes[rtcMem.currentSwadgeMode]->fnEnterMode)
    {
        swadgeModes[rtcMem.currentSwadgeMode]->fnEnterMode();
    }
    swadgeModeInit = true;

    // Debug print
    INIT_PRINTF("mode: %d: %s initialized\n", rtcMem.currentSwadgeMode,
                (NULL != swadgeModes[rtcMem.currentSwadgeMode]->modeName) ?
                (swadgeModes[rtcMem.currentSwadgeMode]->modeName) : ("No Name"));

    // Add a process to filter queued ADC samples and output LED signals
    // This is faster than every 100ms
    system_os_task(procTask, PROC_TASK_PRIO, procTaskQueue, PROC_TASK_QUEUE_LEN);
    system_os_post(PROC_TASK_PRIO, 0, 0 );
}

/*============================================================================
 * Looping Functions
 *==========================================================================*/

/**
 * This task is constantly called by posting itself instead of being in an
 * infinite loop. ESP doesn't like infinite loops.
 *
 * It handles synchronous button events and audio samples which have been read
 * and are queued for processing
 *
 * @param events Checked before posting this task again
 */
static void ICACHE_FLASH_ATTR procTask(os_event_t* events __attribute__((unused)))
{
    // Post another task to this thread
    system_os_post(PROC_TASK_PRIO, 0, 0 );

    // Process queued button presses synchronously
    HandleButtonEventSynchronous();

#if defined(FEATURE_MIC)
    // While there are samples available from the ADC
    while( sampleAvailable() )
    {
        // Get the sample
        int32_t samp = getSample();
        // Run the sample through an IIR filter
        static uint32_t samp_iir = 0;
        samp_iir = samp_iir - (samp_iir >> 10) + samp;
        samp = (samp - (samp_iir >> 10)) * 16;
        // Amplify the sample
        samp = (samp * CCS.gINITIAL_AMP) >> 4;

        // Pass the sample to the mode
        if(swadgeModeInit && NULL != swadgeModes[rtcMem.currentSwadgeMode]->fnAudioCallback)
        {
            swadgeModes[rtcMem.currentSwadgeMode]->fnAudioCallback(samp);
        }
    }
#endif

    // Process all the synchronous timers
    timersCheck();

    // Call this mode's procTask function, if it exists
    if(swadgeModeInit && NULL != swadgeModes[rtcMem.currentSwadgeMode]->fnProcTask)
    {
        swadgeModes[rtcMem.currentSwadgeMode]->fnProcTask();
    }

#if defined(FEATURE_OLED)
    // Track if the full frame, or difference should be drawn
    static bool shouldDrawDifference = true;

    // Cap the display updates at 30fps
    static uint32_t lastDrawTime = 0;
    if(system_get_time() - lastDrawTime > 33333)
    {
        bool forceFullUpdate = false;

        lastDrawTime = system_get_time();

        if(swadgeModeInit && NULL != swadgeModes[rtcMem.currentSwadgeMode]->fnRenderTask)
        {
            forceFullUpdate = swadgeModes[rtcMem.currentSwadgeMode]->fnRenderTask();
        }

        // If we should draw the whole frame, reinit the OLED first
        if(false == shouldDrawDifference)
        {
            initOLED(true);
        }

        // Draw either the whole frame, or just the difference
        switch(updateOLED(shouldDrawDifference && !forceFullUpdate))
        {
            case FRAME_DRAWN:
            {
                // Draw was successful, draw a difference the next time
                shouldDrawDifference = true;
                break;
            }
            case FRAME_NOT_DRAWN:
            {
                // Draw was not successful, reset the OLED and
                // draw a full frame next time
                shouldDrawDifference = false;
                break;
            }
            default:
            case NOTHING_TO_DO:
            {
                break;
            }
        }
    }
#endif
}

#if defined(FEATURE_ACCEL)
/**
 * @brief Polls the accelerometer every 100ms
 *
 * @param arg unused
 */
static void ICACHE_FLASH_ATTR pollAccel(void* arg __attribute__((unused)))
{
    if(swadgeModeInit && NULL != swadgeModes[rtcMem.currentSwadgeMode]->fnAccelerometerCallback)
    {
        accel_t accel = {0};
        if(true == QMA6981_init)
        {
            QMA6981_poll(&accel);
        }
        else
        {
            // Initialization failed, but the accel is necessary. Try again.
            initializeAccelerometer();
        }

#if SWADGE_VERSION == SWADGE_BBKIWI
        int16_t xarrow = TOPOLED;
        int16_t yarrow = LEFTOLED;
        int16_t zarrow = FACEOLED;
        accel.x = xarrow;
        accel.y = yarrow;
        accel.z = zarrow;
#elif SWADGE_VERSION == SWADGE_2019
        // put code to return random, specific periods, or L/R button presses
        accel.x = 0;
        accel.y = 0;
        accel.z = 255;
#endif

        swadgeModes[rtcMem.currentSwadgeMode]->fnAccelerometerCallback(&accel);
    }
}
#endif

/*============================================================================
 * Interrupt Functions
 *==========================================================================*/

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

/*============================================================================
 * Swadge Mode Utility Functions
 *==========================================================================*/

/**
 * This deinitializes the current mode if it is initialized, displays the next
 * mode's LED pattern, and starts a timer to reboot into the next mode.
 * If the reboot timer is running, it will be reset
 *
 */
#if defined(FEATURE_OLED)
    void ICACHE_FLASH_ATTR switchToSwadgeMode(uint8_t newMode)
#else
    void ICACHE_FLASH_ATTR incrementSwadgeMode(void)
#endif
{
    // If the mode is initialized, tear it down
    if(swadgeModeInit)
    {
        // Turn LEDs off
        led_t leds[NUM_LIN_LEDS] = {{0}};
        setLeds(leds, sizeof(leds));
        // Call the exit callback for the current mode
        if(NULL != swadgeModes[rtcMem.currentSwadgeMode]->fnExitMode)
        {
            swadgeModes[rtcMem.currentSwadgeMode]->fnExitMode();
        }

        // Clean up ESP NOW if that's where we were at
        switch(swadgeModes[rtcMem.currentSwadgeMode]->wifiMode)
        {
            case SWADGE_PASS:
            case ESP_NOW:
            {
                espNowDeinit();
                break;
            }
            case WIFI_REGULAR:
            {
                wifi_set_opmode( NULL_MODE );
                break;
            }
            default:
            case NO_WIFI:
            {
                break;
            }
        }
        swadgeModeInit = false;
    }

    // Switch to the next mode, or start from the beginning if we're at the end
#if defined(FEATURE_OLED)
    rtcMem.currentSwadgeMode = newMode;
#else
    rtcMem.currentSwadgeMode = (rtcMem.currentSwadgeMode + 1) % (sizeof(swadgeModes) / sizeof(swadgeModes[0]));
#endif

#if defined(EMU)
    system_set_rst_reason(REASON_DEEP_SLEEP_AWAKE);
    system_rtc_mem_write(RTC_MEM_ADDR, &rtcMem, sizeof(rtcMem));
    user_init();
#else
    enterDeepSleep(swadgeModes[rtcMem.currentSwadgeMode]->wifiMode, 1000);
#endif
}

#if defined(EMU)
/**
 * @brief Helper function for the emulator to clean up the current mode
 */
void ICACHE_FLASH_ATTR exitCurrentSwadgeMode(void)
{
    if(NULL != swadgeModes[rtcMem.currentSwadgeMode]->fnExitMode)
    {
        swadgeModes[rtcMem.currentSwadgeMode]->fnExitMode();
    }
#if defined(FEATURE_ACCEL)
    timerDisarm(&timerHandlePollAccel);
#endif
    timersCheck();
}
#endif

/**
 * Enter deep sleep for timeUs microseconds. Perform radio calibration depending
 * on the wifi mode when it wakes
 *
 * @param wifiMode The wifi mode when the ESP wakes up
 * @param timeUs   The time to sleep for
 */
void ICACHE_FLASH_ATTR enterDeepSleep(wifiMode_t wifiMode, uint32_t timeUs)
{
    // Write the RTC memory so it knows what mode to be in when waking up
    system_rtc_mem_write(RTC_MEM_ADDR, &rtcMem, sizeof(rtcMem));

    // Be extra sure the GPIOs are in the right state for boot
    setGpiosForBoot();

    // Check if the next mode wants wifi or not
    switch(wifiMode)
    {
        case SWADGE_PASS:
        {
            // The chip will not perform the RF calibration during waking up from
            // Deep-sleep. Power consumption is low.
            system_deep_sleep_set_option(2);
            // Sleeeeep. It calls user_init() on wake, which will use the new mode
            system_deep_sleep_instant(timeUs);
            break;
        }
        case WIFI_REGULAR:
        case ESP_NOW:
        {
            // Radio calibration is done after deep-sleep wake up; this increases
            // the current consumption.
            system_deep_sleep_set_option(1);
            // Sleeeeep. It calls user_init() on wake, which will use the new mode
            system_deep_sleep(timeUs);
            break;
        }
        default:
        case NO_WIFI:
        {
            // Disable RF after deep-sleep wake up, just like modem sleep; this
            // has the least current consumption; the device is not able to
            // transmit or receive data after wake up.
            system_deep_sleep_set_option(4);
            // Sleeeeep. It calls user_init() on wake, which will use the new mode
            system_deep_sleep(timeUs);
            break;
        }
    }
}

/**
 * Return the array of swadge mode pointers through a parameter and the number
 * of modes through the return value
 *
 * @return the number of swadge modes
 */
uint8_t ICACHE_FLASH_ATTR getSwadgeModes(swadgeMode***  modePtr)
{
    *modePtr = swadgeModes;
    return (sizeof(swadgeModes) / sizeof(swadgeModes[0]));
}

#if defined(FEATURE_ACCEL)
/**
 * @brief Set the time between accelerometer polls
 *
 * @param drawTimeMs
 */
void ICACHE_FLASH_ATTR setAccelPollTime(uint32_t pollTimeMs)
{
    timerDisarm(&timerHandlePollAccel);
    timerArm(&timerHandlePollAccel, pollTimeMs, true);
}

/**
 * Attempt to initialize the accelerometers. See what we get
 */
void ICACHE_FLASH_ATTR initializeAccelerometer(void)
{
    // Initialize accel
    if(NULL != swadgeModes[rtcMem.currentSwadgeMode]->fnAccelerometerCallback)
    {
        if(true == QMA6981_setup())
        {
            QMA6981_init = true;
            INIT_PRINTF("QMA6981 initialized\n");
        }
        else
        {
            INIT_PRINTF("QMA6981 initialization failed\n");
        }
    }
    else
    {
        INIT_PRINTF("QMA6981 not needed\n");
    }
}
#endif

/*============================================================================
 * Swadge Mode Callback Functions
 *==========================================================================*/

/**
 * Pass a button event from the logic in button.h to the active swadge mode
 * It routes through user_main.c, which knows what the current mode is
 *
 * @param state A bitmask of all button statuses
 * @param button  The button number which was pressed
 * @param down 1 if the button was pressed, 0 if it was released
 */
void ICACHE_FLASH_ATTR swadgeModeButtonCallback(uint8_t state, int button, int down)
{
    if(swadgeModeInit && NULL != swadgeModes[rtcMem.currentSwadgeMode]->fnButtonCallback)
    {
        // Pass the button event to the mode
        swadgeModes[rtcMem.currentSwadgeMode]->fnButtonCallback(state, button, down);
    }
}

/**
 * Callback from ESP NOW to the current Swadge mode whenever a packet is received
 * It routes through user_main.c, which knows what the current mode is
 */
void ICACHE_FLASH_ATTR swadgeModeEspNowRecvCb(uint8_t* mac_addr, uint8_t* data, uint8_t len, uint8_t rssi)
{
    if(swadgeModeInit && NULL != swadgeModes[rtcMem.currentSwadgeMode]->fnEspNowRecvCb)
    {
        swadgeModes[rtcMem.currentSwadgeMode]->fnEspNowRecvCb(mac_addr, data, len, rssi);
    }
}

/**
 * Callback from ESP NOW to the current Swadge mode whenever a packet is sent
 * It routes through user_main.c, which knows what the current mode is
 */
void ICACHE_FLASH_ATTR swadgeModeEspNowSendCb(uint8_t* mac_addr, mt_tx_status status)
{
    ENOW_PRINTF("%s::%d\n", __func__, __LINE__);
    if(swadgeModeInit && NULL != swadgeModes[rtcMem.currentSwadgeMode]->fnEspNowSendCb)
    {
        ENOW_PRINTF("%s::%d\n", __func__, __LINE__);
        swadgeModes[rtcMem.currentSwadgeMode]->fnEspNowSendCb(mac_addr, (mt_tx_status)status);
    }
}

/*============================================================================
 * LED Utility Functions
 *==========================================================================*/

/**
 * Set the state of the six GRB LEDs, but don't overwrite if the LEDs were
 * set via UDP for at least TICKER_TIMEOUT increments of 100ms
 *
 * @param ledData Array of LED color data. Every three bytes corresponds to
 * one LED in GRB order. So index 0 is LED1_G, index 1 is
 * LED1_R, index 2 is LED1_B, index 3 is LED2_G, etc.
 * @param ledDataLen The length of buffer, most likely 6*3
 */
void ICACHE_FLASH_ATTR setLeds(led_t* ledData, uint16_t ledDataLen)
{
    ws2812_push( (uint8_t*) ledData, ledDataLen );
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
void ICACHE_FLASH_ATTR showLedCount(uint8_t num, uint32_t colorToShow)
{
    led_t leds[6] = {{0}};

    led_t rgb;
    rgb.r = (colorToShow >> 16) & 0xFF;
    rgb.g = (colorToShow >>  8) & 0xFF;
    rgb.b = (colorToShow >>  0) & 0xFF;

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
        default:
        {
            break;
        }
    }

    // Draw the LEDs
    setLeds(leds, sizeof(leds));
}
