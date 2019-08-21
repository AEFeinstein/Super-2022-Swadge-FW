// https://en.wikipedia.org/wiki/Maze_generation_algorithm
// example of depth-first search maze generator
// Code by Jacek Wieczorek
// modified by bbkiwi


#include <stdlib.h>
#include <time.h>
#include "mazegen.h"


//#define UBUNTU
#ifdef UBUNTU
#define ICACHE_FLASH_ATTR 
#include <stdio.h>
#include <stdbool.h>
typedef unsigned char       uint8_t;
#define os_printf printf
#define os_random rand
#else
#include <osapi.h>
#include "user_main.h"
#endif

#define FAIL 1
#define DEBUG 1

typedef struct
{
	uint8_t x, y; //Node position - little waste of memory, but it allows faster generation
	void *parent; //Pointer to parent node
	char c; //Character to be displayed
	char dirs; //Directions that still haven't been explored
} Node;

/*============================================================================
 * Prototypes
 *==========================================================================*/

//uint8_t ICACHE_FLASH_ATTR init(uint8_t width, uint8_t height, Node * nodes );
uint8_t ICACHE_FLASH_ATTR init(uint8_t width, uint8_t height, Node ** nodes );
uint8_t ICACHE_FLASH_ATTR deinit(Node * nodes );
int16_t ICACHE_FLASH_ATTR wallIntervals_helper(bool usetranspose, uint8_t outerlooplimit, uint8_t innerlooplimit, uint8_t *out1, uint8_t * out2, uint8_t * in1, uint8_t * in2, uint8_t width, uint8_t height, Node * nodes, int16_t indwall);
int16_t ICACHE_FLASH_ATTR wallIntervals(uint8_t width, uint8_t height, Node * nodes, uint8_t *ybot, uint8_t * ytop, uint8_t * xleft, uint8_t * xright);
Node ICACHE_FLASH_ATTR *link(uint8_t width, uint8_t height, Node * nodes,  Node * n );
void ICACHE_FLASH_ATTR getwalls(uint8_t width, uint8_t height, Node * nodes );
int16_t ICACHE_FLASH_ATTR getwallsintervals(uint8_t width, uint8_t height, Node * nodes, uint8_t xleft[], uint8_t xright[],uint8_t ybot[],uint8_t ytop[]);
int16_t ICACHE_FLASH_ATTR get_maze(uint8_t width, uint8_t height, uint8_t xleft[], uint8_t xright[], uint8_t ybot[], uint8_t ytop[]);
#if DEBUG
void ICACHE_FLASH_ATTR draw(uint8_t width, uint8_t height, Node * nodes);
#endif
/*============================================================================
 * Functions
 *==========================================================================*/



uint8_t ICACHE_FLASH_ATTR init(uint8_t width, uint8_t height, Node ** nodes )
{
	uint8_t i, j;
	Node *n;
	//Allocate memory for maze
	*nodes = calloc( width * height, sizeof( Node ) );
	if ( *nodes == NULL ) return FAIL;

	//Setup crucial nodes
	for ( i = 0; i < width; i++ )
	{
		for ( j = 0; j < height; j++ )
		{
			n = *nodes + i + j * width;
			if ( i * j % 2 )
			{
				n->x = i;
				n->y = j;
				n->dirs = 15; //Assume that all directions can be explored (4 youngest bits set)
				n->c = ' ';
			}
			else n->c = '#'; //Add walls between nodes
		}
	}
	return 0;
}

uint8_t ICACHE_FLASH_ATTR deinit(Node * nodes )
{
	free(nodes);
	return 0;
}

#if DEBUG
void ICACHE_FLASH_ATTR draw(uint8_t width, uint8_t height, Node * nodes)
{
	uint8_t i, j;

	//Outputs maze to terminal - nothing special
	for ( i = 0; i < height; i++ )
	{
		for ( j = 0; j < width; j++ )
		{
			os_printf( "%c", nodes[j + i * width].c );
		}
		os_printf( "\n" );
	}
}
#endif

