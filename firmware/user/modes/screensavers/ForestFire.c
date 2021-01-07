/*
 * ForestFire.c
 *
 *  Created on: January 5, 2020
 *      Author: AEFeinstein
 */

/*==============================================================================
 * Includes
 *============================================================================*/

#include <osapi.h>
#include <mem.h>
#include "oled.h"
#include "ForestFire.h"

/*==============================================================================
 * Structs and Enums
 *============================================================================*/

typedef enum __attribute__((packed))
{
    TREE,
    FIRE,
    EMPTY
}
forestFireCell_t;

typedef struct
{
    forestFireCell_t forest[OLED_WIDTH][OLED_HEIGHT];
    forestFireCell_t nextForest[OLED_WIDTH][OLED_HEIGHT];
} forestFireSim;

/*==============================================================================
 * Variables
 *============================================================================*/

forestFireSim* fFire;

/*==============================================================================
 * Functions
 *============================================================================*/

/**
 * @brief TODO
 *
 */
void ICACHE_FLASH_ATTR initForestFire(void)
{
    fFire = os_malloc(sizeof(forestFireSim));
    for(int16_t x = 0; x < OLED_WIDTH; x++)
    {
        for(int16_t y = 0; y < OLED_HEIGHT; y++)
        {
            if(os_random() % 2 == 0)
            {
                fFire->forest[x][y] = TREE;
            }
            else
            {
                fFire->forest[x][y] = EMPTY;
            }
        }
    }
}

/**
 * @brief TODO
 *
 */
void ICACHE_FLASH_ATTR destroyForestFire(void)
{
    os_free(fFire);
}

/**
 * @brief TODO
 *
 */
void ICACHE_FLASH_ATTR updateForestFire(void)
{
    for(int16_t x = 0; x < OLED_WIDTH; x++)
    {
        for(int16_t y = 0; y < OLED_HEIGHT; y++)
        {
            switch(fFire->forest[x][y])
            {
                case FIRE:
                {
                    // Fires get put out
                    fFire->nextForest[x][y] = EMPTY;
                    drawPixel(x, y, BLACK);
                    break;
                }
                case TREE:
                {
                    // If adjacent cell is fire
                    if(     (x - 1 >= 0          && FIRE == fFire->forest[x - 1][y]) ||
                            (x + 1 < OLED_WIDTH  && FIRE == fFire->forest[x + 1][y]) ||
                            (y - 1 >= 0          && FIRE == fFire->forest[x][y - 1]) ||
                            (y + 1 < OLED_HEIGHT && FIRE == fFire->forest[x][y + 1]))
                    {
                        fFire->nextForest[x][y] = FIRE;
                    }
                    else if(os_random() % 131072 <= 2)
                    {
                        // Trees randomly ignite from 'lightning'
                        fFire->nextForest[x][y] = FIRE;
                    }

                    // Either a tree or burning, pixel is on
                    drawPixel(x, y, WHITE);
                    break;
                }
                default:
                case EMPTY:
                {
                    // Trees randomly grow from nothing
                    if(os_random() % 131072 <= 655)
                    {
                        fFire->nextForest[x][y] = TREE;
                        drawPixel(x, y, WHITE);
                    }
                    else
                    {
                        // Still empty
                        drawPixel(x, y, BLACK);
                    }
                    break;
                }
            }
        }
    }
    ets_memcpy(fFire->forest, fFire->nextForest, sizeof(fFire->forest));
}
