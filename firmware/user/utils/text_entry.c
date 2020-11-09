/*============================================================================
 * Includes
 *==========================================================================*/

#include "text_entry.h"
#include "oled.h"
#include "bresenham.h"
#include "font.h"

/*============================================================================
 * Enums
 *==========================================================================*/

typedef enum
{
    NO_SHIFT,
    SHIFT,
    CAPS_LOCK,
    SPECIAL_DONE
} keyModifier_t;

typedef enum
{
    KEY_SHIFT     = 0x01,
    KEY_CAPSLOCK  = 0x02,
    KEY_BACKSPACE = 0x03,
    KEY_SPACE     = 0x04,
    KEY_EOL       = 0x05,
    KEY_TAB       = 0x09,
    KEY_ENTER     = 0x0A,
} controlChar_t;

/*============================================================================
 * Variables
 *==========================================================================*/

static int texLen;
static char* texString;
static keyModifier_t keyMod;
static int8_t selx;
static int8_t sely;
static char selChar;
static timer_t cursorTimer;
static bool showCursor;

#define KB_LINES 5

// See controlChar_t
static const char* keyboard_upper = "\
~!@#$%^&*()_+\x03\x05\
\x09QWERTYUIOP{}|\x05\
\002ASDFGHJKL:\"\x0a\x05\
\x01ZXCVBNM<>?\x01\x05\
\x04";

// See controlChar_t
static const char* keyboard_lower = "\
`1234567890-=\x03\x05\
\x09qwertyuiop[]\\\x05\
\002asdfghjkl;\'\x0a\x05\
\x01zxcvbnm,./\x01\x05\
\x04";

static const uint8_t lengthperline[] = { 14, 14, 13, 12, 1 };

/*============================================================================
 * Function Prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR blinkCursor(void* arg);

/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * Initialize the text entry
 *
 * @param max_len The length of buffer
 * @param buffer  A char* to store the entered text in
 */
void ICACHE_FLASH_ATTR textEntryStart( int max_len, char* buffer )
{
    texLen = max_len;
    texString = buffer;
    keyMod = NO_SHIFT;
    texString[0] = 0;
    showCursor = true;
    timerSetFn(&cursorTimer, blinkCursor, NULL);
    timerArm(&cursorTimer, 500, true);
}

/**
 * Finish the text entry by disarming the cursor blink timer
 */
void ICACHE_FLASH_ATTR textEntryEnd( void )
{
    timerDisarm(&cursorTimer);
}

/**
 * Draw the text entry UI
 *
 * @return true if text entry is still being used
 *         false if text entry is finished
 */
bool ICACHE_FLASH_ATTR textEntryDraw(void)
{
    // If we're done, return false
    if( keyMod == SPECIAL_DONE )
    {
        return false;
    }

    clearDisplay();

    // Draw the text entered so far, underlining capital letters
    int16_t endPos = 0;
    for(int i = 0; texString[i]; i++ )
    {
        char c = texString[i];
        // If this is a whitespace char
        if(KEY_SPACE == c || KEY_TAB == c)
        {
            // Draw it as a space
            c = ' ';
        }
        // Draw the char
        char cs[] = {c, 0x00};
        int16_t startPos = endPos + 1;
        endPos = plotText( startPos, 2, cs, IBM_VGA_8, WHITE);
        // Underline capital chars
        if( c >= 'A' && c <= 'Z' )
        {
            plotLine( startPos, 13, endPos - 2, 13, WHITE );
        }
    }

    // If the blinky cursor should be shown, draw it
    if(showCursor)
    {
        plotLine(endPos + 1, 2, endPos + 1, 2 + FONT_HEIGHT_IBMVGA8 - 1, WHITE);
    }

    // Draw an indicator for the current key modifier
    switch(keyMod)
    {
        case SHIFT:
        {
            int8_t width = textWidth("Typing: Upper", TOM_THUMB);
            int8_t typingWidth = textWidth("Typing: ", TOM_THUMB);
            plotText(OLED_WIDTH - width, OLED_HEIGHT - FONT_HEIGHT_TOMTHUMB - 2, "Typing: Upper", TOM_THUMB, WHITE);
            plotLine(OLED_WIDTH - width + typingWidth, OLED_HEIGHT - 1, OLED_WIDTH - 1, OLED_HEIGHT - 1, WHITE);
            break;
        }
        case NO_SHIFT:
        {
            int8_t width = textWidth("Typing: Lower", TOM_THUMB);
            plotText(OLED_WIDTH - width, OLED_HEIGHT - FONT_HEIGHT_TOMTHUMB - 2, "Typing: Lower", TOM_THUMB, WHITE);
            break;
        }
        case CAPS_LOCK:
        {
            int8_t width = textWidth("Typing: CAPS LOCK", TOM_THUMB);
            int8_t typingWidth = textWidth("Typing: ", TOM_THUMB);
            plotText(OLED_WIDTH - width, OLED_HEIGHT - FONT_HEIGHT_TOMTHUMB - 2, "Typing: CAPS LOCK", TOM_THUMB, WHITE);
            plotLine(OLED_WIDTH - width + typingWidth, OLED_HEIGHT - 1, OLED_WIDTH - 1, OLED_HEIGHT - 1, WHITE);
            break;
        }
        default:
        case SPECIAL_DONE:
        {
            break;
        }
    }

    // Draw the keyboard
    int x = 0;
    int y = 0;
    char c;
    const char* s = (keyMod == NO_SHIFT) ? keyboard_lower : keyboard_upper;
    while( (c = *s) )
    {
        // EOL character hit, move to the next row
        if( c == KEY_EOL )
        {
            x = 0;
            y++;
        }
        else
        {
            char sts[] = {c, 0};
            int posx = x * 8 + 10;
            int posy = y * 8 + 19;
            int width = 4;
            // Draw the character, may be a control char
            switch( c )
            {
                case KEY_SHIFT:
                case KEY_CAPSLOCK:
                {
                    // Draw shift/capslock
                    plotLine( posx, posy + 4, posx + 2, posy + 4, WHITE );
                    plotLine( posx + 1, posy + 4, posx + 1, posy, WHITE );
                    plotLine( posx + 1, posy, posx + 3, posy + 2, WHITE );
                    plotLine( posx + 1, posy, posx - 1, posy + 2, WHITE );
                    break;
                }
                case KEY_BACKSPACE:
                {
                    // Draw backspace
                    plotLine( posx - 1, posy + 2, posx + 3, posy + 2, WHITE );
                    plotLine( posx - 1, posy + 2, posx + 1, posy + 0, WHITE );
                    plotLine( posx - 1, posy + 2, posx + 1, posy + 4, WHITE );
                    break;
                }
                case KEY_SPACE:
                {
                    // Draw spacebar
                    plotRect( posx + 1, posy + 1, posx + 37, posy + 3, WHITE);
                    width = 40;
                    break;
                }
                case KEY_TAB:
                {
                    // Draw tab
                    plotLine( posx - 1, posy + 2, posx + 3, posy + 2, WHITE );
                    plotLine( posx + 3, posy + 2, posx + 1, posy + 0, WHITE );
                    plotLine( posx + 3, posy + 2, posx + 1, posy + 4, WHITE );
                    plotLine( posx - 1, posy + 0, posx - 1, posy + 4, WHITE );
                    break;
                }
                case KEY_ENTER:
                {
                    // Draw an OK for enter
                    plotText( posx, posy, "OK", TOM_THUMB, WHITE);
                    width = textWidth("OK", TOM_THUMB);
                    break;
                }
                default:
                {
                    // Just draw the char
                    plotText( posx, posy, sts, TOM_THUMB, WHITE);
                }
            }
            if( x == selx && y == sely )
            {
                //Draw Box around selected item.
                plotRect( posx - 2, posy - 2, posx + width, posy + 6, WHITE );
                selChar = c;
            }
            x++;
        }
        s++;
    }
    return true;
}

