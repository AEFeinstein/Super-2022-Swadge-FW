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

#include "../user/user_main.h"
#include "../user/hdw/QMA6981.h"
#include "../user/hdw/buzzer.h"
#include "spi_flash.h"

#define BACKGROUND_COLOR  0x051923
#define BACKGROUND_COLOR2 0x1B2845
#define OLED_ON_COLOR    0x2191FB
#define FOREGROUND_COLOR 0xD00000

unsigned frames = 0;
unsigned long iframeno = 0;
int px_scale = INIT_PX_SCALE;
uint32_t * rawvidmem;
short screenx, screeny;
uint32_t footerpix[FOOTER_PIXELS*OLED_WIDTH];
uint32_t ws2812s[NR_WS2812];
double boottime;



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
	int x, y, lx = 0;
	int ledno;
	for( ledno = 0; ledno < NR_WS2812; ledno++ )
	{
		uint32_t wscol = ws2812s[ledno];
		for( y = 0; y < 10; y++ )
		{
			for( x = 0; x < OLED_WIDTH/NR_WS2812-1; x++ )
			{
				footerpix[lx+x+y*OLED_WIDTH] = wscol;
			}
			footerpix[lx+x+y*OLED_WIDTH] = BACKGROUND_COLOR2;
		}
		lx += OLED_WIDTH/NR_WS2812;
	}
	for( y = 10; y < FOOTER_PIXELS; y++ )
	{
		for( x = 0; x < OLED_WIDTH; x++ )
		{
			footerpix[x+y*OLED_WIDTH] = BACKGROUND_COLOR;
		}
	}
	int i;
	for( i = 0; i < NR_WS2812; i++ )
	{
		//TODO: Iterate over ws2812s and display them here.
	}
	emuSendOLEDData( 1, (uint8_t*)footerpix );
}


