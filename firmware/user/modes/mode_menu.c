/*============================================================================
 * Includes
 *==========================================================================*/

#include "osapi.h"
#include "user_main.h"
#include "mode_menu.h"
#include "display/oled.h"
#include "display/font.h"

/*============================================================================
 * Defines
 *==========================================================================*/

#define MARGIN 3

/*============================================================================
 * Function prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR modeInit(void);
void ICACHE_FLASH_ATTR modeButtonCallback(uint8_t state, int button, int down);
void ICACHE_FLASH_ATTR drawMenu(void);

/*============================================================================
 * Variables
 *==========================================================================*/

swadgeMode menuMode =
{
    .modeName = "menu",
    .fnEnterMode = modeInit,
    .fnExitMode = NULL,
    .fnTimerCallback = NULL,
    .fnButtonCallback = modeButtonCallback,
    .fnAudioCallback = NULL,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
};

uint8_t numModes = 0;
uint8_t selectedMode = 1;
swadgeMode** modes = NULL;

/*============================================================================
 * Variables
 *==========================================================================*/

/**
 * Initialize the meny by getting the list of modes from user_main.c
 */
void ICACHE_FLASH_ATTR modeInit(void)
{
    // Get the list of modes
    numModes = getSwadgeModes(&modes);
    // Don't count the menu as a mode
    numModes--;
    selectedMode = 0;

    drawMenu();
}

/**
 * Handle the button. Left press selects the mode, right press starts it
 *
 * @param state  A bitmask of all buttons, unused
 * @param button The button that was just pressed or released
 * @param down   true if the button was pressed, false if it was released
 */
void ICACHE_FLASH_ATTR modeButtonCallback(uint8_t state __attribute__((unused)),
        int button, int down)
{
    if(down)
    {
        switch(button)
        {
            case 1:
            {
                // Cycle to the next mode
                selectedMode = (selectedMode + 1) % numModes;
                drawMenu();
                break;
            }
            case 2:
            {
                // Select the mode
                switchToSwadgeMode(1 + selectedMode);
                break;
            }
            default:
            {
                // Do nothing
                break;
            }
        }
    }
}

/**
 * Draw a cursor and all the mode names to the display
 */
void ICACHE_FLASH_ATTR drawMenu(void)
{
    clearDisplay();

    // Draw a cursor
    plotText(0, 0, ">", IBM_VGA_8);

    // Draw all the mode names
    uint8_t idx;
    for(idx = 0; idx < numModes; idx++)
    {
        uint8_t modeToDraw = 1 + ((selectedMode + idx) % numModes);
        plotText(MARGIN + 6, idx * (MARGIN + FONT_HEIGHT_IBMVGA8),
                 modes[modeToDraw]->modeName, IBM_VGA_8);
    }

    display();
}