int16_t ICACHE_FLASH_ATTR wallIntervals_helper(bool usetranspose, uint8_t outerlooplimit, uint8_t innerlooplimit, uint8_t *out1, uint8_t * out2, uint8_t * in1, uint8_t * in2, uint8_t width, uint8_t height, Node * nodes, int16_t indwall)
{
	uint8_t i, j, jbegin;
	bool intervalstarted;
	(void)height;
	for ( i = 0; i < outerlooplimit; i++ )
	{
		jbegin = 0;
		intervalstarted = true;
		for ( j = 1; j < innerlooplimit + 1; j++ )
		{
#if DEBUG > 1
			printf("%c i=%d, intervalstarted=%d, j=%d, jbegin=%d, indwall=%d\n",usetranspose ? nodes[i+ j * width].c : nodes[j+ i * width].c,
					 i, intervalstarted, j,jbegin, indwall);
#endif
			if ((j==innerlooplimit) || (usetranspose ? nodes[i+ j * width].c : nodes[j+ i * width].c)  == ' ')
			{
				if (intervalstarted)
				{
					if (j - jbegin > 2)
					{
						if (indwall < MAXNUMWALLS)
						{
						in1[indwall] = jbegin;
						in2[indwall] = j-1;
						out2[indwall] = out1[indwall] = i;
						indwall++;
						} else {
							os_printf("indwall exceeds max of %d\n", MAXNUMWALLS);
							return 0; //give up
						}
					}
					intervalstarted = false;
				}
			} else {
				if (!intervalstarted)
				{
					jbegin = j;
					intervalstarted = true;
				}
			}
		}
	}
	return indwall;
}

int16_t ICACHE_FLASH_ATTR wallIntervals(uint8_t width, uint8_t height, Node * nodes, uint8_t *ybot, uint8_t * ytop, uint8_t * xleft, uint8_t * xright)
{
	int16_t indwall = 0;
	indwall = wallIntervals_helper(false, height, width, ybot, ytop, xleft, xright, width, height, nodes, indwall);
	if (indwall > 0) // did not give up so continue
	{
		indwall = wallIntervals_helper(true, width, height, xleft, xright, ybot, ytop, width, height, nodes, indwall);
	}
	return indwall; //if zero means failed

}



Node ICACHE_FLASH_ATTR *link(uint8_t width, uint8_t height, Node * nodes,  Node * n )
{
	//Connects node to random neighbor (if possible) and returns
	//address of next node that should be visited

	uint8_t x = 0;
	uint8_t y = 0;
	char dir;
	Node *dest;

	//Nothing can be done if null pointer is given - return
	if ( n == NULL ) return NULL;

	//While there are directions still unexplored
	while ( n->dirs )
	{
		//Randomly pick one direction
		dir = ( 1 << ( os_random( ) % 4 ) );
		//If it has already been explored - try again
		if ( ~n->dirs & dir ) continue;

		//Mark direction as explored
		n->dirs &= ~dir;

		//Depending on chosen direction
		switch ( dir )
		{
			//Check if it's possible to go right
			case 1:
				if ( n->x + 2 < width )
				{
					x = n->x + 2;
					y = n->y;
				}
				else continue;
				break;

			//Check if it's possible to go down
			case 2:
				if ( n->y + 2 < height )
				{
					x = n->x;
					y = n->y + 2;
				}
				else continue;
				break;

			//Check if it's possible to go left
			case 4:
				if ( n->x - 2 >= 0 )
				{
					x = n->x - 2;
					y = n->y;
				}
				else continue;
				break;

			//Check if it's possible to go up
			case 8:
				if ( n->y - 2 >= 0 )
				{
					x = n->x;
					y = n->y - 2;
				}
				else continue;
				break;
			default:
				(void)0;
		}

		//Get destination node into pointer (makes things a tiny bit faster)
		dest = nodes + x + y * width;

		//Make sure that destination node is not a wall
		if ( dest->c == ' ' )
		{
			//If destination is a linked node already - abort
			if ( dest->parent != NULL ) continue;

			//Otherwise, adopt node
			dest->parent = n;

			//Remove wall between nodes
			nodes[n->x + ( x - n->x ) / 2 + ( n->y + ( y - n->y ) / 2 ) * width].c = ' ';

			//Return address of the child node
			return dest;
		}
	}

	//If nothing more can be done here - return parent's address
	return n->parent;
}

