// Copyright 2015 <>< Charles Lohr Under the MIT/x11 License, NewBSD License or
// ColorChord License. You Choose.

/*============================================================================
 * Includes
 *==========================================================================*/

#include <osapi.h>
#include <user_interface.h>
#include <driver/uart.h>

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
#include "MMA8452Q.h"

#include "mode_menu.h"
#include "mode_reflector_game.h"
#include "mode_random_d6.h"
#include "mode_dance.h"
#include "mode_demo.h"
#include "mode_snake.h"

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
static os_timer_t timerHandleUpdateDisplay = {0};

os_event_t procTaskQueue[PROC_TASK_QUEUE_LEN] = {{0}};

swadgeMode* swadgeModes[] =
{
    &menuMode, // Menu must be the first
    &snakeMode,
    &demoMode,
    &reflectorGameMode,
    &dancesMode,
    &randomD6Mode,
};
bool swadgeModeInit = false;
rtcMem_t rtcMem = {0};

bool MMA8452Q_init = false;
bool QMA6981_init = false;

uint8_t menuChangeBarProgress = 0;

/*============================================================================
 * Prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR user_pre_init(void);
void ICACHE_FLASH_ATTR user_init(void);

static void ICACHE_FLASH_ATTR procTask(os_event_t* events);
static void ICACHE_FLASH_ATTR updateDisplay(void* arg);
static void ICACHE_FLASH_ATTR pollAccel(void* arg);

static void ICACHE_FLASH_ATTR drawChangeMenuBar(void);

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
    uart_init(BIT_RATE_74880, BIT_RATE_74880);
    os_printf("\nSwadge 2020\n");

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

    // Load configurable parameters from SPI memory
    LoadSettings();

    // Initialize GPIOs
    SetupGPIO(false);
#ifdef PROFILE
    GPIO_OUTPUT_SET(GPIO_ID_PIN(0), 0);
#endif

    // Initialize LEDs
    ws2812_init();
    os_printf("LEDs initialized\n");

    // Initialize i2c
    brzo_i2c_setup(100);
    os_printf("I2C initialized\n");

    // Initialize accel
    if(NULL != swadgeModes[rtcMem.currentSwadgeMode]->fnAccelerometerCallback)
    {
        if(true == MMA8452Q_setup())
        {
            MMA8452Q_init = true;
            os_printf("MMA8452Q initialized\n");
        }
        else
        {
            os_printf("MMA8452Q initialization failed\n");
        }
    }
    else
    {
        os_printf("MMA8452Q not needed\n");
    }

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

    if(QMA6981_init || MMA8452Q_init)
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

        // Start a software timer to run every 100ms
        os_timer_disarm(&timerHandleUpdateDisplay);
        os_timer_setfn(&timerHandleUpdateDisplay, (os_timer_func_t*)updateDisplay, NULL);
        os_timer_arm(&timerHandleUpdateDisplay, 100, 1);
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
    }
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
        if(true == MMA8452Q_init)
        {
            MMA8452Q_poll(&accel);
        }
        else if(true == QMA6981_init)
        {
            QMA6981_poll(&accel);
        }
        swadgeModes[rtcMem.currentSwadgeMode]->fnAccelerometerCallback(&accel);
    }
}

/**
 * @brief Updated the OLED display every 100ms
 *
 * @param arg unused
 */
static void ICACHE_FLASH_ATTR updateDisplay(void* arg __attribute__((unused)))
{
    // Draw the menu change bar if necessary
    drawChangeMenuBar();

    // Update the display
    updateOLED();
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

/**
 * This deinitializes the current mode if it is initialized, displays the next
 * mode's LED pattern, and starts a timer to reboot into the next mode.
 * If the reboot timer is running, it will be reset
 *
 * @param newMode The index of the new mode
 */
void ICACHE_FLASH_ATTR switchToSwadgeMode(uint8_t newMode)
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
    rtcMem.currentSwadgeMode = newMode;

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

/**
 * Draw a progress bar on the bottom of the display if the menu button is being held.
 * When the bar fills the display, reset the mode back to the menu
 */
#define BAR_INCREMENT 10
void ICACHE_FLASH_ATTR drawChangeMenuBar(void)
{
    if(0 < menuChangeBarProgress)
    {
        // Clear the bottom bar
        fillDisplayArea(0, OLED_HEIGHT - 1, OLED_WIDTH - 1, OLED_HEIGHT - 1, BLACK);
        // Draw the menu change progress bar
        fillDisplayArea(0, OLED_HEIGHT - 1, menuChangeBarProgress, OLED_HEIGHT - 1, WHITE);
        // Increment the progress for next time
        menuChangeBarProgress += BAR_INCREMENT;

        // If it was held for long enough
        if(menuChangeBarProgress >= 131)
        {
            // Stop bar so will only get here once
            menuChangeBarProgress = 0;
            // Go back to the menu
            switchToSwadgeMode(0);
        }
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
        // If the menu button was pressed
        if(0 == rtcMem.currentSwadgeMode)
        {
            // Only for the menu mode, pass the button event to the mode
            swadgeModes[rtcMem.currentSwadgeMode]->fnButtonCallback(state, button, down);
        }
        else if(down)
        {
            // Start drawing the progress bar
            menuChangeBarProgress = 1;
        }
        else
        {
            // If it was released, stop drawing the progress bar and clear it
            fillDisplayArea(0, OLED_HEIGHT - 1, menuChangeBarProgress, OLED_HEIGHT - 1, BLACK);
            menuChangeBarProgress = 0;
        }
    }
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
