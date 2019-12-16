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
#include "embeddedout.h"

#include "ws2812_i2s.h"
#include "hpatimer.h"
#include "ccconfig.h"
#include "gpio_user.h"
#include "buttons.h"
#include "custom_commands.h"
#include "user_main.h"
#include "espNowUtils.h"
#include "brzo_i2c.h"
#include "oled.h"
#include "PartitionMap.h"
#include "QMA6981.h"

#include "mode_menu.h"
#include "mode_joust_game.h"
#include "mode_snake.h"
#include "mode_gallery.h"
#include "mode_tiltrads.h"
#include "mode_mazerf.h"
#include "mode_color_movement.h"
#ifdef TEST_MODE
    #include "mode_test.h"
#endif
#include "mode_music.h"

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

static os_timer_t timerHandlePollAccel = {0};
static os_timer_t timerHandleReturnToMenu = {0};

os_event_t procTaskQueue[PROC_TASK_QUEUE_LEN] = {{0}};

swadgeMode* swadgeModes[] =
{
    /* This MUST NOT be defined for production firmware. It should only be
     * enabled from the makefile when building test firmware for manufacturing.
     * It uses all hardware features to quickly validate if hardware is functional.
     * This comes before menuMode so the menu does not have to be navigated.
     */
#ifdef TEST_MODE
    &testMode,
#endif
    /* SWADGE_2019 doesn't have an OLED, so this is useless.
     * For all other swadges, it comes first so the swadge boots into the menu.
     */
#if SWADGE_VERSION != SWADGE_2019
    &menuMode,
#endif
    /* These are the modes which are displayed in the menu */
    &tiltradsMode,
    &snakeMode,
    &joustGameMode,
    &mazerfMode,
    /* SWADGE_2019 doesn't have a buzzer either */
#if SWADGE_VERSION != SWADGE_2019
    &muteOptionOff,
#endif
    &galleryMode,
    &colorMoveMode,
    &musicMode,
};

bool swadgeModeInit = false;
rtcMem_t rtcMem = {0};

bool QMA6981_init = false;

