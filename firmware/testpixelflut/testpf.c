#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>

#define CNFG_IMPLEMENTATION
#define HAS_XSHAPE

#include "CNFG.h"

extern Display *CNFGDisplay;
extern Window CNFGWindow;

#define PORT 1337

int main( int argc, char ** argv )
{
	if( argc != 2 )
	{
		fprintf( stderr, "Usage: [testpf] [server IP]\n" );
		return -5;
	}
    void    CNFGPrepareForTransparency();
    CNFGPrepareForTransparency();

    CNFGSetup( "PFTest", 128, 64 );

    struct sockaddr_in si_other;
    int s, i, slen=sizeof(si_other);

    s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    memset((char *) &si_other, 0, sizeof(si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(PORT);
    
    if (inet_aton(argv[1] , &si_other.sin_addr) == 0) 
    {
        fprintf(stderr, "inet_aton() failed\n");
        exit(1);
    }

    if( connect( s, (struct sockaddr *) &si_other, slen ) < 0 )
    {
        fprintf(stderr, "connect() failed\n");
        exit(1);
    }

    void    CNFGClearTransparencyLevel();
    CNFGClearTransparencyLevel();

    uint8_t last_key_state;
    while(1)
    {
        int x, y;
        short w,h;
        XWindowAttributes xwa;
        Window Child;
        XTranslateCoordinates( CNFGDisplay, CNFGWindow, RootWindow(CNFGDisplay, DefaultScreen (CNFGDisplay)), 0, 0, &x, &y, &Child );
        XGetWindowAttributes( CNFGDisplay, CNFGWindow, &xwa );
        w = xwa.width;
        h = xwa.height;

        XImage *image;
        image = XGetImage (CNFGDisplay, RootWindow (CNFGDisplay, DefaultScreen (CNFGDisplay)), x, y, w, h, AllPlanes, /*XYPixmap*/ZPixmap);
        //printf( "%d\n",image->data[0] );
        int stride = image->bytes_per_line;
        int bpp = image->bits_per_pixel;
        uint8_t * px = image->data;
        //printf( "STRIDE: %d %d %d\n", stride, w, h );

        uint8_t sendbuffer[7+1024];
        memset( sendbuffer, 0, sizeof( sendbuffer ) );
        sprintf( sendbuffer, "BUFFER " );
        uint8_t * sb = sendbuffer + 7;

        for( y = 0; y < 64; y++ )
        {
            for( x = 0; x < 128; x++ )
            {
                int ipix = x * w / 128;
                int ipiy = y * h / 64;
                int pxval;
                if( bpp == 1 )
                {
                    uint8_t pxvals = px[ipiy*stride+ipix/8];
                    pxval = (pxvals>>(ipix%8))&1;
                }
                else
                {
                    uint8_t * pxvals = &((uint8_t*)px)[ipiy*stride+ipix*4];
                    int maxv = pxvals[0];
                    if( pxvals[1] > maxv ) maxv = pxvals[1];
                    if( pxvals[2] > maxv ) maxv = pxvals[2];
                    if( pxvals[3] > maxv ) maxv = pxvals[3];
                    pxval = maxv > 160;
                }
                sb[x*8+(y/8)] |= pxval<<(y&7);
            }
        }
        //sb[0] = 0;
        XDestroyImage( image );
        //XFree (image);

        int rr = send( s, sendbuffer, sizeof(sendbuffer), MSG_NOSIGNAL );


        char buffer[1024];
        int rb = recv( s, buffer, 1023, MSG_DONTWAIT );
        if( rb > 0 )
        {
            buffer[rb] = 0;
            int st = atoi( buffer+4 );
            int i;
            const KeySym keyCodes[5] = { XK_Left, XK_Down, XK_Right, XK_Up, XK_space };
            for( i = 0; i < 5; i++ )
            {
                if( (last_key_state & (1<<i)) && !(st & (1<<i) ))
                {
                    XTestFakeKeyEvent( CNFGDisplay, XKeysymToKeycode(CNFGDisplay,keyCodes[i]), False, 0);
                }
                if( !(last_key_state & (1<<i)) && (st & (1<<i) ))
                {
                    XTestFakeKeyEvent( CNFGDisplay, XKeysymToKeycode(CNFGDisplay,keyCodes[i]), True, 0);
                }
            }
            last_key_state = st;
            //write( 0, buffer, rb );
        }

        //XQueryColor (d, DefaultColormap(d, DefaultScreen (d)), c);
        //cout << c.red << " " << c.green << " " << c.blue << "\n";


        CNFGHandleInput();
        CNFGSwapBuffers();
        usleep(20000);
    }
}

void HandleKey( int keycode, int bDown ) {}
void HandleButton( int x, int y, int button, int bDown ) {}
void HandleMotion( int x, int y, int mask ) {}
void HandleDestroy() {}


