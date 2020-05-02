//Copyright (c) 2011 <>< Charles Lohr - Under the MIT/x11 or NewBSD License you choose.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "rawdraw/CNFGFunctions.h"
#include "rawdraw/os_generic.h"
#include "swadgemu.h"

//ESP Includes
#include "user_interface.h"

unsigned frames = 0;
unsigned long iframeno = 0;
int px_scale = INIT_PX_SCALE;
uint32_t * rawvidmem;
short screenx, screeny;
uint8_t footerpix[FOOTER_PIXELS*OLED_WIDTH];

void HandleKey( int keycode, int bDown )
{
	if( keycode == 65307 ) exit( 0 );
	printf( "Key: %d -> %d\n", keycode, bDown );
}

void HandleButton( int x, int y, int button, int bDown )
{
	printf( "Button: %d,%d (%d) -> %d\n", x, y, button, bDown );
}

void HandleMotion( int x, int y, int mask )
{
}

void HandleDestroy()
{
	printf( "Destroying\n" );
	exit(10);
}

void emuFooter()
{
	int x, y;
	for( y = 0; y < FOOTER_PIXELS; y++ )
	for( x = 0; x < OLED_WIDTH; x++ )
	{
		footerpix[x+y*OLED_WIDTH] = rand()&1;
	}
	emuSendOLEDData( 1, footerpix );
}


void emuSendOLEDData( int disp, uint8_t * currentFb )
{
	int x, y;
	for( y = 0; y < (disp?FOOTER_PIXELS:OLED_HEIGHT); y++ )
	{
		for( x = 0; x < OLED_WIDTH; x++ )
		{
			uint8_t col;
			if( disp == 0 )
			{
				col = currentFb[(x + (y / 8) * OLED_WIDTH)] & (1 << (y & 7));
			}
			else
			{
				col = currentFb[x+y*OLED_WIDTH];
			}
			uint32_t pxcol = col?(disp?0xffffffff:0xff80ffff):0x00000000;
			int lx, ly;
			uint32_t * pxloc = rawvidmem + ( x +  ( ( y + (disp?OLED_HEIGHT:0) ) ) * OLED_WIDTH * px_scale ) * px_scale;
			for( ly = 0; ly < px_scale; ly++ )
			{
				for( lx = 0; lx < px_scale; lx++ )
				{
					pxloc[lx] = pxcol;
				}
				pxloc += OLED_WIDTH * px_scale;
			}
		}
	}
}

void emuCheckResize()
{
	CNFGGetDimensions( &screenx, &screeny );
	int targsx = screenx / OLED_WIDTH;
	if( targsx != px_scale )
	{
		px_scale = targsx;
		printf( "Rescaling OLED to scale %d\n", px_scale );
		rawvidmem = realloc( rawvidmem, px_scale * OLED_WIDTH*px_scale * (OLED_HEIGHT+FOOTER_PIXELS)*px_scale * 4 );
		updateOLED( FALSE );
	}
}


int main()
{
	int i, x, y;
	double ThisTime;
	double LastFPSTime = OGGetAbsoluteTime();
	double LastFrameTime = OGGetAbsoluteTime();
	double SecToWait;
	int linesegs = 0;

	CNFGBGColor = 0x800000;
	CNFGDialogColor = 0x444444;
	CNFGSetup( "swadgemu", OLED_WIDTH*px_scale, px_scale * ( OLED_HEIGHT + FOOTER_PIXELS ) );
	rawvidmem = malloc( px_scale * OLED_WIDTH*px_scale * (OLED_HEIGHT+FOOTER_PIXELS)*px_scale * 4 );

	// CNFGSetupFullscreen( "Test Bench", 0 );

	initOLED(0);

	while(1)
	{
		int i, pos;
		float f;
		iframeno++;

		updateOLED(0);

		CNFGHandleInput();

		CNFGClearFrame();
		CNFGColor( 0xFFFFFF );
		emuCheckResize();

		emuFooter();
		CNFGUpdateScreenWithBitmap( rawvidmem, OLED_WIDTH*px_scale, (OLED_HEIGHT+FOOTER_PIXELS)*px_scale  );

		frames++;
		//CNFGSwapBuffers();

		ThisTime = OGGetAbsoluteTime();
		if( ThisTime > LastFPSTime + 1 )
		{
			printf( "FPS: %d\n", frames );
			frames = 0;
			linesegs = 0;
			LastFPSTime+=1;
		}

		SecToWait = .016 - ( ThisTime - LastFrameTime );
		LastFrameTime += .016;
		if( SecToWait > 0 )
			OGUSleep( (int)( SecToWait * 1000000 ) );
	}

	return(0);
}