/*============================================================================
 * Prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR user_pre_init(void);
void ICACHE_FLASH_ATTR user_init(void);

static void ICACHE_FLASH_ATTR procTask(os_event_t* events);
static void ICACHE_FLASH_ATTR pollAccel(void* arg);
void ICACHE_FLASH_ATTR initializeAccelerometer(void);
static void ICACHE_FLASH_ATTR returnToMenuTimerFunc(void* arg);

#if SWADGE_VERSION == SWADGE_2019
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

    os_printf("\nSwadge %d\n", SWADGE_VERSION);

    // Read data fom RTC memory if we're waking from deep sleep
    if(REASON_DEEP_SLEEP_AWAKE == system_get_rst_info()->reason)
    {
        os_printf("read rtc mem\n");
        // Try to read from rtc memory
        if(!system_rtc_mem_read(RTC_MEM_ADDR, &rtcMem, sizeof(rtcMem)))
        {
            // if it fails, zero it out instead
            ets_memset(&rtcMem, 0, sizeof(rtcMem));
            os_printf("rtc mem read fail\n");
        }
    }
    else
    {
        // if it fails, zero it out instead
        ets_memset(&rtcMem, 0, sizeof(rtcMem));
        os_printf("zero rtc mem\n");
    }

    // Set the current WiFi mode based on what the swadge mode wants
    switch(swadgeModes[rtcMem.currentSwadgeMode]->wifiMode)
    {
        case ESP_NOW:
        {
            if(!(wifi_set_opmode_current( SOFTAP_MODE ) &&
                    wifi_set_opmode( SOFTAP_MODE )))
            {
                os_printf("Set SOFTAP_MODE before boot failed\n");
            }
            espNowInit();
            os_printf( "Booting in ESP-NOW\n" );
            break;
        }
        default:
        case SOFT_AP:
        case NO_WIFI:
        {
            if(!(wifi_set_opmode_current( NULL_MODE ) &&
                    wifi_set_opmode( NULL_MODE )))
            {
                os_printf("Set NULL_MODE before boot failed\n");
            }
            os_printf( "Booting with no wifi\n" );
            break;
        }
    }

#ifdef COLORCHORD_DFT
    // Sets up gConfigs in custom_commands.c
    // Could do major refactor to eliminate
    PopulategConfigs();
#endif
    // Load configurable parameters from SPI memory
    LoadSettings();

    // Initialize GPIOs
    SetupGPIO(false);
#ifdef PROFILE
    GPIO_OUTPUT_SET(GPIO_ID_PIN(0), 0);
#endif

    // Initialize LEDs
#ifndef USE_ESP_GDB
    ws2812_init();
    os_printf("LEDs initialized\n");
#endif

    // Initialize i2c
    brzo_i2c_setup(100);
    os_printf("I2C initialized\n");

    // Initialize accel
    initializeAccelerometer();

#if SWADGE_VERSION != SWADGE_2019
    if(NULL != swadgeModes[rtcMem.currentSwadgeMode]->fnAccelerometerCallback)
#else
    if(true)
#endif
    {
        // Start a software timer to run every 100ms
        os_timer_disarm(&timerHandlePollAccel);
        os_timer_setfn(&timerHandlePollAccel, (os_timer_func_t*)pollAccel, NULL);
        os_timer_arm(&timerHandlePollAccel, 100, 1);
    }

    // Initialize display
    if(true == initOLED(true))
    {
        os_printf("OLED initialized\n");
    }
    else
    {
        os_printf("OLED initialization failed\n");
    }

    // Start the HPA timer, used for PWMing the buzzer
    StartHPATimer();

    // Initialize the buzzer
    initBuzzer();
    setBuzzerNote(SILENCE);

    // Turn LEDs off
    led_t leds[NUM_LIN_LEDS] = {{0}};
    setLeds(leds, sizeof(leds));

    // Initialize the current mode
    if(NULL != swadgeModes[rtcMem.currentSwadgeMode]->fnEnterMode)
    {
        swadgeModes[rtcMem.currentSwadgeMode]->fnEnterMode();
    }
    swadgeModeInit = true;

    // Debug print
    os_printf("mode: %d: %s initialized\n", rtcMem.currentSwadgeMode,
              (NULL != swadgeModes[rtcMem.currentSwadgeMode]->modeName) ?
              (swadgeModes[rtcMem.currentSwadgeMode]->modeName) : ("No Name"));

    // Add a process to filter queued ADC samples and output LED signals
    // This is faster than every 100ms
    system_os_task(procTask, PROC_TASK_PRIO, procTaskQueue, PROC_TASK_QUEUE_LEN);
    system_os_post(PROC_TASK_PRIO, 0, 0 );

    // Setup a software timer to return to the menu
    os_timer_disarm(&timerHandleReturnToMenu);
    os_timer_setfn(&timerHandleReturnToMenu, (os_timer_func_t*)returnToMenuTimerFunc, NULL);
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

    // For profiling so we can see how much CPU is spent in this loop.
#ifdef PROFILE
    WRITE_PERI_REG( PERIPHS_GPIO_BASEADDR + GPIO_ID_PIN(0), 1 );
#endif

    // Process queued button presses synchronously
    HandleButtonEventSynchronous();

    // Update the display as fast as possible.
    // This only sends I2C data if there was some pixel change
    updateOLED();

#ifdef PROFILE
    WRITE_PERI_REG( PERIPHS_GPIO_BASEADDR + GPIO_ID_PIN(0), 0 );
#endif
}

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
#endif
#if SWADGE_VERSION == SWADGE_2019
        //TODO put code to return random, specific periods, or L/R button presses
        accel.x = 0;
        accel.y = 0;
        accel.z = 255;
#endif

        swadgeModes[rtcMem.currentSwadgeMode]->fnAccelerometerCallback(&accel);
    }
}

/**
 * @brief Function called on a 10ms timer when the return menu bar is being drawn
 *
 * @param arg unused
 */
static void ICACHE_FLASH_ATTR returnToMenuTimerFunc(void* arg __attribute__((unused)))
{
    // If the bar is full
    if(128 == incrementMenuBar())
    {
        // Go back to the menu
        switchToSwadgeMode(0);
        os_timer_disarm(&timerHandleReturnToMenu);
    }
}

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
#if SWADGE_VERSION == SWADGE_2019
void ICACHE_FLASH_ATTR switchToSwadgeMode(uint8_t newMode)
{
    (void) newMode;
}
#endif

/**
 * This deinitializes the current mode if it is initialized, displays the next
 * mode's LED pattern, and starts a timer to reboot into the next mode.
 * If the reboot timer is running, it will be reset
 *
 */
