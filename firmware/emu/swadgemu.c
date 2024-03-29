//Copyright (c) 2011 <>< Charles Lohr - Under the MIT/x11 or NewBSD License you choose.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define _GNU_SOURCE /* for tm_gmtoff and tm_zone */
#include <time.h>
#include "rawdraw/CNFG.h"
#include "rawdraw/os_generic.h"
#include "swadgemu.h"
#include "ip_addr.h"
#include "espconn.h"

//ESP Includes
#include "user_interface.h"

#include "../user/user_main.h"
#include "../user/hdw/QMA6981.h"
#include "../user/hdw/buzzer.h"
#include "../user/hdw/buttons.h"
#include "../user/utils/assets.h"
#include "spi_flash.h"

#define BACKGROUND_COLOR  0x000000
#define BACKGROUND_COLOR2 0x1B2845
#define OLED_ON_COLOR    0xFFFFFF
#define FOREGROUND_COLOR 0xD00000

#define NR_BUTTONS 5

#ifdef ANDROID
    #define LOGI(...)  ((void)__android_log_print(/*ANDROID_LOG_INFO*/4, APPNAME, __VA_ARGS__))
    #define printf( x...) LOGI( x )
#endif

#if !defined(WINDOWS) && !defined(ANDROID)
    #define LINUX
#endif


#ifdef LINUX
    //For shm
    #include <sys/mman.h>
    #include <sys/stat.h>        /* For mode constants */
    #include <fcntl.h>           /* For O_* constants */

    int swadgeshm_video;
    int swadgeshm_input;
    uint32_t* swadgeshm_video_data;
    uint8_t* swadgeshm_input_data;
    size_t swadgeshm_video_data_size;
#endif

int px_scale = INIT_PX_SCALE;
uint32_t* rawvidmem;
short screenx, screeny;
uint32_t headerpix[HEADER_PIXELS * OLED_WIDTH];
uint32_t footerpix[FOOTER_PIXELS * OLED_WIDTH];
uint32_t ws2812s[NR_WS2812];
double boottime;

uint8_t gpio_status;

void HandleButtonStatus( int button, int bDown );
void system_os_check_tasks(void);
void ets_timer_check_timers(void);

//Really, this function is currently only used on Android.  TODO: Make the mouse actually do this.
void emuCheckFooterMouse( int x, int y, int finger, int bDown )
{
    static int8_t fingermap[10];
    if( finger >= 10 )
    {
        return;
    }
    if( !bDown )
    {
        fingermap[finger] = 0;
        //printf( "PX: UP %d\n", finger );
    }
    else
    {
        int pxx = x / px_scale;
        int pxy = y / px_scale;
        pxy -= OLED_HEIGHT + WS_HEIGHT;
        if( pxy < 0 )
        {
            return;
        }
        if( pxy >= BTN_HEIGHT )
        {
            return;
        }
        if( pxx >= OLED_WIDTH )
        {
            return;
        }
        int button = pxx * NR_BUTTONS / OLED_WIDTH;
        //printf( "PX: %d, %d / %d / %d, %d / %d\n", pxx, pxy, finger, x, y, bDown );
        fingermap[finger] = button + 1;
    }

    int b;
    for( b = 0; b < NR_BUTTONS; b++ )
    {
        int i;
        int bIsDown = 0;
        for( i = 0; i < 10; i++ )
        {
            if( (fingermap[i] - 1) == b )
            {
                bIsDown = true;
            }
        }
        HandleButtonStatus( b, bIsDown );
    }
}

void emuHeader()
{
    // Draw background color first
    for(int i = 0; i < HEADER_PIXELS * OLED_WIDTH; i++)
    {
        headerpix[i] = BACKGROUND_COLOR2;
    }

    // Then draw LEDs
    int x, y, xS, yS;
    for( int ledno = 0; ledno < NR_WS2812; ledno++ )
    {
        if(ledno < NR_WS2812 / 2)
        {
            xS = 0;
        }
        else
        {
            xS = OLED_WIDTH / 2;
        }

        if(ledno == 0 || ledno == 5)
        {
            yS = WS_HEIGHT * 2;
        }
        else if (ledno == 1 || ledno == 4)
        {
            yS = WS_HEIGHT;
        }
        else
        {
            yS = 0;
        }

        for( y = 0; y < WS_HEIGHT; y++ )
        {
            for( x = 0; x < OLED_WIDTH / 2; x++ )
            {
                headerpix[(xS + x) + ((yS + y) * OLED_WIDTH)] = ws2812s[ledno];
            }
        }
    }

    emuSendOLEDData( 0, (uint8_t*)headerpix );
}

void emuFooter()
{
    int x, y, lx;
    int btn;

    lx = 0;
    for( btn = 0; btn < NR_BUTTONS; btn++ )
    {
        uint32_t btncol = (gpio_status & (1 << btn)) ? FOREGROUND_COLOR : BACKGROUND_COLOR;
        for( y = 0; y < BTN_HEIGHT; y++ )
        {
            for( x = 0; x < OLED_WIDTH / NR_BUTTONS - 1; x++ )
            {
                footerpix[lx + x + y * OLED_WIDTH] = btncol;
            }
            footerpix[lx + x + y * OLED_WIDTH] = BACKGROUND_COLOR2;
        }
        lx += OLED_WIDTH / NR_BUTTONS;
    }

    // Fill extra space with BACKGROUND_COLOR
    for( y = BTN_HEIGHT; y < FOOTER_PIXELS; y++ )
    {
        for( x = 0; x < OLED_WIDTH; x++ )
        {
            footerpix[x + y * OLED_WIDTH] = BACKGROUND_COLOR;
        }
    }

    // Draw bar between OLED and buttons
    for( x = 0; x < OLED_WIDTH; x++ )
    {
        footerpix[x] = BACKGROUND_COLOR2;
    }
    emuSendOLEDData( 2, (uint8_t*)footerpix );
}


