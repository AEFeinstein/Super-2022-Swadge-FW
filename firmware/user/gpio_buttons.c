/*============================================================================
 * Includes
 *==========================================================================*/

#include "gpio_buttons.h"
#include "user_interface.h"
#include "c_types.h"
#include <gpio.h>
#include <ets_sys.h>
#include <esp82xxutil.h>
#include <osapi.h>
#include "missingEspFnPrototypes.h"
#include <uart.h>

/*============================================================================
 * Defines
 *==========================================================================*/

#define lengthof(x) (sizeof(x) / sizeof(x[0]))
#define REV_B

/*============================================================================
 * Prototypes
 *==========================================================================*/

void gpio_pin_intr_state_set(uint32 i, GPIO_INT_TYPE intr_state);
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

void (* volatile mButtonHandler)(uint8_t state, int button, int down) = NULL;
volatile uint8_t LastGPIOState;

// Matches order in button_mask
static const gpioInfo_t gpioInfoInput[] =
{
	// Up
	{
		.GPID = 13,
		.func = FUNC_GPIO13,
		.periph = PERIPHS_IO_MUX_MTCK_U,
		.initialState = 1
	},
	// Down
	{
		.GPID = 5,
		.func = FUNC_GPIO5,
		.periph = PERIPHS_IO_MUX_GPIO5_U,
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
	// Pull GPIO 14 high, this is for the microphone
	{
		.GPID = 14,
		.func = FUNC_GPIO14,
		.periph = PERIPHS_IO_MUX_MTMS_U,
		.initialState = 1
	},
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
            // Call the callback
            if(NULL != mButtonHandler)
            {
                mButtonHandler( status, i, (status & mask) ? 1 : 0 );
            }
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
 */
void ICACHE_FLASH_ATTR SetupGPIO(void (*handler)(uint8_t state, int button, int down), bool enableMic)
{
    // Save the handler
    mButtonHandler = handler;

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

    // Turn off the mic if it's not being used
    if(false == enableMic)
    {
    	GPIO_OUTPUT_SET(GPIO_ID_PIN(14), 0);
    }

    // Set GPIO16 for Input,  mux configuration for XPD_DCDC and rtc_gpio0 connection
    WRITE_PERI_REG(PAD_XPD_DCDC_CONF,
                   (READ_PERI_REG(PAD_XPD_DCDC_CONF) & 0xffffffbc) | (uint32)
                   0x1);

    // mux configuration for out enable
    WRITE_PERI_REG(RTC_GPIO_CONF,
                   (READ_PERI_REG(RTC_GPIO_CONF) & (uint32)0xfffffffe) | (uint32)0x0);

    // out disable
    WRITE_PERI_REG(RTC_GPIO_ENABLE,
                   READ_PERI_REG(RTC_GPIO_ENABLE) & (uint32)0xfffffffe);

    os_printf( "Setup GPIO Complete\n" );
}

/**
 * Read up to eight button states from PIN_IN and return them as a bitmas
 *
 * @return An 8 bit bitmask corresponding to the button states in gpioInfo[]
 *         Bit 0 is the first button, bit 1 is the second button, etc
 */
uint8_t GetButtons()
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
 *
 * @param on
 */
void ICACHE_FLASH_ATTR setOledResetOn(bool on)
{
	 GPIO_OUTPUT_SET(GPIO_ID_PIN(15), on ? 1 : 0 );
}