void emuSendOLEDData( int disp, uint8_t * currentFb )
{
	int x, y;
	for( y = 0; y < (disp?FOOTER_PIXELS:OLED_HEIGHT); y++ )
	{
		for( x = 0; x < OLED_WIDTH; x++ )
		{
			uint32_t pxcol;
			if( disp == 0 )
			{
				uint8_t col = currentFb[(x + (y / 8) * OLED_WIDTH)] & (1 << (y & 7));
				pxcol = col?(disp?0xffffffff:0xff80ffff):0x00000000;
			}
			else
			{
				pxcol = ((uint32_t*)currentFb)[x+y*OLED_WIDTH];
			}
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

	boottime = OGGetAbsoluteTime();

	initOLED(0);

	void user_init();
	user_init();

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
int ets_memcmp( const void * a, const void * b, size_t n ) { return memcmp( a, b, n ); }
int ets_strlen( const char * s ) { return strlen( s ); }
char * ets_strncpy ( char * destination, const char * source, size_t num ) { return strncpy( destination, source, num ); }
int ets_strcmp (const char* str1, const char* str2) { return strcmp( str1, str2 ); }
void system_set_os_print( uint8 onoff ) { }
void LoadDefaultPartitionMap(void) {}
uint32 system_get_time(void) { return (OGGetAbsoluteTime()-boottime)*1000000; }
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

static void system_rtc_init()
{
	FILE * f = fopen( "rtc.dat", "wb" );
	if( f )
	{
		uint8_t * raw = malloc(512);
		memset( raw, 0, 512 );
		fwrite( raw, 512, 1, f );
		fclose( f );
		free( raw );
	}
	else
	{
		fprintf( stderr, "EMU Error: Could not open rtc.dat for reading/writing\n" );
	}
}


bool system_rtc_mem_write(uint8 des_addr, const void *src_addr, uint16 save_size)
{
	FILE * f = fopen( "rtc.dat", "wb+" );
	if( !f )
	{
		fprintf( stderr, "EMU Error: Could not open rtc.dat for reading/writing\n" );
		return false;
	}
	fseek( f, SEEK_SET, des_addr );
	fwrite( src_addr, save_size, 1, f );
	fclose( f );
}

bool system_rtc_mem_read(uint8 src_addr, void *des_addr, uint16 load_size)
{
	FILE * f = fopen( "rtc.dat", "rb" );
	if( !f )
	{
		system_rtc_init();
		f = fopen( "rtc.dat", "rb" );
	}
	if( !f )
	{
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


void ws2812_init()
{
}

void cnlohr_i2c_setup(uint32_t clock_stretch_time_out_usec)
{
}

void ets_intr_lock()
{
}

void ets_intr_unlock()
{
}


void ws2812_push( uint8_t* buffer, uint16_t buffersize )
{
	if( buffersize / 3 > sizeof( ws2812s ) / 4 )
	{
		fprintf( stderr, "EMU WARNING: ws2812_push invalid\n" );
		return;
	}
	int led = 0;
	for( ; led < buffersize/3; led++ )
	{
		uint32_t col = 0xff000000;
		col |= buffer[led*3+2]<<16;
		col |= buffer[led*3+0]<<8;
		col |= buffer[led*3+1]<<0;
		ws2812s[led] = col;
	}
}

///////////////////////////////////////////////////////////////////////////////////////

void * os_malloc( int x ) { return malloc( x ); }
void os_free( void * x ) { free( x ); }


////////////////////////////////////////////////////////////////////////////////////////
// Sound system (need to write)
#include "sound/sound.h"

struct SoundDriver * sounddriver;
#define SSBUF 8192
uint8_t ssamples[SSBUF];
int sshead;
int sstail;

void EMUSoundCBType( float * out, float * in, int samplesr, int * samplesp, struct SoundDriver * sd )
{
	int i;
	for( i = 0; i < samplesr; i++ )
	{
		if( sstail != (( sshead + 1 ) % SSBUF) )
		{
			ssamples[sshead] = (int)( (in[i]*0.5 + 0.5) * 255);
			sshead = ( sshead + 1 ) % SSBUF;
		}
	}
	*samplesp = 0;
}

void initMic(void)
{
	printf( "EMU: initMic()\n" );
	sounddriver = InitSound( 0 /* You could specify ALSA/Pulse etc. here */, EMUSoundCBType );
}

void initBuzzer(void)
{
}

void setBuzzerNote(uint16_t note)
{
}

uint8_t getSample(void)
{
	if( sshead != sstail )
	{
		uint8_t r = ssamples[sstail];
		sstail = (sstail+1)%SSBUF;
		return r;
	}
	else
		return 0;
}

bool sampleAvailable(void)
{
	return FALSE;
}

void PauseHPATimer()
{
}

void ContinueHPATimer()
{
}

void stopBuzzerSong(void)
{
}

void  startBuzzerSong(const song_t* song, bool shouldLoop)
{
}



void SetupGPIO(bool enableMic)
{
}

void setGpiosForBoot(void)
{
}

////////////////////////////////////////////////////////////////////////////////
//TODO: Need to write accelerometer.

void QMA6981_poll(accel_t* currentAccel)
{
}

bool QMA6981_setup(void)
{
}

////////////////////////////////////////////////////////////////////////////////


void ets_timer_disarm(ETSTimer *a)
{
	fprintf( stderr, "EMU Warning: TODO: need to implement ets_timer_disarm(...)\n" );	
}

void ets_timer_setfn(ETSTimer *t, ETSTimerFunc *fn, void *parg)
{
	fprintf( stderr, "EMU Warning: TODO: need to implement ets_timer_setfn(...)\n" );	
}

void ets_timer_arm_new(ETSTimer *a, int b, int c, int isMstimer)
{
	fprintf( stderr, "EMU Warning: TODO: need to implement ets_timer_arm_new(...)\n" );	
}


bool system_os_task(os_task_t task, uint8 prio, os_event_t *queue, uint8 qlen)
{
	fprintf( stderr, "EMU Warning: TODO: need to implement system_os_task(...)\n" );	
}

bool system_os_post(uint8 prio, os_signal_t sig, os_param_t par)
{
	fprintf( stderr, "EMU Warning: TODO: need to implement system_os_post(...)\n" );	
}


/////////////////////////////////////////////////////////////////////////////////////////////////

void espNowInit(void)
{
	fprintf( stderr, "EMU Warning: TODO: need to implement espNow as a broadcast UDP system\n" );	
}

void espNowDeinit()
{
}

void ICACHE_FLASH_ATTR espNowSend(const uint8_t* data, uint8_t len)
{
	fprintf( stderr, "EMU Warning: TODO: need to implement espNow as a broadcast UDP system\n" );	
}


bool wifi_get_macaddr(uint8 if_index, uint8 *macaddr)
{
	fprintf( stderr, "EMU Warning: TODO: need to implement wifi_get_macaddr\n" );	
}

/////////////////////////////////////////////////////////////////////////////////////////////////
//Deep sleep.  How do we want to handle it?

bool system_deep_sleep_set_option(uint8 option)
{
	fprintf( stderr, "EMU Warning: TODO: need to implement system_deep_sleep_set_option(...)\n" );
}

bool system_deep_sleep_instant(uint64 time_in_us)
{
	fprintf( stderr, "EMU Warning: TODO: need to implement system_deep_sleep_set_option(...)\n" );
}

bool system_deep_sleep(uint64 time_in_us)
{
	fprintf( stderr, "EMU Warning: TODO: need to implement system_deep_sleep(...)\n" );
}


/////////////////////////////////////////////////////////////////////////////////////////////////
//XXX TODO: Handle this more gracefully and test it.


static void system_flash_init()
{
	FILE * f = fopen( "flash.dat", "wb" );
	if( f )
	{
		uint8_t * raw = malloc(1024*1024*2);
		memset( raw, 0, 1024*1024*2 );
		fwrite( raw, 1024*1024*2, 1, f );
		fclose( f );
		free( raw );
	}
	else
	{
		fprintf( stderr, "EMU Error: Could not open rtc.dat for reading/writing\n" );
	}
}


SpiFlashOpResult spi_flash_erase_sector(uint16 sec)
{
	FILE * f = fopen( "flash.dat", "wb+" );
	if( !f )
	{
		fprintf( stderr, "EMU Error: Could not open rtc.dat for reading/writing\n" );
		return SPI_FLASH_RESULT_ERR;
	}
	fseek( f, SEEK_SET, sec *  SPI_FLASH_SEC_SIZE );
	uint8_t * erased = malloc(  SPI_FLASH_SEC_SIZE );
	memset( erased, 0xff, SPI_FLASH_SEC_SIZE );
	fwrite( erased, SPI_FLASH_SEC_SIZE, 1, f );
	free( erased );
	fclose( f );
	return SPI_FLASH_RESULT_OK;
}

SpiFlashOpResult spi_flash_write(uint32 des_addr, uint32 *src_addr, uint32 size)
{
	FILE * f = fopen( "flash.dat", "wb+" );
	if( !f )
	{
		fprintf( stderr, "EMU Error: Could not open rtc.dat for reading/writing\n" );
		return SPI_FLASH_RESULT_ERR;
	}
	fseek( f, SEEK_SET, des_addr );
	fwrite( src_addr, size, 1, f );
	fclose( f );
	return SPI_FLASH_RESULT_OK;
}

SpiFlashOpResult spi_flash_read(uint32 src_addr, uint32 *des_addr, uint32 size)
{
	FILE * f = fopen( "flash.dat", "rb" );
	if( !f )
	{
		system_flash_init();
		f = fopen( "flash.dat", "rb" );
	}

	if( !f )
	{
		fprintf( stderr, "EMU Error: Could not open flash.dat for reading/writing\n" );
		return SPI_FLASH_RESULT_ERR;
	}	

	fseek( f, SEEK_SET, src_addr );
	fread( des_addr, size, 1, f );
	fclose( f );
	return SPI_FLASH_RESULT_OK;
}