//General emulation stubs.

unsigned long os_random() { return rand(); }
void  * ets_memcpy( void * dest, const void * src, size_t n ) { memcpy( dest, src, n ); return dest; }
void  * ets_memset( void * s, int c, size_t n ) { memset( s, c, n ); return s; }
void system_set_os_print( uint8 onoff ) { }
void LoadDefaultPartitionMap(void) {}
struct rst_info* system_get_rst_info(void)
{
	static struct rst_info srst; 
	srst.reason = REASON_DEFAULT_RST;
	srst.exccause = 0;
	srst.epc1 = 0xbaad0001;
	srst.epc2 = 0xbaad0002;
	srst.epc3 = 0xbaad0003;
	srst.excvaddr = 0xbaadbeef;
	srst.depc = 0xcafebeef;
	return &srst;
}

bool system_rtc_mem_read(uint8 src_addr, void *des_addr, uint16 load_size)
{
	FILE * f = fopen( "rtc.dat", "rb" );
	if( !f )
	{
		f = fopen( "rtc.dat", "wb" );
		if( f )
		{
			uint8_t * raw = malloc(8192);
			memset( raw, 0, 8192 );
			fwrite( raw, 8192, 1, f );
			fclose( f );
			free( raw );
		}
		f = fopen( "rtc.dat", "rb" );
	}
	if( !f )
	{
		fprintf( stderr, "EMU Error: Could not open rtc.dat for reading/writing\n" );
		return false;
	}
	fseek( f, SEEK_SET, src_addr );
	if( fread( des_addr, load_size, 1, f ) != 1 )
	{
		fprintf( stderr, "EMU Error: Could not load data out of rtc.dat\n" );
		fclose( f );
		return false;
	}
	fclose( f );
	return true;
}

uint8 wifi_opmode;

bool wifi_set_opmode_current(uint8 opmode)
{
	wifi_opmode = opmode;
	return true;
}

bool wifi_set_opmode(uint8 opmode )
{
	fprintf( stderr, "EMU Warning: TODO: wifi_set_opmode does not save wireless state\n" );
	return true;
}

void espNowInit(void)
{
	fprintf( stderr, "EMU Warning: TODO: need to implement espNow as a broadcast UDP system\n" );	
}

void espNowDeinit()
{
}

void ws2812_init()
{
}

void cnlohr_i2c_setup(uint32_t clock_stretch_time_out_usec)
{
}

void initMic(void)
{
}

void initBuzzer(void)
{
}

void setBuzzerNote(uint16_t note)
{
	fprintf( stderr, "EMU Warning: TODO: need to implement setBuzzerNote(...)\n" );	
}


void SetupGPIO(bool enableMic)
{
}

bool system_os_task(os_task_t task, uint8 prio, os_event_t *queue, uint8 qlen)
{
	fprintf( stderr, "EMU Warning: TODO: need to implement system_os_task(...)\n" );	
}

bool system_os_post(uint8 prio, os_signal_t sig, os_param_t par)
{
	fprintf( stderr, "EMU Warning: TODO: need to implement system_os_post(...)\n" );	
}

