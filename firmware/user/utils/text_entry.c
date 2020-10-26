#include "text_entry.h"
#include "oled.h"
#include "bresenham.h"
#include "font.h"

typedef enum
{
    UPPERCASE,
    LOWERCASE,
    DONECASE
} letterCase_t;

static int telen;
static char* testring;
static letterCase_t uppercase;
static int8_t selx;
static int8_t sely;
static char selchar;
timer_t cursorTimer;
bool showCursor;

void ICACHE_FLASH_ATTR blinkCursor(void* arg)
{
    showCursor = !showCursor;
}

void ICACHE_FLASH_ATTR textEntryStart( int max_len, char* buffer )
{
    telen = max_len;
    testring = buffer;
    uppercase = LOWERCASE;
    testring[0] = 0;
    showCursor = true;
    timerSetFn(&cursorTimer, blinkCursor, NULL);
    timerArm(&cursorTimer, 500, true);
}

void ICACHE_FLASH_ATTR textEntryEnd( void )
{
    timerDisarm(&cursorTimer);
}

#define KB_LINES 5

static const char* keyboard_upper = "\
~!@#$%^&*()_+\x03\x05\
\x09QWERTYUIOP{}|\x05\
\002ASDFGHJKL:\"\x0a\x05\
\x01ZXCVBNM<>?\x01\x05\x04";

static const char* keyboard_lower = "\
`1234567890-=\x03\x05\
\x09qwertyuiop[]\\\x05\
\002asdfghjkl;\'\x0a\x05\
\x01zxcvbnm,./\x01\x05\x04";

static const uint8_t lengthperline[] RODATA_ATTR = { 14, 14, 13, 12, 1 };

bool ICACHE_FLASH_ATTR textEntryDraw()
{
    clearDisplay();

    int16_t endPos = 0;
    for(int i = 0; testring[i]; i++ )
    {
        char c = testring[i];
        char cs[2];
        cs[0] = c;
        cs[1] = 0;
        endPos = plotText( 3 + i * 8, 2, cs, IBM_VGA_8, WHITE);
        if( c >= 'a' && c <= 'z' )
        {
            plotLine( 2 + i * 8, 13, 10 + i * 8, 13, WHITE );
        }
    }

    if(showCursor)
    {
        plotLine(endPos + 1, 2, endPos + 1, 2 + FONT_HEIGHT_IBMVGA8 - 1, WHITE);
    }

    if( uppercase == DONECASE )
    {
        return false;
    }

    switch(uppercase)
    {
        case UPPERCASE:
        {
            int8_t width = textWidth("Upper", TOM_THUMB);
            plotText(OLED_WIDTH - width, OLED_HEIGHT - FONT_HEIGHT_TOMTHUMB - 2, "Upper", TOM_THUMB, WHITE);
            break;
        }
        case LOWERCASE:
        {
            int8_t width = textWidth("Lower", TOM_THUMB);
            plotText(OLED_WIDTH - width, OLED_HEIGHT - FONT_HEIGHT_TOMTHUMB - 2, "Lower", TOM_THUMB, WHITE);
            plotLine(OLED_WIDTH - width, OLED_HEIGHT - 1, OLED_WIDTH - 1, OLED_HEIGHT - 1, WHITE);
            break;
        }
        default:
        case DONECASE:
        {
            break;
        }
    }

    int x = 0;
    int y = 0;
    char c;
    const char* s = (uppercase == UPPERCASE) ? keyboard_upper : keyboard_lower;
    while( (c = *s) )
    {
        if( c == 5 )
        {
            x = 0;
            y++;
        }
        else
        {
            char sts[2];
            sts[0] = c;
            sts[1] = 0;
            int posx = x * 8 + 10;
            int posy = y * 8 + 19;
            int width = 4;
            switch( c )
            {
                case 1: //Shift
                {
                    plotLine( posx, posy + 4, posx + 2, posy + 4, WHITE );
                    plotLine( posx + 1, posy + 4, posx + 1, posy, WHITE );
                    plotLine( posx + 1, posy, posx + 3, posy + 2, WHITE );
                    plotLine( posx + 1, posy, posx - 1, posy + 2, WHITE );
                    break;
                }
                case 2: //Capslock
                {
                    plotLine( posx, posy + 4, posx + 2, posy + 4, WHITE );
                    plotLine( posx + 1, posy + 4, posx + 1, posy, WHITE );
                    plotLine( posx + 1, posy, posx + 3, posy + 2, WHITE );
                    plotLine( posx + 1, posy, posx - 1, posy + 2, WHITE );
                    break;
                }
                case 3: //Backspace
                {
                    plotLine( posx - 1, posy + 2, posx + 3, posy + 2, WHITE );
                    plotLine( posx - 1, posy + 2, posx + 1, posy + 0, WHITE );
                    plotLine( posx - 1, posy + 2, posx + 1, posy + 4, WHITE );
                    break;
                }
                case 4:
                {
                    plotLine( posx + 1, posy + 5, posx + 36, posy + 5, WHITE );
                    width = 40;
                    break;
                }
                case 9: // tab
                {
                    plotLine( posx - 1, posy + 2, posx + 3, posy + 2, WHITE );
                    plotLine( posx + 3, posy + 2, posx + 1, posy + 0, WHITE );
                    plotLine( posx + 3, posy + 2, posx + 1, posy + 4, WHITE );
                    plotLine( posx - 1, posy + 0, posx - 1, posy + 4, WHITE );
                    break;
                }
                case 10:
                {
                    //plotLine( posx, posy, posx+1, posy+5, WHITE );
                    //plotLine( posx+1, posy+5, posx+3, posy+3, WHITE );
                    plotText( posx, posy, "OK", TOM_THUMB, WHITE);
                    width = 9;
                    break;
                }
                default:
                {
                    plotText( posx, posy, sts, TOM_THUMB, WHITE);
                }
            }
            if( x == selx && y == sely )
            {
                //Draw Box around selected item.
                plotRect( posx - 2, posy - 2, posx + width, posy + 6, WHITE );
                selchar = c;
            }
            x++;
        }
        s++;
    }
    return true;
}

