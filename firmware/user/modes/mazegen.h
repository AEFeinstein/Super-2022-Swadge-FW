#ifndef _MAZEGEN_H_
#define _MAZEGEN_H_

/*============================================================================
 * Defines
 *==========================================================================*/
#define MAXNUMWALLS 350

//#define MAZE_DEBUG_PRINT
#ifdef MAZE_DEBUG_PRINT
    #include <stdlib.h>
    #define maze_printf(...) maze_printf(__VA_ARGS__)
#else
    #define maze_printf(...)
#endif

/*============================================================================
 * Function Prototypes
 *==========================================================================*/

typedef signed short int16_t;
typedef struct
{
    int16_t indwall;
    int16_t indSolution;
} get_maze_output_t;

#endif /* _MAZEGEN_H_ */
