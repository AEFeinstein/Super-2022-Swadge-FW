/*============================================================================
 * Includes
 *==========================================================================*/

#include "gpio_buttons.h"
#include "user_interface.h"
#include "c_types.h"
#include <gpio.h>
#include <ets_sys.h>
#include <esp82xxutil.h>

/*============================================================================
 * Defines
 *==========================================================================*/

#define lengthof(x) (sizeof(x) / sizeof(x[0]))
#define REV_A

/*============================================================================
 * Prototypes
 *==========================================================================*/

void gpio_pin_intr_state_set(uint32 i, GPIO_INT_TYPE intr_state);
unsigned char ICACHE_FLASH_ATTR GetButtons();

/*============================================================================
 * Structs
 *==========================================================================*/

typedef struct
{
    uint8_t GPID;
    uint8_t func;
    int periph;
} gpioInfo_t;

/*============================================================================
 * Variables
 *==========================================================================*/

fnButtonCallback mButtonHandler = NULL;
volatile uint8_t LastGPIOState;

#if defined(REV_A)
static const gpioInfo_t gpioInfo[] =
{
    {
        .GPID = 0,
        .func = FUNC_GPIO0,
        .periph = PERIPHS_IO_MUX_GPIO0_U
    },
    {
        .GPID = 4,
        .func = FUNC_GPIO4,
        .periph = PERIPHS_IO_MUX_GPIO4_U
    },
    {
        .GPID = 2,
        .func = FUNC_GPIO2,
        .periph = PERIPHS_IO_MUX_GPIO2_U
    },
    {
        .GPID = 5,
        .func = FUNC_GPIO5,
        .periph = PERIPHS_IO_MUX_GPIO5_U
    }
};
#elif defined(REV_B)
static const gpioInfo_t gpioInfo[] =
{
    // Mode select
    {
        .GPID = 0,
        .func = FUNC_GPIO0,
        .periph = PERIPHS_IO_MUX_GPIO0_U
    },
    // Left button
    {
        .GPID = 13,
        .func = FUNC_GPIO13,
        .periph = PERIPHS_IO_MUX_MTCK_U
    },
    // Right button
    {
        .GPID = 2,
        .func = FUNC_GPIO2,
        .periph = PERIPHS_IO_MUX_GPIO2_U
    }
};
#else
#error "Please define a board revision"
#endif

/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * This interrupt is called ever time a button is pressed or released
 *
 * @param v unused
 */
void ICACHE_FLASH_ATTR gpioInterrupt( void* v )
{
    // Get the current button status
    uint8_t status = GetButtons();

    // For all the buttons
    int i;
    for( i = 0; i < lengthof(gpioInfo); i++ )
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
void ICACHE_FLASH_ATTR SetupGPIO(fnButtonCallback handler)
{
    // Save the handler
    mButtonHandler = handler;

    // Disable gpio interrupts
    ETS_GPIO_INTR_DISABLE();
    // GPIO12 interrupt handler
    ETS_GPIO_INTR_ATTACH(gpioInterrupt, 0);

    // For each button
    int i;
    for(i = 0; i < lengthof(gpioInfo); i++ )
    {
        // Set the function
        PIN_FUNC_SELECT(gpioInfo[i].periph, gpioInfo[i].func);
        // Set it as an input
        PIN_DIR_INPUT = 1 << gpioInfo[i].GPID;
        // And enable a pullup
        GPIO_OUTPUT_SET(GPIO_ID_PIN(gpioInfo[i].GPID), 1 );

        // Enable interrupt on any edge
        gpio_pin_intr_state_set(GPIO_ID_PIN(gpioInfo[i].GPID), GPIO_PIN_INTR_ANYEDGE);
        // Clear interrupt status
        GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, BIT(gpioInfo[i].GPID));
    }

    // Enable GPIO interrupts
    ETS_GPIO_INTR_ENABLE();

    // Get the initial button state
    LastGPIOState = GetButtons();

    // Pull GPIO 14 high, this is for the microphone
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTMS_U, FUNC_GPIO14);
    GPIO_OUTPUT_SET(GPIO_ID_PIN(14), 1 );

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

    printf( "Setup GPIO Complete\n" );
}

/**
 * Read up to eight button states from PIN_IN and return them as a bitmas
 *
 * @return An 8 bit bitmask corresponding to the button states in gpioInfo[]
 *         Bit 0 is the first button, bit 1 is the second button, etc
 */
uint8_t ICACHE_FLASH_ATTR GetButtons()
{
    uint8_t ret = 0;

    // For each button, read it and set it in ret
    int i;
    for( i = 0; i < lengthof(gpioInfo); i++ )
    {
        ret |= (PIN_IN & (1 << gpioInfo[i].GPID)) ? (1 << i) : 0;
    }

    // GPIO15's logic is inverted.  Don't flip it but flip everything else.
    ret ^= ~0x20;

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
