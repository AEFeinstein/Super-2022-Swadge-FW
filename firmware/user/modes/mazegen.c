// https://en.wikipedia.org/wiki/Maze_generation_algorithm
// example of depth-first search maze generator
// Code by Jacek Wieczorek
// modified by bbkiwi

#include <osapi.h>
#include <mem.h>
#include "mazegen.h"


//#define UBUNTU
#ifdef UBUNTU
    #define ICACHE_FLASH_ATTR
    #include <stdio.h>
    //#include <stdbool.h>
    typedef unsigned char bool;
    typedef unsigned char uint8_t;
    typedef unsigned short uint16_t;
    typedef signed short int16_t;
    #define os_printf printf
    #define os_random rand
    #define true 1
    #define false 0
#else
    #include <osapi.h>
    #include "user_main.h"
    #include <user_interface.h>
#endif

#define FAIL 1
#define DEBUG 0

typedef struct
{
    //uint8_t x, y; //Node position - little waste of memory, but it allows faster generation
    // eliminated and changed code to compute on fly as was short of memory
    void* parent; //Pointer to parent node
    char c; //Character to be displayed
    char dirs; //Directions that still haven't been explored
} Node;


/*============================================================================
 * Prototypes
 *==========================================================================*/

//uint8_t ICACHE_FLASH_ATTR init(uint8_t width, uint8_t height, Node * nodes );
uint8_t ICACHE_FLASH_ATTR init(uint8_t width, uint8_t height, Node** nodes );
uint8_t ICACHE_FLASH_ATTR deinit(Node* nodes );
int16_t ICACHE_FLASH_ATTR wallIntervals_helper(bool usetranspose, uint8_t outerlooplimit, uint8_t innerlooplimit,
        uint8_t* out1, uint8_t* out2, uint8_t* in1, uint8_t* in2, uint8_t width, uint8_t height, Node* nodes, int16_t indwall);
int16_t ICACHE_FLASH_ATTR wallIntervals(uint8_t width, uint8_t height, Node* nodes, uint8_t* ybot, uint8_t* ytop,
                                        uint8_t* xleft, uint8_t* xright);
Node ICACHE_FLASH_ATTR* link(uint8_t width, uint8_t height, Node* nodes,  Node* n );
Node ICACHE_FLASH_ATTR* relink(uint8_t width, uint8_t __attribute__((unused))height, Node* nodes,  Node* n );
void ICACHE_FLASH_ATTR makeMazeNodes(uint8_t width, uint8_t height, Node* nodes );
void ICACHE_FLASH_ATTR rerootMazeNodes(uint8_t width, uint8_t height, uint8_t xroot, uint8_t yroot, Node* nodes );
uint16_t ICACHE_FLASH_ATTR getPathToRoot(uint8_t xsol[], uint8_t ysol[],  uint8_t width, uint8_t height, uint8_t xpoint,
        uint8_t ypoint, Node* nodes, uint8_t mazescalex, uint8_t mazescaley );
int16_t ICACHE_FLASH_ATTR getwallsintervals(uint8_t width, uint8_t height, Node* nodes, uint8_t xleft[],
        uint8_t xright[], uint8_t ybot[], uint8_t ytop[]);
void ICACHE_FLASH_ATTR removeParents(uint8_t width, uint8_t height, Node* nodes);
#if DEBUG
    void ICACHE_FLASH_ATTR draw(uint8_t width, uint8_t height, Node* nodes);
#endif
/*============================================================================
 * Functions
 *==========================================================================*/



uint8_t ICACHE_FLASH_ATTR init(uint8_t width, uint8_t height, Node** nodes )
{
    uint8_t i, j;
    Node* n;
    //Allocate memory for maze
    maze_printf("width = %d, height = %d, sizeofNode = %d\n", width, height, (uint8_t)sizeof(Node));
    *nodes = os_calloc( width * height, sizeof( Node ) );
    if ( *nodes == NULL )
    {
        return FAIL;
    }

    //Setup crucial nodes
    for ( i = 0; i < width; i++ )
    {
        for ( j = 0; j < height; j++ )
        {
            n = *nodes + i + j * width;
            if ( i * j % 2 )
            {
                //n->x = i;
                //n->y = j;
                n->dirs = 15; //Assume that all directions can be explored (4 youngest bits set)
                n->c = ' ';
            }
            else
            {
                n->c = '#';    //Add walls between nodes
            }
        }
    }
    return 0;
}

uint8_t ICACHE_FLASH_ATTR deinit(Node* nodes )
{
    os_free(nodes);
    return 0;
}

