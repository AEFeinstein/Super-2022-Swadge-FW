/*
 * ForestFire.c
 *
 *  Created on: January 5, 2020
 *      Author: AEFeinstein
 */

#include <osapi.h>
#include "oled.h"

typedef enum __attribute__((packed))
{
    TREE,
    FIRE,
    EMPTY
}
forestFireCell_t;

forestFireCell_t forest[OLED_WIDTH][OLED_HEIGHT];
forestFireCell_t nextForest[OLED_WIDTH][OLED_HEIGHT];

/**
 * @brief TODO
 *
 */
void ICACHE_FLASH_ATTR initForestFire(void)
{
    for(int16_t x = 0; x < OLED_WIDTH; x++)
    {
        for(int16_t y = 0; y < OLED_HEIGHT; y++)
        {
            if(os_random() % 2 == 0)
            {
                forest[x][y] = TREE;
            }
            else
            {
                forest[x][y] = EMPTY;
            }
        }
    }
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
            switch(forest[x][y])
            {
                case FIRE:
                {
                    // Fires get put out
                    nextForest[x][y] = EMPTY;
                    drawPixel(x, y, BLACK);
                    break;
                }
                case TREE:
                {
                    // If adjacent cell is fire
                    if(     (x - 1 >= 0          && FIRE == forest[x - 1][y]) ||
                            (x + 1 < OLED_WIDTH  && FIRE == forest[x + 1][y]) ||
                            (y - 1 >= 0          && FIRE == forest[x][y - 1]) ||
                            (y + 1 < OLED_HEIGHT && FIRE == forest[x][y + 1]))
                    {
                        nextForest[x][y] = FIRE;
                    }
                    else if(os_random() % 131072 <= 2)
                    {
                        // Trees randomly ignite from 'lightning'
                        nextForest[x][y] = FIRE;
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
                        nextForest[x][y] = TREE;
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
    ets_memcpy(forest, nextForest, sizeof(forest));
}
