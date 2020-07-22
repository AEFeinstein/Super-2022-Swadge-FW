/*============================================================================
 * Includes
 *==========================================================================*/

#include <osapi.h>
#include <gpio.h>
#include "gpio_user.h"
#include "buttons.h"
#include "printControl.h"

/*============================================================================
 * Defines
 *==========================================================================*/

#define PIN_DIR_OUTPUT ( *((uint32_t*)0x60000310) )
#define PIN_DIR_INPUT ( *((uint32_t*)0x60000314) )
#define PIN_IN        ( *((volatile uint32_t*)0x60000318) )

#define lengthof(x) (sizeof(x) / sizeof(x[0]))

/*============================================================================
 * Prototypes
 *==========================================================================*/

void gpioInterrupt( void* v );

/*============================================================================
 * Structs
 *==========================================================================*/

typedef struct
{
    uint8_t GPID;
    uint8_t func;
    int periph;
    bool initialState;
} gpioInfo_t;

/*============================================================================
 * Variables
 *==========================================================================*/

volatile uint8_t LastGPIOState;
bool mBuzzerState = false;

// Matches order in button_mask
static const gpioInfo_t gpioInfoInput[] =
{
#if SWADGE_VERSION == SWADGE_2019
    // Up
    {
        .GPID = 13,
        .func = FUNC_GPIO13,
        .periph = PERIPHS_IO_MUX_MTCK_U,
        .initialState = 1
    },
    // Left
    {
        .GPID = 0,
        .func = FUNC_GPIO0,
        .periph = PERIPHS_IO_MUX_GPIO0_U
    },
    // Right
    {
        .GPID = 2,
        .func = FUNC_GPIO2,
        .periph = PERIPHS_IO_MUX_GPIO2_U
    },
#elif (SWADGE_VERSION == BARREL_1_0_0)
    // Menu
    {
        .GPID = 12,
        .func = FUNC_GPIO12,
        .periph = PERIPHS_IO_MUX_MTDI_U,
        .initialState = 1
    },
    // Left
    {
        .GPID = 13,
        .func = FUNC_GPIO13,
        .periph = PERIPHS_IO_MUX_MTCK_U,
        .initialState = 1
    },
    // Right
    {
        .GPID = 4,
        .func = FUNC_GPIO4,
        .periph = PERIPHS_IO_MUX_GPIO4_U,
        .initialState = 1
    },
#elif (SWADGE_VERSION == SWADGE_CHAINSAW)
    // Left
    {
        .GPID = 4,
        .func = FUNC_GPIO4,
        .periph = PERIPHS_IO_MUX_GPIO4_U,
        .initialState = 1
    },
    // Down
    {
        .GPID = 5,
        .func = FUNC_GPIO5,
        .periph = PERIPHS_IO_MUX_GPIO5_U,
        .initialState = 1
    },
    // Right
    {
        .GPID = 12,
        .func = FUNC_GPIO12,
        .periph = PERIPHS_IO_MUX_MTDI_U,
        .initialState = 1
    },
    // Up
    {
        .GPID = 13,
        .func = FUNC_GPIO13,
        .periph = PERIPHS_IO_MUX_MTCK_U,
        .initialState = 1
    },
    // Select
    {
        .GPID = 14,
        .func = FUNC_GPIO14,
        .periph = PERIPHS_IO_MUX_MTMS_U,
        .initialState = 1
    },
#else
    // Up
    {
        .GPID = 13,
        .func = FUNC_GPIO13,
        .periph = PERIPHS_IO_MUX_MTCK_U,
        .initialState = 1
    },
    // Left
    {
        .GPID = 4,
        .func = FUNC_GPIO4,
        .periph = PERIPHS_IO_MUX_GPIO4_U,
        .initialState = 1
    },
    // Right
    {
        .GPID = 12,
        .func = FUNC_GPIO12,
        .periph = PERIPHS_IO_MUX_MTDI_U,
        .initialState = 1
    },
#ifdef USE_BUTTON_3_NOT_BZR
    // Down
    {
        .GPID = 5,
        .func = FUNC_GPIO5,
        .periph = PERIPHS_IO_MUX_GPIO5_U,
        .initialState = 1
    },
#endif
#endif
};

static const gpioInfo_t gpioInfoOutput[] =
{
    // GPIO15 used as RST for the OLED
    {
        .GPID = 15,
        .func = FUNC_GPIO15,
        .periph = PERIPHS_IO_MUX_MTDO_U,
        .initialState = 0
    },
#if (SWADGE_VERSION != SWADGE_CHAINSAW)
    // Pull GPIO 14 high, this is for the microphone
    {
        .GPID = 14,
        .func = FUNC_GPIO14,
        .periph = PERIPHS_IO_MUX_MTMS_U,
        .initialState = 1
    },
#ifndef USE_BUTTON_3_NOT_BZR
    // Buzzer
    {
        .GPID = 5,
        .func = FUNC_GPIO5,
        .periph = PERIPHS_IO_MUX_GPIO5_U,
        .initialState = 0
    },
#endif
#endif
};

/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * This interrupt is called ever time a button is pressed or released
 * Should not be ICACHE_FLASH_ATTR because it's an interrupt
 *
 * @param v unused
 */
