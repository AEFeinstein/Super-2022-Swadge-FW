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
#define NUM_MENU_ROWS 5

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
    .fnButtonCallback = modeButtonCallback,
    .fnAudioCallback = NULL,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
};

uint8_t numModes = 0;
swadgeMode** modes = NULL;
uint8_t selectedMode = 0;
uint8_t menuPos = 0;
uint8_t cursorPos = 0;

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
    menuPos = 0;
    cursorPos = 0;

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
            case 0:
            {
                // Select the mode
                switchToSwadgeMode(1 + selectedMode);
                break;
            }
            case 2:
            {
                // Cycle the menu by either moving the cursor or shifting names
                if(cursorPos < NUM_MENU_ROWS - 1)
                {
                    // Move the cursor
                    cursorPos++;
                }
                else
                {
                    // Shift the names
                    menuPos = (menuPos + 1) % numModes;
                }

                // Cycle the currently selected mode
                selectedMode = (selectedMode + 1) % numModes;

                // Draw the menu
                drawMenu();
                break;
            }
            case 1:
            {
                // Cycle the menu by either moving the cursor or shifting names
                if(cursorPos > 0)
                {
                    // Move the cursor
                    cursorPos--;
                }
                else
                {
                    // Shift the names
                    if(0 == menuPos)
                    {
                        menuPos = numModes - 1;
                    }
                    else
                    {
                        menuPos--;
                    }
                }

                // Cycle the currently selected mode
                if(0 == selectedMode)
                {
                    selectedMode = numModes - 1;
                }
                else
                {
                    selectedMode--;
                }

                // Draw the menu
                drawMenu();
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
    plotText(0, cursorPos * (MARGIN + FONT_HEIGHT_IBMVGA8), ">", IBM_VGA_8);

    // Draw all the mode names
    uint8_t idx;
    for(idx = 0; idx < NUM_MENU_ROWS; idx++)
    {
        uint8_t modeToDraw = 1 + ((menuPos + idx) % numModes);
        plotText(MARGIN + 6, idx * (MARGIN + FONT_HEIGHT_IBMVGA8),
                 modes[modeToDraw]->modeName, IBM_VGA_8);
    }
}