void ICACHE_FLASH_ATTR removeParents(uint8_t width, uint8_t height, Node* nodes)
{
    uint8_t i, j;
    // for i or j even not part of tree
    for ( i = 1; i < height - 1; i += 2 )
    {
        for ( j = 1; j < width - 1; j += 2 )
        {
            nodes[j + i * width].parent = NULL;
        }
    }
}

#if DEBUG
void ICACHE_FLASH_ATTR draw(uint8_t width, uint8_t height, Node* nodes)
{
    uint8_t i, j;

    //Outputs maze to terminal - nothing special
    for ( i = 0; i < height; i++ )
    {
        for ( j = 0; j < width; j++ )
        {
            maze_printf( "%c", nodes[j + i * width].c );
        }
        maze_printf( "\n" );
    }
#ifdef UBUNTU
#if DEBUG > 1
    Node* n;
    Node* n11;
    // for i or j even not part of tree
    n11 = nodes + 1 + width;
    for ( i = 1; i < height - 1; i += 2 )
    {
        for ( j = 1; j < width - 1; j += 2 )
        {
            n = nodes + j + i * width;
            //maze_printf("(%d, %d) %c %x %x \n",j, i, n->c, n - nodes, (int)(n->parent) - (int)nodes);
            maze_printf("(%d, %d) %c %x %d %d \n", j, i, n->c, (0xff & n->dirs), n - n11, (Node*)(n->parent) - n11);
        }
        maze_printf( "\n" );
    }
#endif
#endif
}
#endif

int16_t ICACHE_FLASH_ATTR wallIntervals_helper(bool usetranspose, uint8_t outerlooplimit, uint8_t innerlooplimit,
        uint8_t* out1, uint8_t* out2, uint8_t* in1, uint8_t* in2, uint8_t width, uint8_t height, Node* nodes, int16_t indwall)
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
#if DEBUG > 2
            maze_printf("%c i=%d, intervalstarted=%d, j=%d, jbegin=%d, indwall=%d\n",
                        usetranspose ? nodes[i + j * width].c : nodes[j + i * width].c,
                        i, intervalstarted, j, jbegin, indwall);