void gpioInterrupt( void* v __attribute__((unused)))
{
    // Get the current button status
    uint8_t status = GetButtons();

    // For all the buttons
    uint8_t i;
    for( i = 0; i < lengthof(gpioInfoInput); i++ )
    {
        int mask = 1 << i;
        // If the current button state doesn't match the previous button state
        if( (status & mask) != (LastGPIOState & mask) )
        {
            HandleButtonEventIRQ( status, i, (status & mask) ? 1 : 0 );
        }
    }
    // Record the current state for the next interrupt
    LastGPIOState = status;

    // Clear interrupt status
    uint32 gpio_status = GPIO_REG_READ(GPIO_STATUS_ADDRESS);
    GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, gpio_status);
}

/**
 * Initialize the GPIOs as button inputs with internal pullups and interrupts
 * Also set 14 high for the microphone
 *
 * @param enableMic true to enable the microphone, false to disable it
 */
void ICACHE_FLASH_ATTR SetupGPIO(bool enableMic)
{
    // Disable gpio interrupts
    ETS_GPIO_INTR_DISABLE();
    // interrupt handler for GPIOs in gpioInfo[]
    ETS_GPIO_INTR_ATTACH(gpioInterrupt, 0);

    // For each button
    uint8_t i;
    for(i = 0; i < lengthof(gpioInfoInput); i++ )
    {
        // Set the function
        PIN_FUNC_SELECT(gpioInfoInput[i].periph, gpioInfoInput[i].func);
        // Set it as an input
        PIN_DIR_INPUT = 1 << gpioInfoInput[i].GPID;
        // And enable a pullup
        GPIO_OUTPUT_SET(GPIO_ID_PIN(gpioInfoInput[i].GPID), gpioInfoInput[i].initialState );

        // Enable interrupt on any edge
        gpio_pin_intr_state_set(GPIO_ID_PIN(gpioInfoInput[i].GPID), GPIO_PIN_INTR_ANYEDGE);
        // Clear interrupt status
        GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, BIT(gpioInfoInput[i].GPID));
    }

    // Enable GPIO interrupts
    ETS_GPIO_INTR_ENABLE();

    // Get the initial button state
    LastGPIOState = GetButtons();

    for(i = 0; i < lengthof(gpioInfoOutput); i++ )
    {
        // Set the function
        PIN_FUNC_SELECT(gpioInfoOutput[i].periph, gpioInfoOutput[i].func);
        // Set it as an input
        PIN_DIR_OUTPUT = 1 << gpioInfoOutput[i].GPID;
        // And enable a pullup
        GPIO_OUTPUT_SET(GPIO_ID_PIN(gpioInfoOutput[i].GPID), gpioInfoOutput[i].initialState );
    }

#if (SWADGE_VERSION != SWADGE_CHAINSAW)
    // Turn off the mic if it's not being used
    if(false == enableMic)
    {
        GPIO_OUTPUT_SET(GPIO_ID_PIN(14), 0);
    }
#endif

    /* If you need to configure GPIO16, add ESP8266_NONOS_SDK/driver_lib/driver/gpio16.c
     * to the makefile and call either gpio16_output_conf() or gpio16_input_conf()
     */

    mBuzzerState = false;

    INIT_PRINTF( "Setup GPIO Complete\n" );
}

/**
 * Read up to eight button states from PIN_IN and return them as a bitmask
 *
 * @return An 8 bit bitmask corresponding to the button states in gpioInfo[]
 *         Bit 0 is the first button, bit 1 is the second button, etc
 */
uint8_t GetButtons(void)
{
    uint8_t ret = 0;

    // For each button, read it and set it in ret
    uint8_t i;
    for( i = 0; i < lengthof(gpioInfoInput); i++ )
    {
        ret |= (PIN_IN & (1 << gpioInfoInput[i].GPID)) ? (1 << i) : 0;
    }

    // Invert all the logic
    ret ^= ~0x00;

    return ret;
}

/**
 * Get the last gpio state, for debugging purposes
 * @return
 */
uint8_t ICACHE_FLASH_ATTR getLastGPIOState(void)
{
    return LastGPIOState;
}

/**
 * TODO
 * @param on
 */
void ICACHE_FLASH_ATTR setOledResetOn(bool on)
{
    GPIO_OUTPUT_SET(GPIO_ID_PIN(15), on ? 1 : 0 );
}

/**
 * Set the buzzer either off or on
 * @param on true to set it on, false to set it off
 */
void ICACHE_FLASH_ATTR setBuzzerGpio(bool on)
{
    mBuzzerState = on;
    GPIO_OUTPUT_SET(GPIO_ID_PIN(5), on ? 1 : 0 );
}

/**
 * Get the buzzer state
 *
 * @return true if the buzzer is on, false if it is off
 */
bool ICACHE_FLASH_ATTR getBuzzerGpio(void)
{
    return mBuzzerState;
}

/**
 * TODO
 */
void ICACHE_FLASH_ATTR setGpiosForBoot(void)
{
    GPIO_OUTPUT_SET(GPIO_ID_PIN(0),  1 );
    GPIO_OUTPUT_SET(GPIO_ID_PIN(2),  1 );
    GPIO_OUTPUT_SET(GPIO_ID_PIN(15), 0 );
}