void ICACHE_FLASH_ATTR getwalls(uint8_t width, uint8_t height, Node * nodes )
{
	Node *start, *last;
	//Initialize maze
	//Setup start node
	start = nodes + 1 + width;
	start->parent = start;
	last = start;
	//Connect nodes until start node is reached and can't be left
	while ( ( last = link(width, height, nodes, last ) ) != start );
}

int16_t ICACHE_FLASH_ATTR getwallsintervals(uint8_t width, uint8_t height, Node * nodes, uint8_t xleft[], uint8_t xright[], uint8_t ybot[], uint8_t ytop[])
{
	int16_t indwall;
	// Need to check but seems walls always have odd number, so min wall length is 3
#if DEBUG
	int16_t numwallsbound = 4 + (height/2 - 1) * (width/4) + (width/2) * (height/4); //should be upper bound
	os_printf("Bound on number of walls = %d\n", numwallsbound);
#endif
	indwall = wallIntervals(width, height, nodes, ybot, ytop, xleft, xright);
	return indwall;
}

int16_t ICACHE_FLASH_ATTR get_maze(uint8_t width, uint8_t height, uint8_t xleft[], uint8_t xright[], uint8_t ybot[], uint8_t ytop[])
{
// Input width, height
// Output arrays xleft, xright, ybot and ytop of dim MAXNUMWALLS
//               will contain coordinates of endpoints of walls
//  returns number of walls N so i=0; i< N will index them all.
//  Note if returns N means failed due to not being able to allocate
//        working memory or exceeding MAXNUMWALLS

	Node * nodes; // used to make maze and then dealocated
	//Seed random generator
	//srand( time( NULL ) );


	//Allocate memory and set up nodes
	if ( init(width, height, &nodes ) )
	{
		os_printf( "out of memory trying to init!\n");
		return 0;
	}


	// Make a random maze
	getwalls(width, height, nodes);

	//Get intervals making up walls
	// can be used to draw it on OLED and cannot be crossed
	int16_t indwall = getwallsintervals(width, height, nodes, xleft, xright, ybot, ytop);

#if DEBUG
	// Show Maze as printed characters
	draw(width, height, nodes );


	//Print wall intervals
	os_printf("indwall = %d\n", indwall);
	for (int16_t i=0; i<indwall; i++)
	{
		os_printf( "(%d, %d) to (%d, %d)\n", xleft[i], ybot[i], xright[i], ytop[i] );
	}
	os_printf("\n");
#endif
	//Deallocate memory
	deinit(nodes);

	return indwall;
}

#ifdef UBUNTU
//TODO this prob not working
void ICACHE_FLASH_ATTR main( uint8_t argc, char **argv )
{
	uint8_t width, height; //Maze dimensions probably for OLED use 31 15
	int16_t indwall;
	uint8_t xleft[MAXNUMWALLS];
	uint8_t xright[MAXNUMWALLS];
	uint8_t ytop[MAXNUMWALLS];
	uint8_t ybot[MAXNUMWALLS];


	//Check argument count
	if ( argc < 3 )
	{
		os_printf("%s: please specify maze dimensions!\n", argv[0] );
		exit( FAIL );
	}

	//Read maze dimensions from command line arguments
	if ( sscanf( argv[1], "%d", &width ) + sscanf( argv[2], "%d", &height ) < 2 )
	{
		os_printf("%s: invalid maze size value!\n", argv[0] );
		exit( FAIL );
	}

	//Allow only odd dimensions
	if ( !( width % 2 ) || !( height % 2 ) )
	{
		os_printf("%s: dimensions must be odd!\n", argv[0] );
		exit( FAIL );
	}

	//Do not allow negative dimensions
	if ( width <= 0 || height <= 0 )
	{
		os_printf("%s: dimensions must be greater than 0!\n", argv[0] );
		exit( FAIL );
	}

	indwall = get_maze(width, height, xleft, xright, ybot, ytop);
}
#endif