#endif
            if ((j == innerlooplimit) || (usetranspose ? nodes[i + j * width].c : nodes[j + i * width].c)  == ' ')
            {
                if (intervalstarted)
                {
                    if (j - jbegin > 2)
                    {
                        if (indwall < MAXNUMWALLS)
                        {
                            in1[indwall] = jbegin;
                            in2[indwall] = j - 1;
                            out2[indwall] = out1[indwall] = i;
                            indwall++;
                        }
                        else
                        {
                            maze_printf("indwall exceeds max of %d\n", MAXNUMWALLS);
                            return 0; //give up
                        }
                    }
                    intervalstarted = false;
                }
            }
            else
            {
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

int16_t ICACHE_FLASH_ATTR wallIntervals(uint8_t width, uint8_t height, Node* nodes, uint8_t* ybot, uint8_t* ytop,
                                        uint8_t* xleft, uint8_t* xright)
{
    int16_t indwall = 0;
    indwall = wallIntervals_helper(false, height, width, ybot, ytop, xleft, xright, width, height, nodes, indwall);
    if (indwall > 0) // did not give up so continue
    {
        indwall = wallIntervals_helper(true, width, height, xleft, xright, ybot, ytop, width, height, nodes, indwall);
    }
    return indwall; //if zero means failed

}



Node ICACHE_FLASH_ATTR* link(uint8_t width, uint8_t height, Node* nodes,  Node* n )
{
    //Connects node to random neighbor (if possible) and returns
    //address of next node that should be visited

    //Original code used n->x and n->y but if not set up in struct
    //they can be replace by  (n - nodes) % width and (n-nodes)/width resp.

    uint8_t x = 0;
    uint8_t y = 0;
    char dir;
    char dirofparent;
    Node* dest;

#if DEBUG > 2
    Node* n11;
    n11 = nodes + 1 + width;
    maze_printf("%d ", n - n11);
#endif

    //Nothing can be done if null pointer is given - return
    if ( n == NULL )
    {
        return NULL;
    }

    //While there are directions still unexplored
    while ( 0xf & n->dirs )
    {
        //Randomly pick one direction
        dir = ( 1 << ( os_random( ) % 4 ) );
        //If it has already been explored - try again
        if ( ~n->dirs & dir )
        {
            continue;
        }

        //Mark direction as explored
        n->dirs &= (~dir | 0xf0);

        //Depending on chosen direction
        switch ( dir )
        {
            //Check if it's possible to go right
            case 1:
                if ( ((n - nodes) % width) + 2 < width )
                {
                    x = ((n - nodes) % width) + 2;
                    y = ((n - nodes) / width);
                    dirofparent = 0x40;
                }
                else
                {
                    continue;
                }
                break;

            //Check if it's possible to go down
            case 2:
                if ( ((n - nodes) / width) + 2 < height )
                {
                    x = ((n - nodes) % width);
                    y = ((n - nodes) / width) + 2;
                    dirofparent = 0x80;
                }
                else
                {
                    continue;
                }
                break;

            //Check if it's possible to go left
            case 4:
                if ( ((n - nodes) % width) - 2 >= 0 )
                {
                    x = ((n - nodes) % width) - 2;
                    y = ((n - nodes) / width);
                    dirofparent = 0x10;
                }
                else
                {
                    continue;
                }
                break;

            //Check if it's possible to go up
            case 8:
                if ( ((n - nodes) / width) - 2 >= 0 )
                {
                    x = ((n - nodes) % width);
                    y = ((n - nodes) / width) - 2;
                    dirofparent = 0x20;
                }
                else
                {
                    continue;
                }
                break;
            default:
                dirofparent = 0x00;
        }

        //Get destination node into pointer (makes things a tiny bit faster)
        dest = nodes + x + y * width;

        //Make sure that destination node is not a wall
        if ( dest->c == ' ' )
        {
            //If destination is a linked node already - abort
            if ( dest->parent != NULL )
            {
                continue;
            }

            //Otherwise, adopt node
            dest->parent = n;

            // Save this direction in top bits
            n->dirs |= (dir << 4);

            //Add to dest->dirs in top bits direction of its parent
            dest->dirs |= dirofparent;

            //Remove wall between nodes
            //nodes[((n - nodes) % width) + ( x - ((n - nodes) % width) ) / 2 + ( ((n-nodes)/width) + ( y - ((n-nodes)/width) ) / 2 ) * width].c = ' ';
            nodes[( x + ((n - nodes) % width) ) / 2 + ( ( y + ((n - nodes) / width) ) / 2 ) * width].c = ' ';

            //Return address of the child node
            return dest;
        }
    }
    //If nothing more can be done here - return parent's address
    return n->parent;
}

Node ICACHE_FLASH_ATTR* relink(uint8_t width, uint8_t __attribute__((unused))height, Node* nodes,  Node* n )
{
    //Connects node to same neighbors and returns
    //address of next node that should be visited
    // used to start at another node

    uint8_t x = 0;
    uint8_t y = 0;
    char dir;
    Node* dest;

#if DEBUG > 2
    Node* n11;
    n11 = nodes + 1 + width;
    maze_printf("%d ", n - n11);
#endif
    //Nothing can be done if null pointer is given - return
    if ( n == NULL )
    {
        return NULL;
    }

    //Use the stored directions in top 4 bits

    for (uint8_t i = 4; i < 8; i++)
    {
        dir = ( 1 << i );
        //If it has already been explored - try again
        if ( ~n->dirs & dir )
        {
            continue;
        }

        //Depending on chosen direction
        switch ( 0xf0 & dir )
        {
            //go right
            case 0x10:
                x = ((n - nodes) % width) + 2;
                y = ((n - nodes) / width);
                break;
            //go down
            case 0x20:
                x = ((n - nodes) % width);
                y = ((n - nodes) / width) + 2;
                break;
            //go left
            case 0x40:
                x = ((n - nodes) % width) - 2;
                y = ((n - nodes) / width);
                break;
            //go up
            case 0x80:
                x = ((n - nodes) % width);
                y = ((n - nodes) / width) - 2;
                break;
            default:
                (void)0;
        }

        //Get destination node into pointer (makes things a tiny bit faster)
        dest = nodes + x + y * width;
#if DEBUG > 2
        maze_printf(" Went %x Try dest %d ", 0xf0 & dir,  dest - n11);
#endif
        //If destination is a linked node already - abort
        if ( dest->parent != NULL )
        {
#if DEBUG > 2
            maze_printf(" all ready linked ");
#endif
            continue;
        }
#if DEBUG > 2
        maze_printf(" Adopted \n");
#endif

        //Otherwise, adopt node
        dest->parent = n;

        //Return address of the child node
        return dest;
    }
#if DEBUG > 2
    maze_printf(" Returning parent %d \n", (Node*)(n->parent) - n11);
#endif
    //If nothing more can be done here - return parent's address
    return n->parent;
}

void ICACHE_FLASH_ATTR makeMazeNodes(uint8_t width, uint8_t height, Node* nodes )
{
    Node* root, *last;
    //Initialize maze
    //Setup root node
    root = nodes + 1 + width; // upper left
    //root = nodes + width -2 + (height -2) * width; // lower right
    root->parent = root;
    last = root;
    //Connect nodes until root node is reached and can't be left
    while ( ( last = link(width, height, nodes, last ) ) != root );
    //maze_printf("\n");
}

void ICACHE_FLASH_ATTR rerootMazeNodes(uint8_t width, uint8_t height, uint8_t xroot, uint8_t yroot, Node* nodes )
{
    //xroot odd from 1 to width-2, yroot odd from 1 to height-2
    Node* root, *last;
    removeParents(width, height, nodes);
    //Setup root node at new root
    root = nodes + xroot + yroot * width;
    root->parent = root;
    last = root;
    //reconnect nodes until root node is reached
    // Do 4 x as arbitrary starting point could have up to 4 direction to go
    while ( ( last = relink(width, height, nodes, last ) ) != root );
    while ( ( last = relink(width, height, nodes, last ) ) != root );
    while ( ( last = relink(width, height, nodes, last ) ) != root );
    while ( ( last = relink(width, height, nodes, last ) ) != root );
    //maze_printf("\n");
}

// Find shortest path from an arbitrary point to the root
//    returns scaled coordinates
uint16_t ICACHE_FLASH_ATTR getPathToRoot(uint8_t xsol[], uint8_t ysol[],  uint8_t width,
        uint8_t __attribute__((unused))height, uint8_t xpoint, uint8_t ypoint, Node* nodes,  uint8_t mazescalex,
        uint8_t mazescaley )
{
    //xpoint odd from 1 to width-2, ypoint odd from 1 to height-2
    Node* point;

    //Node * n11;
    //n11 = nodes + 1 + width;

    point = nodes + xpoint + ypoint * width;
    uint8_t x;
    uint8_t y;
    uint16_t i = 0;

    while (true)
    {
        x = ((point - nodes) % width);
        y = ((point - nodes) / width);
        xsol[i] = x * mazescalex;
        ysol[i] = y * mazescaley;
        i++;
        //maze_printf("%d (%d, %d)", point - n11, x, y);
        //maze_printf("(%d, %d)", x, y);
        if (point == (Node*)point->parent)
        {
            break;
        }
        point = (Node*)point->parent;
        //maze_printf(" -> ");
    }
    //maze_printf("\n");
    return i;
}

int16_t ICACHE_FLASH_ATTR getwallsintervals(uint8_t width, uint8_t height, Node* nodes, uint8_t xleft[],
        uint8_t xright[], uint8_t ybot[], uint8_t ytop[])
{
    int16_t indwall;
    // Need to check but seems walls always have odd number, so min wall length is 3
#if DEBUG
    int16_t numwallsbound = 4 + (height / 2 - 1) * (width / 4) + (width / 2) * (height / 4); //should be upper bound
    maze_printf("Bound on number of walls = %d\n", numwallsbound);
#endif
    indwall = wallIntervals(width, height, nodes, ybot, ytop, xleft, xright);
    return indwall;
}
get_maze_output_t ICACHE_FLASH_ATTR get_maze(uint8_t width, uint8_t height, uint8_t xleft[], uint8_t xright[],
        uint8_t ybot[], uint8_t ytop[], uint8_t xsol[], uint8_t ysol[], float scxcexits[], float scycexits[],
        uint8_t mazescalex, uint8_t mazescaley)
{
    // Input width, height, scxcexits[], scycexits[], mazescalex, mazescaley);

    // Output arrays xleft, xright, ybot and ytop of dim MAXNUMWALLS
    //                     will contain coordinates of endpoints of walls
    //  returns out where out.indwall = number of walls
    //        so i=0; i< out.indwall will index them all.
    //  and     out.indSolution indexes xsol and ysol
    //  Note if returns out.indwall = 0 means failed due to not being able to allocate
    //        working memory or exceeding MAXNUMWALLS

    Node* nodes;  // used to make maze and then dealocated
    //Seed random generator
#ifdef UBUNTU
    srand( time( NULL ) );
#endif

    //Allocate memory and set up nodes
    if ( init(width, height, &nodes ) )
    {
        maze_printf( "out of memory trying to init!\n");
        get_maze_output_t zout = {0, 0};
        return zout;
    }


    // Make a random maze rooted in upper left corner
    makeMazeNodes(width, height, nodes);

    //Get intervals making up walls
    // can be used to draw it on OLED and cannot be crossed
    int16_t indwall = getwallsintervals(width, height, nodes, xleft, xright, ybot, ytop);



    // Find solutions to maze
    uint16_t indSolution = 0;
    uint8_t xroot = 1;
    uint8_t yroot = 1;
    // center point
    uint8_t xpoint = (width - 1) / 2;
    uint8_t ypoint = (height - 1) / 2;
    maze_printf("Number of walls = %d\n", indwall);
    for (uint8_t i = 0; i < 4; i++)
    {
#if DEBUG
        // Show Maze as printed characters
        if (i == 0)
        {
            draw(width, height, nodes );
        }
#endif
        // Make complete solution starting at center and going to each exit in turn
        // Get partial solution path to ith exit
        indSolution +=  getPathToRoot(&xsol[indSolution], &ysol[indSolution], width, height, xpoint, ypoint, nodes, mazescalex,
                                      mazescaley);
        // Insert ith exit
        xsol[indSolution] = scxcexits[i];
        ysol[indSolution] = scycexits[i];
        indSolution++;
        xpoint = xroot;
        ypoint = yroot;
        switch (i)
        {
            case 0: //lower left next
                xroot = 1;
                yroot = height - 2;
                rerootMazeNodes(width, height, xroot, yroot, nodes );
                break;
            case 1: //lower right next
                xroot = width - 2;
                rerootMazeNodes(width, height, xroot, yroot, nodes );
                break;
            case 2: //upper right next
                yroot = 1;
                rerootMazeNodes(width, height, xroot, yroot, nodes );
                break;
            default:
                break;
        }
        //maze_printf("%d at %d\n", indSolution, i);
    }
#if DEBUG > 1
    // Print solution start center, then go to four corners
    maze_printf("indSolution %d numcells %d ratio %d / 100\n", indSolution, width * height,
                100 * width * height / indSolution);
    for (__int16_t i = 0; i < indSolution; i++)
    {
        maze_printf("(%d, %d) -> ", xsol[i], ysol[i]);
    }
    maze_printf("\n");
#endif
#if DEBUG > 1
    //Print wall intervals
    for (int16_t i = 0; i < indwall; i++)
    {
        maze_printf( "(%d, %d) to (%d, %d)\n", xleft[i], ybot[i], xright[i], ytop[i] );
    }
    maze_printf("\n");
#endif
    //Deallocate memory
    deinit(nodes);
    get_maze_output_t out;
    out.indwall = indwall;
    out.indSolution = indSolution;
    return out;
}

#ifdef UBUNTU
void ICACHE_FLASH_ATTR main( uint8_t argc, char** argv )
{
    //Maze dimensions probably for OLED use 31 15
    uint8_t width;
    uint8_t height;
    get_maze_output_t out;
    uint8_t xleft[MAXNUMWALLS];
    uint8_t xright[MAXNUMWALLS];
    uint8_t ytop[MAXNUMWALLS];
    uint8_t ybot[MAXNUMWALLS];
    uint8_t xsol[2 * MAXNUMWALLS];
    uint8_t ysol[2 * MAXNUMWALLS];
    float scxcexits[4] = {55, 56, 57, 58};
    float scycexits[4] = {65, 66, 67, 68};

    //Check argument count
    if ( argc < 3 )
    {
        maze_printf("%s: please specify maze dimensions!\n", argv[0] );
        exit( FAIL );
    }

    //Read maze dimensions from command line arguments
    //TODO this is altering memory past &width and &height????
    if ( sscanf( argv[1], "%hhu", &width ) + sscanf( argv[2], "%hhu", &height ) < 2 )
    {
        maze_printf("%s: invalid maze size value!\n", argv[0] );
        exit( FAIL );
    }

    //Allow only odd dimensions
    if ( !( width % 2 ) || !( height % 2 ) )
    {
        maze_printf("%s: dimensions must be odd!\n", argv[0] );
        exit( FAIL );
    }

    //Do not allow negative dimensions
    if ( width <= 0 || height <= 0 )
    {
        maze_printf("%s: dimensions must be greater than 0!\n", argv[0] );
        exit( FAIL );
    }
    uint8_t mazescalex = 1;
    uint8_t mazescaley = 2;

    maze_printf("Scales before calling get_maze in main %d %d\n ", mazescalex, mazescaley);
    out = get_maze(width, height, xleft, xright, ybot, ytop, xsol, ysol, scxcexits, scycexits, mazescalex, mazescaley);
}
#endif