#if SWADGE_VERSION != SWADGE_2019
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
            case ESP_NOW:
            {
                espNowDeinit();
                break;
            }
            default:
            case SOFT_AP:
            case NO_WIFI:
            {
                break;
            }
        }
        swadgeModeInit = false;
    }

    // Switch to the next mode, or start from the beginning if we're at the end
#if SWADGE_VERSION != SWADGE_2019
    rtcMem.currentSwadgeMode = newMode;
#else
    rtcMem.currentSwadgeMode = (rtcMem.currentSwadgeMode + 1) % (sizeof(swadgeModes) / sizeof(swadgeModes[0]));
#endif
    // Write the RTC memory so it knows what mode to be in when waking up
    system_rtc_mem_write(RTC_MEM_ADDR, &rtcMem, sizeof(rtcMem));
    os_printf("rtc mem written\n");

    // Check if the next mode wants wifi or not
    switch(swadgeModes[rtcMem.currentSwadgeMode]->wifiMode)
    {
        case ESP_NOW:
        {
            // Radio calibration is done after deep-sleep wake up; this increases
            // the current consumption.
            system_deep_sleep_set_option(1);
            os_printf("deep sleep option set 1\n");
            break;
        }
        default:
        case SOFT_AP:
        case NO_WIFI:
        {
            // Disable RF after deep-sleep wake up, just like modem sleep; this
            // has the least current consumption; the device is not able to
            // transmit or receive data after wake up.
            system_deep_sleep_set_option(4);
            os_printf("deep sleep option set 4\n");
            break;
        }
    }

    // Be extra sure the GPIOs are in the right state for boot
    setGpiosForBoot();

    // Sleeeeep. It calls user_init() on wake, which will use the new mode
    system_deep_sleep(1000);
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

#if SWADGE_VERSION != SWADGE_2019

/**
 * @brief Set the time between accelerometer polls
 *
 * @param drawTimeMs
 */
void ICACHE_FLASH_ATTR setAccelPollTime(uint32_t pollTimeMs)
{
    os_timer_disarm(&timerHandlePollAccel);
    os_timer_arm(&timerHandlePollAccel, pollTimeMs, true);
}

#else
void ICACHE_FLASH_ATTR setAccelPollTime(uint32_t pollTimeMs)
{
}
#endif

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
            os_printf("QMA6981 initialized\n");
        }
        else
        {
            os_printf("QMA6981 initialization failed\n");
        }
    }
    else
    {
        os_printf("QMA6981 not needed\n");
    }
}

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
    if(0 == button)
    {
#if SWADGE_VERSION != SWADGE_2019
        // If the menu button was pressed
        if(0 == rtcMem.currentSwadgeMode)
        {
            // Only for the menu mode, pass the button event to the mode
            swadgeModes[rtcMem.currentSwadgeMode]->fnButtonCallback(state, button, down);
        }
        else if(down)
        {
            // Start drawing the progress bar
            os_timer_arm(&timerHandleReturnToMenu, 10, true);
        }
        else
        {
            // If it was released, stop drawing the progress bar and clear it
            os_timer_disarm(&timerHandleReturnToMenu);
            zeroMenuBar();
        }
#else
        // Switch the mode
        incrementSwadgeMode();
#endif
        // And also pass this to the music mode, and only the music mode
        if(8 == rtcMem.currentSwadgeMode &&
                swadgeModeInit && NULL != swadgeModes[rtcMem.currentSwadgeMode]->fnButtonCallback)
        {
            swadgeModes[rtcMem.currentSwadgeMode]->fnButtonCallback(state, button, down);
        }
    }
    //NOTE for 2020 button 0 can only be used for menu, if want to be able to use momentary press in other modes
    //     change 'else if' to 'if'
    else if(swadgeModeInit && NULL != swadgeModes[rtcMem.currentSwadgeMode]->fnButtonCallback)
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
    if(swadgeModeInit && NULL != swadgeModes[rtcMem.currentSwadgeMode]->fnEspNowSendCb)
    {
        swadgeModes[rtcMem.currentSwadgeMode]->fnEspNowSendCb(mac_addr, (mt_tx_status)status);
    }
}

/*============================================================================
 * LED Utility Functions
 *==========================================================================*/

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
    ws2812_push( (uint8_t*) ledData, ledDataLen );
    //os_printf("%s, %d LEDs\n", __func__, ledDataLen / 3);
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