void emuSendOLEDData( int disp, uint8_t* currentFb )
{
    int x, y;
    int yStart, yHeight;
    switch(disp)
    {
        case 0:
        {
            // Header
            yStart = 0;
            yHeight = HEADER_PIXELS;
            break;
        }
        case 1:
        {
            // OLED
            yStart = HEADER_PIXELS;
            yHeight = OLED_HEIGHT;
            break;
        }
        case 2:
        {
            // Footer
            yStart = HEADER_PIXELS + OLED_HEIGHT;
            yHeight = FOOTER_PIXELS;
            break;
        }
        default:
        {
            return;
        }
    }

    for( y = 0; y < yHeight; y++ )
    {
        for( x = 0; x < OLED_WIDTH; x++ )
        {

            uint32_t pxcol;
            if( disp == 1 )
            {
                uint8_t col = currentFb[(y + x * OLED_HEIGHT) / 8] & (1 << (y & 7));
                pxcol = col ? OLED_ON_COLOR : BACKGROUND_COLOR;
            }
            else
            {
                pxcol = ((uint32_t*)currentFb)[x + y * OLED_WIDTH];
            }
#ifdef ANDROID
            pxcol = 0xff000000 | ( (pxcol & 0xff) << 16 ) | ( pxcol & 0xff00 ) | ( (pxcol & 0xff0000) >> 16 );
#endif
            int lx, ly;
            uint32_t* pxloc = rawvidmem + ( x +  ( ( y + yStart ) ) * OLED_WIDTH * px_scale ) * px_scale;
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
#ifdef LINUX
        // Unmap old memory
        munmap(swadgeshm_video_data, swadgeshm_video_data_size);

        // Figure out new size
        int rawvmsize = px_scale * OLED_WIDTH * px_scale * (HEADER_PIXELS + OLED_HEIGHT + FOOTER_PIXELS) * px_scale * 4;
        swadgeshm_video_data_size = rawvmsize + 64;

        // Resize the file, should still be open
        ftruncate( swadgeshm_video, swadgeshm_video_data_size);

        // Remap memory
        swadgeshm_video_data = (uint32_t*)mmap(0, swadgeshm_video_data_size, PROT_READ | PROT_WRITE, MAP_SHARED,
                                               swadgeshm_video, 0);
        swadgeshm_video_data[0] = px_scale * OLED_WIDTH;
        swadgeshm_video_data[1] = px_scale * OLED_HEIGHT;
        //[0] = width
        //[1] = height
        //[4..12] = LEDs
        rawvidmem = swadgeshm_video_data + 16;
#else
        rawvidmem = realloc( rawvidmem, px_scale * OLED_WIDTH * px_scale * (HEADER_PIXELS + OLED_HEIGHT + FOOTER_PIXELS) *
                             px_scale * 4 );
#endif
        updateOLED( false );
    }
}

// void exitMode(void)
// {
//  printf("called on exit");
//  exitCurrentSwadgeMode();
// }

#ifndef ANDROID
    int main()
#else
    int emumain()
#endif
{
    unsigned frames = 0;
    int i, x, y;
    double ThisTime;
    double LastFPSTime = OGGetAbsoluteTime();
    double LastFrameTime = OGGetAbsoluteTime();
    double SecToWait;
    int linesegs = 0;

    CNFGBGColor = 0x800000;
    // CNFGDialogColor = 0x444444;
    CNFGSetup( "swadgemu", OLED_WIDTH * px_scale, px_scale * ( HEADER_PIXELS + OLED_HEIGHT + FOOTER_PIXELS ) );
    int rawvmsize = px_scale * OLED_WIDTH * px_scale * (HEADER_PIXELS + OLED_HEIGHT + FOOTER_PIXELS) * px_scale * 4;

    // atexit(exitMode);
    // CNFGSetupFullscreen( "Test Bench", 0 );

#ifdef WINDOWS
    void REGISTERSoundWin();
    REGISTERSoundWin();
#endif

#ifdef LINUX
    swadgeshm_video = shm_open("/swadgevideo", O_CREAT | O_RDWR, 0644);
    swadgeshm_input = shm_open("/swadgeinput", O_CREAT | O_RDWR, 0644);
    ftruncate( swadgeshm_input, 10 );
    swadgeshm_input_data = mmap(0, 10, PROT_READ | PROT_WRITE, MAP_SHARED, swadgeshm_input, 0);

    swadgeshm_video_data_size = rawvmsize + 64;
    ftruncate( swadgeshm_video, swadgeshm_video_data_size);
    swadgeshm_video_data = (uint32_t*)mmap(0, swadgeshm_video_data_size, PROT_READ | PROT_WRITE, MAP_SHARED,
                                           swadgeshm_video, 0);
    swadgeshm_video_data[0] = px_scale * OLED_WIDTH;
    swadgeshm_video_data[1] = px_scale * OLED_HEIGHT;
    //[0] = width
    //[1] = height
    //[4..12] = LEDs
    rawvidmem = swadgeshm_video_data + 16;
#else
    rawvidmem = malloc( rawvmsize );
#endif

    boottime = OGGetAbsoluteTime();

    srand(time(NULL));

    initOLED(0);

    void user_init();
    user_init();

    while(1)
    {
        int i, pos;
        float f;

        system_os_check_tasks();
        ets_timer_check_timers();

        updateOLED(0);

        CNFGHandleInput();
#ifdef LINUX
        //Handle input from SHM.
        if( swadgeshm_input_data[6] )
        {
            for( i = 0; i < 5; i++ )
            {
                HandleButtonStatus( i, swadgeshm_input_data[i] );
            }
        }
#endif

        CNFGClearFrame();
        CNFGColor( 0xFFFFFF );
        emuCheckResize();

        emuHeader();
        emuFooter();
        CNFGUpdateScreenWithBitmap( rawvidmem, OLED_WIDTH * px_scale, (HEADER_PIXELS + OLED_HEIGHT + FOOTER_PIXELS)*px_scale  );

        frames++;
        //CNFGSwapBuffers();

        ThisTime = OGGetAbsoluteTime();
        if( ThisTime > LastFPSTime + 1 )
        {
            // printf( "FPS: %d\n", frames );
            frames = 0;
            linesegs = 0;
            LastFPSTime += 1;
        }

        SecToWait = .01 - ( ThisTime - LastFrameTime );
        LastFrameTime += .01;
        if( SecToWait > 0 )
        {
            OGUSleep( (int)( SecToWait * 1000000 ) );
        }
    }

    return(0);
}



//General emulation stubs.
unsigned long os_random()
{
    return rand();
}
void*   ets_memcpy( void* dest, const void* src, size_t n )
{
    memcpy( dest, src, n );
    return dest;
}
void*   ets_memset( void* s, int c, size_t n )
{
    memset( s, c, n );
    return s;
}
void*   ets_memmove(void* dest, const void* src, size_t n)
{
    return memmove(dest, src, n);
}
int ets_memcmp( const void* a, const void* b, size_t n )
{
    return memcmp( a, b, n );
}
int ets_strlen( const char* s )
{
    return strlen( s );
}
char* ets_strcpy(char* dest, const char* src)
{
    return strcpy(dest, src);
}
char* ets_strncpy ( char* destination, const char* source, size_t num )
{
    return strncpy( destination, source, num );
}
int ets_strcmp (const char* str1, const char* str2)
{
    return strcmp( str1, str2 );
}
char* ets_strcat(char* dest, const char* src)
{
    return strcat(dest, src);
}
int ets_strncmp(const char* s1, const char* s2, int len)
{
    return strncmp(s1, s2, len);
}

bool canPrint = true;
void system_set_os_print( uint8 onoff )
{
    canPrint = onoff;
}
int os_printf(const char* format, ...)
{
    if(canPrint)
    {
        va_list argp;
        va_start(argp, format);
        int out = vprintf(format, argp);
        va_end(argp);
        return out;
    }
    return 0;
};
int os_sprintf(char* str, const char* format, ...)
{
    if(canPrint)
    {
        va_list argp;
        va_start(argp, format);
        int out = vsprintf(str, format, argp);
        va_end(argp);
        return out;
    }
    return 0;
};
void LoadDefaultPartitionMap(void) {}
uint32 system_get_time(void)
{
    return (OGGetAbsoluteTime() - boottime) * 1000000;
}

struct rst_info srst =
{
    .reason = REASON_DEFAULT_RST,
    .exccause = 0,
    .epc1 = 0xbaad0001,
    .epc2 = 0xbaad0002,
    .epc3 = 0xbaad0003,
    .excvaddr = 0xbaadbeef,
    .depc = 0xcafebeef
};

void system_set_rst_reason(uint32_t reason)
{
    srst.reason = reason;
}

struct rst_info* system_get_rst_info(void)
{
    return &srst;
}

static void system_rtc_init()
{
    FILE* f = fopen( "rtc.dat", "wb" );
    if( f )
    {
        uint8_t* raw = malloc(512);
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


bool system_rtc_mem_write(uint8 des_addr, const void* src_addr, uint16 save_size)
{
    FILE* f = fopen( "rtc.dat", "wb+" );
    if( !f )
    {
        fprintf( stderr, "EMU Error: Could not open rtc.dat for reading/writing\n" );
        return false;
    }
    fseek( f, SEEK_SET, des_addr );
    fwrite( src_addr, save_size, 1, f );
    fclose( f );
    return true;
}

bool system_rtc_mem_read(uint8 src_addr, void* des_addr, uint16 load_size)
{
    FILE* f = fopen( "rtc.dat", "rb" );
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
    for( ; led < buffersize / 3; led++ )
    {
        uint32_t col = 0xff000000;
        col |= (buffer[led * 3 + 1] * 240 / 255 + 15) << 16; // r
        col |= (buffer[led * 3 + 0] * 240 / 255 + 15) << 8; // g
        col |= (buffer[led * 3 + 2] * 240 / 255 + 15) << 0; // b
        ws2812s[led] = col;
#ifdef LINUX
        swadgeshm_video_data[4 + led] = col;
#endif
    }
}

///////////////////////////////////////////////////////////////////////////////////////

void* os_malloc( int x )
{
    // Allocate some space
    void* ptr = malloc( x );
    // Fill the pointer with garbage, ESP-style
    if(NULL != ptr)
    {
        for( int i = 0; i < x; i++ )
        {
            ((uint8_t*)ptr)[i] = rand() & 0xff;
        }
    }
    // Return the space
    return ptr;
}
void* os_zalloc( int x )
{
    void* ptr = malloc( x );
    if(NULL != ptr)
    {
        memset(ptr, 0, x);
    }
    return ptr;
}
void os_free( void* x )
{
    free( x );
}

////////////////////////////////////////////////////////////////////////////////////////
// Sound system (need to write)
#include "sound/sound.h"
struct SoundDriver* sounddriver;
#define SSBUF 8192
uint8_t ssamples[SSBUF];
int sshead;
int sstail;
void ICACHE_FLASH_ATTR songTimerCb(void* arg __attribute__((unused)));
void stopBuzzerSong(void);
void ICACHE_FLASH_ATTR loadNextNote(void);
struct
{
    uint16_t currNote; // Actually a clock divisor
    uint16_t currDuration;
    const song_t* song;
    bool songShouldLoop;
    uint32_t noteTime;
    uint32_t noteIdx;
    timer_t songTimer;
} bzr = {0};

uint16_t buzzernote;
og_mutex_t* buzzernotemutex;
int getIsMutedOption();

#ifndef ANDROID
    #define BZR_PRINTF printf
#else
    #define BZR_PRINTF LOGI
#endif

void EMUSoundCBType( struct SoundDriver* sd, short* in, short* out, int samplesr, int samplesp )
{
    int i;
    if( samplesr )
    {
        for( i = 0; i < samplesr; i++ )
        {
            if( sstail != (( sshead + 1 ) % SSBUF) )
            {
                int v = in[i];
#ifdef ANDROID
                v *= 5;
                if( v > 32767 )
                {
                    v = 32767;
                }
                else if( v < -32768 )
                {
                    v = -32768;
                }
#endif
                ssamples[sshead] = (v / 256) + 128;
                sshead = ( sshead + 1 ) % SSBUF;
            }
        }
    }

    if( samplesp && out )
    {
        static uint16_t iplaceinwave;

        OGLockMutex( buzzernotemutex );
        if ( buzzernote )
        {
            for( i = 0; i < samplesp; i++ )
            {
                out[i] = 16384 * sin( (3.1415926 * 2.0 * iplaceinwave) / ((float)(buzzernote)) ); //sineaev
                iplaceinwave += 156; //Actually 156.255 5682/(16000/440)
                iplaceinwave = iplaceinwave % buzzernote;
            }
        }
        else
        {
            memset( out, 0, samplesp * 2 );
        }
        OGUnlockMutex( buzzernotemutex );
    }
}

void initMic(void)
{
    if( !buzzernotemutex )
    {
        buzzernotemutex = OGCreateMutex();
    }
    if( !sounddriver )
    {
        sounddriver = InitSound( 0, EMUSoundCBType, 16000, 1, 1, 256, 0, 0 );
    }
}

uint8_t getSample(void)
{
    if( sshead != sstail )
    {
        uint8_t r = ssamples[sstail];
        sstail = (sstail + 1) % SSBUF;
        return r;
    }
    else
    {
        return 0;
    }
}

bool sampleAvailable(void)
{
    return sstail != sshead;
}

void initBuzzer(void)
{
    stopBuzzerSong();
    if( !sounddriver )
    {
        sounddriver = InitSound( 0, EMUSoundCBType, 16000, 1, 1, 256, 0, 0 );
    }

    // Keep it high in the idle state
    //setBuzzerGpio(false);
    // If it's muted, don't set anything
    if(getIsMutedOption())
    {
        return;
    }

    ets_memset(&bzr, 0, sizeof(bzr));
    timerSetFn(&bzr.songTimer, songTimerCb, NULL);
}


/**
 * Set the song currently played by the buzzer. The pointer will be saved, but
 * no memory will be copied, so don't modify it!
 *
 * @param song A pointer to the song_t struct to be played
 * @param shouldLoop true to loop the song, false otherwise
 */
void ICACHE_FLASH_ATTR startBuzzerSong(const song_t* song, bool shouldLoop)
{
    BZR_PRINTF("%s, %d notes, %d internote pause\n", __func__, song->numNotes, song->interNotePause);
    // If it's muted, don't set anything
    if(getIsMutedOption())
    {
        return;
    }

    // Stop everything
    stopBuzzerSong();

    // Save the song pointer
    bzr.song = song;
    bzr.songShouldLoop = shouldLoop;

    // Set the timer to call every 1ms
    timerArm(&bzr.songTimer, 1, true);

    // Start playing the first note
    loadNextNote();
}

void setBuzzerNote( uint16_t note )
{
    if( !buzzernotemutex )
    {
        buzzernotemutex = OGCreateMutex();
    }
    OGLockMutex( buzzernotemutex );
    buzzernote = note;
    OGUnlockMutex( buzzernotemutex );
}

/**
 * @brief Load the next note from the song's ROM and play it
 */
void ICACHE_FLASH_ATTR loadNextNote(void)
{
    uint32_t noteAndDuration = bzr.song->notes[bzr.noteIdx];
    bzr.currDuration = (noteAndDuration >> 16) & 0xFFFF;
    setBuzzerNote(noteAndDuration & 0xFFFF);

    BZR_PRINTF("%s n:%5d d:%5d\n", __func__, bzr.currNote, bzr.currDuration);
}

/**
 * Stops the song currently being played
 */
void ICACHE_FLASH_ATTR stopBuzzerSong(void)
{
    BZR_PRINTF("%s\n", __func__);

    setBuzzerNote(SILENCE);
    bzr.song = NULL;
    bzr.noteTime = 0;
    bzr.noteIdx = 0;
    timerDisarm(&bzr.songTimer);
}

/**
 * A function called every millisecond. It advances through the song_t struct
 * and plays notes. It will loop the song if shouldLoop is set
 *
 * @param arg unused
 */
void ICACHE_FLASH_ATTR songTimerCb(void* arg __attribute__((unused)))
{
    // If it's muted, don't set anything
    if(getIsMutedOption())
    {
        return;
    }

    // Increment the time
    bzr.noteTime++;

    // Check if it's time for a new note
    if(bzr.noteTime >= bzr.currDuration)
    {
        // This note's time elapsed, try playing the next one
        bzr.noteIdx++;
        bzr.noteTime = 0;
        if(bzr.noteIdx < bzr.song->numNotes)
        {
            // There's another note to play, so play it
            loadNextNote();
        }
        else
        {
            // No more notes
            if(bzr.songShouldLoop)
            {
                BZR_PRINTF("Loop\n");
                // Song over, but should loop, so start again
                bzr.noteIdx = 0;
                loadNextNote();
            }
            else
            {
                BZR_PRINTF("Don't loop\n");
                // Song over, not looping, stop the timer and the note
                setBuzzerNote(SILENCE);
                timerDisarm(&bzr.songTimer);
            }
        }
    }
    else if ((bzr.currDuration > bzr.song->interNotePause) &&
             (bzr.noteTime >= bzr.currDuration - bzr.song->interNotePause))
    {
        // Pause a little between notes
        setBuzzerNote(SILENCE);
    }
}


void PauseHPATimer()
{
}

void ContinueHPATimer()
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
#ifndef ANDROID
void QMA6981_poll(accel_t* currentAccel)
{
}

bool QMA6981_setup(void)
{
    return false;
}
#endif

////////////////////////////////////////////////////////////////////////////////

ETSTimer* etsTimerList = NULL;

/**
 * Disarm the timer
 *
 * @param ptimer timer structure.
 */
void ets_timer_disarm(ETSTimer* ptimer)
{
    ptimer->timer_expire = 0;
    ptimer->timer_period = 0;

    // Unlink timer. Check if it's the head of the list
    if(NULL == etsTimerList)
    {
        // Nothing to unlink
        return;
    }
    else if(etsTimerList == ptimer)
    {
        // Timer is at the head, simple to remove
        etsTimerList = etsTimerList->timer_next;
    }
    else
    {
        // Timer is not at the head.
        // Iterate through the list, looking for a timer to remove
        ETSTimer* tmr = etsTimerList;
        while(NULL != tmr)
        {
            if(ptimer == tmr->timer_next)
            {
                // Timer is next, so unlink it
                tmr->timer_next = tmr->timer_next->timer_next;
                return;
            }
            else
            {
                // Iterate to the next
                tmr = tmr->timer_next;
            }
        }
    }
}

/**
 * Set timer callback function. The timer callback function must be set before
 * arming a timer.
 *
 * @param ptimer    timer structure.
 * @param pfunction timer callback function; use typecasting to pass function as
 *                  your function.
 * @param parg      callback function parameter
 */
void ets_timer_setfn(ETSTimer* ptimer, ETSTimerFunc* pfunction, void* parg)
{
    ptimer->timer_func = pfunction;
    ptimer->timer_arg = parg;
}

/**
 * Enable a millisecond timer.
 *
 * @param ptimer       timer structure
 * @param milliseconds timing; unit: millisecond.
 *                     If system_timer_reinit has been called, the timer value allowed ranges from 100 to 0x689D0.
 *                     If system_timer_reinit has NOT been called, the timer value allowed ranges from 5 to 0x68D7A3.
 * @param repeat_flag  whether the timer will be invoked repeatedly or not.
 * @param isMstimer    true if this is a ms timer, false if it is a us timer
 */
void ets_timer_arm_new(ETSTimer* ptimer, int milliseconds, int repeat_flag,
                       int isMstimer)
{
    if (milliseconds <= 0 || ptimer == NULL)
    {
        // Timer is null or is being registered for 0ms, don't allow that
        return;
    }

    if(NULL == etsTimerList)
    {
        // List is empty, set this timer as the head
        etsTimerList = ptimer;
        ptimer->timer_next = NULL;
    }
    else if (ptimer == etsTimerList)
    {
        // The first element of the list is the timer, don't add it twice
    }
    else
    {
        // List isn't empty and the head isn't this timer
        ETSTimer* tmr = etsTimerList;
        // Also check if the timer is already linked
        bool alreadyExists = false;
        // Iterate to the last element
        while(NULL != tmr->timer_next)
        {
            if(tmr->timer_next == ptimer)
            {
                // This timer is already linked in the list!
                alreadyExists = true;
                break;
            }
            else
            {
                tmr = tmr->timer_next;
            }
        }

        // If the timer wasn't in the list, link it
        if(false == alreadyExists)
        {
            tmr->timer_next = ptimer;
            ptimer->timer_next = NULL;
        }
    }

    // Timer was linked, now set up the params
    ptimer->timer_expire = milliseconds;
    if(repeat_flag)
    {
        ptimer->timer_period = milliseconds;
    }
    else
    {
        ptimer->timer_period = 0;
    }
}

/**
 * Check if timers have expired and call them. This will reset periodic
 * timers and unlink non-periodic timers when they expire.
 */
void ets_timer_check_timers(void)
{
    // Keep the last time this function was called
    static long long timeMs = -1;

    // Get the current time in milliseconds
    long long currTimeMs = (OGGetAbsoluteTime() - boottime) * 1000;

    // If time hasn't been initialized yet
    if(timeMs == -1)
    {
        // Initialize it
        timeMs = currTimeMs;
    }
    else
    {
        // While at least 1ms has elapsed
        while(timeMs != currTimeMs)
        {
            // Increment the static time
            timeMs++;

            // Iterate through all the timers
            ETSTimer* tmr = etsTimerList;
            while(NULL != tmr)
            {
                // If this timer is active
                if(tmr->timer_expire > 0)
                {
                    // Decrement the count
                    tmr->timer_expire--;
                    // If the timer expired
                    if(tmr->timer_expire == 0)
                    {
                        // Save the timer function and args
                        ETSTimerFunc* pfunction = tmr->timer_func;
                        void* parg = tmr->timer_arg;

                        // If the timer should repeat
                        if(tmr->timer_period)
                        {
                            // Reset the count
                            tmr->timer_expire = tmr->timer_period;
                        }
                        else
                        {
                            // Disarm non-repeating timers
                            ets_timer_disarm(tmr);
                        }

                        // Call the timer function
                        pfunction(parg);
                    }
                }

                // Iterate to the next
                tmr = tmr->timer_next;
            }
        }
    }
}

#define NUM_OS_TASKS 3

struct taskAndQueue
{
    os_task_t task;
    os_event_t* queue;
    uint8 qlen;
    uint8 qElems;
} os_tasks[NUM_OS_TASKS] = {{0}};

/**
 * @brief Register a system OS task
 *
 * @param task  task function
 * @param prio  task priority. Three priorities are supported: 0/1/2; 0 is the
 *              lowest priority. This means only 3 tasks are allowed to be set up.
 * @param queue message queue pointer
 * @param qlen  message queue depth
 * @return true if the task was registered, false if it was not
 */
bool system_os_task(os_task_t task, uint8 prio, os_event_t* queue, uint8 qlen)
{
    // If it's out of bounds or trying to register a null or empty queue
    if(prio >= NUM_OS_TASKS || queue == NULL || qlen == 0)
    {
        // Don't let that happen
        return false;
    }

    // Save the task, overwriting anything that exists
    os_tasks[prio].task = task;
    os_tasks[prio].queue = queue;
    os_tasks[prio].qlen = qlen;

    return true;
}

/**
 * @brief Post a signal & parameter to a system OS task
 *
 * @param task  task function
 * @param prio  task priority. Three priorities are supported: 0/1/2; 0 is the
 *              lowest priority. This means only 3 tasks are allowed to be set up.
 * @param queue message queue pointer
 * @param qlen  message queue depth
 * @return true if the task was registered, false if it was not
 */
bool system_os_post(uint8 prio, os_signal_t sig, os_param_t par)
{
    // If it's out of bounds or the event queue is full
    if(prio >= NUM_OS_TASKS || os_tasks[prio].qElems >= os_tasks[prio].qlen)
    {
        // Don't let that happen
        return false;
    }

    // Add this signal and parameter to the end of the task's queue
    os_tasks[prio].queue[os_tasks[prio].qElems].par = par;
    os_tasks[prio].queue[os_tasks[prio].qElems].sig = sig;
    os_tasks[prio].qElems++;
    return true;
}

/**
 * @brief Check for and dispatch any events to the OS tasks
 */
void system_os_check_tasks(void)
{
    // Service all tasks from high priority to low
    for(uint8_t i = 0; i < NUM_OS_TASKS; i++)
    {
        // If there is some event in the event queue
        if(os_tasks[i].qElems > 0)
        {
            // Create an event with the signal and parameter from the task queue
            ETSEvent evt =
            {
                .sig = os_tasks[i].queue[0].sig,
                .par = os_tasks[i].queue[0].par
            };

            // If the queue has space for more than one element
            if(os_tasks[i].qlen > 1)
            {
                // Shift all but the first element up one
                memmove(&os_tasks[i].queue[0], &os_tasks[i].queue[1], os_tasks[i].qlen - 1);
            }
            // Zero out the last element
            os_tasks[i].queue[os_tasks[i].qlen - 1].sig = 0;
            os_tasks[i].queue[os_tasks[i].qlen - 1].par = 0;
            os_tasks[i].qElems--;

            // Dispatch this event
            os_tasks[i].task(&evt);
        }
    }
}

/** Prints heap size for ESP, does nothing for EMU */
uint32 system_get_free_heap_size(void)
{
    return 0;
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


/////////////////////////////////////////////////////////////////////////////////////////////////
//Deep sleep.  How do we want to handle it?

bool system_deep_sleep_set_option(uint8 option)
{
    fprintf( stderr, "EMU Warning: TODO: need to implement system_deep_sleep_set_option(...)\n" );
    return true;
}

bool system_deep_sleep_instant(uint64 time_in_us)
{
    fprintf( stderr, "EMU Warning: TODO: need to implement system_deep_sleep_set_option(...)\n" );
    return true;
}

bool system_deep_sleep(uint64 time_in_us)
{
    fprintf( stderr, "EMU Warning: TODO: need to implement system_deep_sleep(...)\n" );
    return true;
}


/////////////////////////////////////////////////////////////////////////////////////////////////
//XXX TODO: Handle this more gracefully and test it.


static void system_flash_init()
{
    FILE* f = fopen( "flash.dat", "wb" );
    if( f )
    {
        uint8_t* raw = malloc(1024 * 1024 * 2);
        memset( raw, 0, 1024 * 1024 * 2 );
        fwrite( raw, 1024 * 1024 * 2, 1, f );
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
    FILE* f = fopen( "flash.dat", "wb+" );
    if( !f )
    {
        fprintf( stderr, "EMU Error: Could not open rtc.dat for reading/writing\n" );
        return SPI_FLASH_RESULT_ERR;
    }
    fseek( f, SEEK_SET, sec *  SPI_FLASH_SEC_SIZE );
    uint8_t* erased = malloc(  SPI_FLASH_SEC_SIZE );
    memset( erased, 0xff, SPI_FLASH_SEC_SIZE );
    fwrite( erased, SPI_FLASH_SEC_SIZE, 1, f );
    free( erased );
    fclose( f );
    return SPI_FLASH_RESULT_OK;
}

SpiFlashOpResult spi_flash_write(uint32 des_addr, uint32* src_addr, uint32 size)
{
    FILE* f = fopen( "flash.dat", "wb+" );
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

SpiFlashOpResult spi_flash_read(uint32 src_addr, uint32* des_addr, uint32 size)
{
    FILE* f = fopen( "flash.dat", "rb" );
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
    int success = fread( des_addr, size, 1, f );
    fclose( f );
    return (success == 1) ? SPI_FLASH_RESULT_OK : SPI_FLASH_RESULT_ERR;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
// Required functions

void HandleButtonStatus( int button, int bDown )
{
    if( bDown )
    {
        if( gpio_status & (1 << button) )
        {
            return;
        }
        gpio_status |= 1 << button;
    }
    else
    {
        if( !( gpio_status & (1 << button) ) )
        {
            return;
        }
        gpio_status &= ~(1 << button);
    }
    HandleButtonEventIRQ( gpio_status, button, (bDown) ? 1 : 0 );
}

#ifndef ANDROID

void HandleKey( int keycode, int bDown )
{
    if( keycode == 65307 )
    {
        exit( 0 );
    }
    // printf( "Key: %d -> %d\n", keycode, bDown );
    int button = -1;
    switch( keycode )
    {
        case 'w':
        case 'W':
            button = 3;
            break;
        case 's':
        case 'S':
            button = 1;
            break;
        case 'a':
        case 'A':
            button = 0;
            break;
        case 'd':
        case 'D':
            button = 2;
            break;
        case 'l':
        case 'L':
            button = 4;
            break;
    }
    if( button >= 0 )
    {
        HandleButtonStatus( button, bDown );
    }
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
    exitCurrentSwadgeMode();

    CloseSound(sounddriver);
    if(buzzernotemutex)
    {
        OGDeleteMutex(buzzernotemutex);
    }

#ifdef LINUX
    // Unmap old memory
    munmap(swadgeshm_video_data, swadgeshm_video_data_size);
    munmap(swadgeshm_input_data, 10);

    // Unlink shared memory
    shm_unlink("/swadgevideo");
    shm_unlink("/swadgeinput");
#else
    if(rawvidmem)
    {
        free(rawvidmem);
    }
#endif

    freeAssets();
}

#endif

/////////////////////////////////////////////////////////////////////////////////////////////////
// Wifi Stubs

bool wifi_get_macaddr(uint8 if_index, uint8* macaddr)
{
    static bool warned = false;
    if(!warned)
    {
        warned = true;
        fprintf( stderr, "EMU Warning: %s not implemented\n", __func__);
    }
    return true;
}

bool wifi_set_opmode_current(uint8 opmode)
{
    static bool warned = false;
    if(!warned)
    {
        warned = true;

        fprintf( stderr, "EMU Warning: %s not implemented\n", __func__);
    }
    return true;
}

bool wifi_set_opmode(uint8 opmode )
{
    static bool warned = false;
    if(!warned)
    {
        warned = true;
        fprintf( stderr, "EMU Warning: %s not implemented\n", __func__);
    }
    return true;
}

bool wifi_station_set_config(struct station_config* config)
{
    static bool warned = false;
    if(!warned)
    {
        warned = true;
        fprintf( stderr, "EMU Warning: %s not implemented\n", __func__);
    }
    return true;
}

bool wifi_station_connect(void)
{
    static bool warned = false;
    if(!warned)
    {
        warned = true;
        fprintf( stderr, "EMU Warning: %s not implemented\n", __func__);
    }
    return true;
}

void wifi_enable_signaling_measurement(void)
{
    static bool warned = false;
    if(!warned)
    {
        warned = true;
        fprintf( stderr, "EMU Warning: %s not implemented\n", __func__);
    }
}

sint8 wifi_station_get_rssi(void)
{
    static bool warned = false;
    if(!warned)
    {
        warned = true;
        fprintf( stderr, "EMU Warning: %s not implemented\n", __func__);
    }
    return 0;
}

bool wifi_station_scan(struct scan_config* config, scan_done_cb_t cb)
{
    static bool warned = false;
    if(!warned)
    {
        warned = true;
        fprintf( stderr, "EMU Warning: %s not implemented\n", __func__);
    }
    struct bss_info bss = {0};
    bss.channel = 11;
    bss.authmode = AUTH_OPEN;
    bss.rssi = 0;
    ets_strcpy((char*)(&bss.ssid[0]), "DUMMY_SSID");
    bss.ssid_len = strlen("DUMMY_SSID");
    cb(&bss, OK);
    return true;
}

bool wifi_get_ip_info(uint8 if_index, struct ip_info* info)
{
    static bool warned = false;
    if(!warned)
    {
        warned = true;
        fprintf( stderr, "EMU Warning: %s not implemented\n", __func__);
    }
    return true;
}

bool wifi_set_sleep_type(enum sleep_type type)
{
    static bool warned = false;
    if(!warned)
    {
        warned = true;
        fprintf( stderr, "EMU Warning: %s not implemented\n", __func__);
    }
    return true;
}



/////////////////////////////////////////////////////////////////////////////////////////////////
// SNTP Stubs

/**
 * get the seconds since Jan 01, 1970, 00:00 (GMT + 8)
 */
uint32 sntp_get_current_timestamp()
{
    return time(NULL);
}

/**
 * get real time (GTM + 8 time zone)
 */
char* sntp_get_real_time(long t)
{
    /* Define temporary variables */
    time_t current_time;
    struct tm* local_time;

    /* Retrieve the current time */
    current_time = time(NULL);

    /* Get the local time using the current time */
    local_time = localtime(&current_time);

    /* Return the local time */
    return asctime(local_time);
}

/**
 * SNTP get time_zone default GMT + 8
 */
sint8 sntp_get_timezone(void)
{
    time_t t = time(NULL);
    struct tm lt = {0};
    localtime_r(&t, &lt);

    return lt.tm_gmtoff;
}

/**
 * SNTP set time_zone (default GMT + 8)
 */
bool sntp_set_timezone(sint8 timezone)
{
    // Don't support time zones for emulation
    return false;
}

/**
 * Initialize this module.
 * Send out request instantly or after SNTP_STARTUP_DELAY(_FUNC).
 */
void sntp_init(void)
{
    // Do nothing, it's emulated
    return;
}

/**
 * Stop this module.
 */
void sntp_stop(void)
{
    // Do nothing, it's emulated
    return;
}

/**
 * Initialize one of the NTP servers by IP address
 *
 * @param numdns the index of the NTP server to set must be < SNTP_MAX_SERVERS
 * @param dnsserver IP address of the NTP server to set
 */
void sntp_setserver(unsigned char idx, ip_addr_t* addr)
{
    return;
}

/**
 * Obtain one of the currently configured by IP address (or DHCP) NTP servers
 *
 * @param numdns the index of the NTP server
 * @return IP address of the indexed NTP server or "ip_addr_any" if the NTP
 *         server has not been configured by address (or at all).
 */
ip_addr_t sntp_getserver(unsigned char idx)
{
    static ip_addr_t dummy = {0};
    return dummy;
}

/**
 * Initialize one of the NTP servers by name
 *
 * @param numdns the index of the NTP server to set must be < SNTP_MAX_SERVERS,now sdk support SNTP_MAX_SERVERS = 3
 * @param dnsserver DNS name of the NTP server to set, to be resolved at contact time
 */
void sntp_setservername(unsigned char idx, char* server)
{
    return;
}

/**
 * Obtain one of the currently configured by name NTP servers.
 *
 * @param numdns the index of the NTP server
 * @return IP address of the indexed NTP server or NULL if the NTP
 *         server has not been configured by name (or at all)
 */
char* sntp_getservername(unsigned char idx)
{
    return NULL;
}

struct tm* sntp_localtime(const time_t* tim_p)
{
    return localtime(tim_p);
}

sint8 espconn_accept(struct espconn* espconn)
{
    return 0;
}

sint8 espconn_create(struct espconn* espconn)
{
    return 0;
}

sint8 espconn_get_connection_info(struct espconn* pespconn, remot_info** pcon_info, uint8 typeflags)
{
    return 0;
}

sint8 espconn_send(struct espconn* espconn, uint8* psent, uint16 length)
{
    return 0;
}

sint16 espconn_sendto(struct espconn* espconn, uint8* psent, uint16 length)
{
    return 0;
}

sint8 espconn_regist_connectcb(struct espconn* espconn, espconn_connect_callback connect_cb)
{
    return 0;
}

sint8 espconn_regist_recvcb(struct espconn* espconn, espconn_recv_callback recv_cb)
{
    return 0;
}