/**
 * handle button input for text entry
 *
 * @param down   true if the button was pressed, false if it was released
 * @param button The button that was pressed
 * @return true if text entry is still ongoing
 *         false if the enter key was pressed and text entry is done
 */
bool ICACHE_FLASH_ATTR textEntryInput( uint8_t down, button_num button )
{
    // If this was a release, just return true
    if( !down )
    {
        return true;
    }

    // If text entry is done, return false
    if( keyMod == SPECIAL_DONE )
    {
        return false;
    }

    // Handle the button
    switch( button )
    {
        case ACTION:
        {
            // User selected this key
            int stringLen = ets_strlen(texString);
            switch( selChar )
            {
                case KEY_ENTER:
                {
                    // Enter key was pressed, so text entry is done
                    keyMod = SPECIAL_DONE;
                    return false;
                }
                case KEY_SHIFT:
                case KEY_CAPSLOCK:
                {
                    // Rotate the keyMod from NO_SHIFT -> SHIFT -> CAPS LOCK, and back
                    if(NO_SHIFT == keyMod)
                    {
                        keyMod = SHIFT;
                    }
                    else if(SHIFT == keyMod)
                    {
                        keyMod = CAPS_LOCK;
                    }
                    else
                    {
                        keyMod = NO_SHIFT;
                    }
                    break;
                }
                case KEY_BACKSPACE:
                {
                    // If there is any text, delete the last char
                    if(stringLen > 0)
                    {
                        texString[stringLen - 1] = 0;
                    }
                    break;
                }
                default:
                {
                    // If there is still space, add the selected char
                    if( stringLen < texLen - 1 )
                    {
                        texString[stringLen] = selChar;
                        texString[stringLen + 1] = 0;

                        // Clear shift if it as active
                        if( keyMod == SHIFT )
                        {
                            keyMod = NO_SHIFT;
                        }
                    }
                    break;
                }
            }
            break;
        }
        case LEFT:
        {
            // Move cursor
            selx--;
            break;
        }
        case DOWN:
        {
            // Move cursor
            sely++;
            break;
        }
        case RIGHT:
        {
            // Move cursor
            selx++;
            break;
        }
        case UP:
        {
            // Move cursor
            sely--;
            break;
        }
        default:
        {
            break;
        }
    }

    // Make sure the cursor is in bounds, wrap around if necessary
    if( sely < 0 )
    {
        sely = KB_LINES - 1;
    }
    else if( sely >= KB_LINES )
    {
        sely = 0;
    }

    // Make sure the cursor is in bounds, wrap around if necessary
    if( selx < 0 )
    {
        selx = lengthperline[sely] - 1;
    }
    else if( selx >= lengthperline[sely] )
    {
        selx = 0;
    }

    // All done, still entering text
    return true;
}

/**
 * This is called on a timer to blink the cursor
 *
 * @param arg unused
 */
void ICACHE_FLASH_ATTR blinkCursor(void* arg __attribute__((unused)))
{
    showCursor = !showCursor;
}
