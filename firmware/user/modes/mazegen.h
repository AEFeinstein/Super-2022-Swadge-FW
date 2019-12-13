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

typedef struct
{
    int16_t indwall;
    int16_t indSolution;
} get_maze_output_t;

get_maze_output_t ICACHE_FLASH_ATTR get_maze(uint8_t width, uint8_t height, uint8_t xleft[], uint8_t xright[],
        uint8_t ybot[], uint8_t ytop[], uint8_t xsol[], uint8_t ysol[], float scxcexits[], float scycexits[],
        uint8_t mazescalex, uint8_t mazescaley);

#endif /* _MAZEGEN_H_ */