bool ICACHE_FLASH_ATTR textEntryInput( uint8_t down, uint8_t button )
{
    if( !down )
    {
        return true;
    }
    if( uppercase == DONECASE )
    {
        return false;
    }
    if( button == 4 )
    {
        int sl = ets_strlen(testring);
        switch( selchar )
        {
            case 10:    //Done.
            {
                uppercase = DONECASE;
                return false;
            }
            case 1:
            case 2: //Shift or caps
            {
                if(LOWERCASE == uppercase)
                {
                    uppercase = UPPERCASE;
                }
                else
                {
                    uppercase = LOWERCASE;
                }
                break;
            }
            case 3: //backspace.
            {
                if(sl > 0)
                {
                    testring[sl - 1] = 0;
                    sl--;
                }
                break;
            }
            default:
            {
                if( sl < telen - 1 )
                {
                    testring[sl + 1] = 0;
                    testring[sl] = selchar;

                    if( uppercase == UPPERCASE )
                    {
                        uppercase = LOWERCASE;
                    }
                }
                break;
            }
        }
    }

    switch( button )
    {
        case 0: //Left?
        {
            selx--;
            break;
        }
        case 1: //Down
        {
            sely++;
            break;
        }
        case 2: //Right?
        {
            selx++;
            break;
        }
        case 3: //Up
        {
            sely--;
            break;
        }
        default:
        {
            break;
        }
    }
    if( sely < 0 )
    {
        sely = KB_LINES - 1;
    }
    if( sely >= KB_LINES )
    {
        sely = 0;
    }
    if( selx < 0 )
    {
        selx = lengthperline[sely] - 1;
    }
    if( selx >= lengthperline[sely] )
    {
        selx = 0;
    }
    return true;
}
