#include "text_entry.h"
#include "oled.h"
#include "bresenham.h"
#include "font.h"

static int telen;
static char * testring;
static uint8_t uppercase;
static int8_t selx;
static int8_t sely;
static char selchar;

void ICACHE_FLASH_ATTR textEntryStart( int max_len, char * buffer )
{
	telen = max_len;
	testring = buffer;
	uppercase = 0;
	testring[0] = 0;
}

#define KB_LINES 5

//WARNING: NO TILDE OR {}

static const char * keyboard_upper = "\
`!@#$%^&*()_+\x03\x05\
\x09QWERTYUIOP[]|\x05\
\002ASDFGHJKL:\"\x0a\x05\
\x01ZXCVBNM<>?\x01\x05\x04";

static const char * keyboard_lower = "\
`1234567890-=\x03\x05\
\x09qwertyuiop[]\\\x05\
\002asdfghjkl;\'\x0a\x05\
\x01zxcvbnm,./\x01\x05\x04";

static const uint8_t lengthperline[] RODATA_ATTR = { 14, 14, 13, 12, 1 };

bool ICACHE_FLASH_ATTR textEntryDraw()
{
    clearDisplay();

	{
		int i;
		for( i = 0; testring[i]; i++ )
		{
			char c = testring[i];
			char cs[2];
			cs[0] = c;
			cs[1] = 0;
			plotText( 3+i*8, 2, cs, IBM_VGA_8, WHITE);
			if( c >= 'a' && c <= 'z' )
				plotLine( 2+i*8, 13, 10+i*8, 13, WHITE );
		}
	}

	if( uppercase == 3 ) return false;

	int x = 0;
	int y = 0;
	char c;
	const char * s = uppercase?keyboard_upper:keyboard_lower;
	os_printf( "%d %c\n", uppercase, s[0] );
	while( (c = *s) )
	{
		if( c == 5 )
		{
			x = 0;
			y ++;
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
				plotLine( posx, posy+4, posx+2, posy+4, WHITE );
				plotLine( posx+1, posy+4, posx+1, posy, WHITE );
				plotLine( posx+1, posy, posx+3, posy+2, WHITE );
				plotLine( posx+1, posy, posx-1, posy+2, WHITE );
				break;
			case 2: //Capslock
				plotLine( posx, posy+4, posx+2, posy+4, WHITE );
				plotLine( posx+1, posy+4, posx+1, posy, WHITE );
				plotLine( posx+1, posy, posx+3, posy+2, WHITE );
				plotLine( posx+1, posy, posx-1, posy+2, WHITE );
				break;
			case 3: //Backspace
				plotLine( posx, posy+3, posx+4, posy+3, WHITE );
				plotLine( posx, posy+3, posx+2, posy+1, WHITE );
				plotLine( posx, posy+3, posx+2, posy+5, WHITE );
				break;
			case 4:
				plotLine( posx+1, posy+5, posx+36, posy+5, WHITE );
				width = 40;
				break;
			case 9:
				plotLine( posx, posy+3, posx+4, posy+3, WHITE );
				plotLine( posx+4, posy+3, posx+2, posy+1, WHITE );
				plotLine( posx+4, posy+3, posx+2, posy+5, WHITE );
				plotLine( posx, posy+1, posx, posy+5, WHITE );
				break;
			case 10:
				//plotLine( posx, posy, posx+1, posy+5, WHITE );
				//plotLine( posx+1, posy+5, posx+3, posy+3, WHITE );
			    plotText( posx, posy, "OK", TOM_THUMB, WHITE);
				width = 9;
				break;
			default:
			    plotText( posx, posy, sts, TOM_THUMB, WHITE);
			}
			if( x == selx && y == sely )
			{
				//Draw Box around selected item.
				plotRect( posx-2, posy-2, posx+width, posy+6 , WHITE );
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
	if( !down ) return true;
	if( uppercase == 3 ) return false;
	if( button == 4 )
	{
		int sl = strlen(testring);
		switch( selchar )
		{
			case 10:	//Done.
				uppercase = 3;
				return false;
			case 1: case 2: //Shift or caps
				uppercase = uppercase?0:selchar;
				break;
			case 3: //backspace.
				if(sl>0)
				{
					testring[sl-1] = 0;
				}
				break;
			default:			
				if( sl < telen-1 )
				{
					testring[sl+1] = 0;
					testring[sl] = selchar;

					if( uppercase == 1 ) uppercase = 0;
				}
			break;
		}
	}

	switch( button )
	{
	case 0: //Left?
		selx--;
		break;
	case 1: //Down
		sely++;
		break;
	case 2: //Right?
		selx++;
		break;
	case 3: //Up
		sely--;
		break;
	default:
		break;
	}
	if( sely < 0 ) sely = KB_LINES-1;
	if( sely >= KB_LINES ) sely = 0;
	if( selx < 0 ) selx = lengthperline[sely]-1;
	if( selx >= lengthperline[sely] ) selx = 0;
	return true;
}


