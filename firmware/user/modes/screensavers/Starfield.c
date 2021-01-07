/*
 * Starfield.c
 *
 *  Created on: January 5, 2020
 *      Author: AEFeinstein, nathansmith11170
 * 
 * Adapted from
 * https://github.com/nathansmith11170/StarField/blob/master/StarField/StarField.c
 */

#include <osapi.h>
#include <user_interface.h>
#include <limits.h>

#include "oled.h"
#include "bresenham.h"
#include "Starfield.h"

/**
 * The x and the y are the position of the ellipse, and the z value represents
 * the proximity of the star, with 1 being far away and 0 being very close.
 */
typedef struct
{
    int x;
    int y;
    int z;
} Star;

void ICACHE_FLASH_ATTR updateStar(Star* star);
void ICACHE_FLASH_ATTR translate(int (*coord)[], int translateByX, int translateByY);
int ICACHE_FLASH_ATTR randomInt(int lowerBound, int upperBound);

#define NUM_STARS 92
Star stars[NUM_STARS];

/**
 * @brief Generates a random integer in a range
 *
 * @param lowerBound
 * @param upperBound
 * @return int
 */
int ICACHE_FLASH_ATTR randomInt(int lowerBound, int upperBound)
{
    return os_random() % (upperBound - lowerBound + 1) + lowerBound;
}

/**
 * @brief Moves a star by a certain x and y
 *
 * @param coord
 * @param translateByX
 * @param translateByY
 */
void ICACHE_FLASH_ATTR translate(int (*coord)[], int translateByX, int translateByY)
{
    (*coord)[0] = (*coord)[0] + translateByX;
    (*coord)[1] = (*coord)[1] + translateByY;
}

/**
 * Shifts a star's proximity by decrementing its z value
 *
 * @param star
 */
void ICACHE_FLASH_ATTR updateStar(Star* star)
{
    star->z -= 20;
    if(star->z <= 0)
    {
        star->x = randomInt(-OLED_WIDTH / 2, OLED_WIDTH / 2);
        star->y = randomInt(-OLED_HEIGHT / 2, OLED_HEIGHT / 2);
        star->z += 1024;
    }
}

/**
 * @brief Initialize Starfield
 */
void ICACHE_FLASH_ATTR initStarField(void)
{
    /* Initialize the stars */
    for(int i = 0; i < NUM_STARS; i++)
    {
        stars[i].x = randomInt(-OLED_WIDTH / 2, OLED_WIDTH / 2);
        stars[i].y = randomInt(-OLED_HEIGHT / 2, OLED_HEIGHT / 2);
        stars[i].z = 1 + (os_random() % 1023);
    }
}

/**
 * @brief Update and display Starfield
 */
void ICACHE_FLASH_ATTR starField(void)
{
    clearDisplay();

    /* rendering */
    for(int i = 0; i < NUM_STARS; i++)
    {
        /* Move and size the star */
        int temp[2];
        updateStar(&stars[i]);
        temp[0] = (1024 * stars[i].x) / stars[i].z;
        temp[1] = (1024 * stars[i].y) / stars[i].z;
        translate(&temp, OLED_WIDTH / 2, OLED_HEIGHT / 2);

        /* Draw the star */
        if( stars[i].z < 205)
        {
            plotRect(temp[0] - 3, temp[1] - 1, temp[0] + 3, temp[1] + 1, WHITE);
            plotRect(temp[0] - 1, temp[1] - 3, temp[0] + 1, temp[1] + 3, WHITE);
            plotRect(temp[0] - 2, temp[1] - 2, temp[0] + 2, temp[1] + 2, WHITE);
            drawPixel(temp[0], temp[1], WHITE);
        }
        else if (stars[i].z < 410)
        {
            plotRect(temp[0] - 2, temp[1] - 1, temp[0] + 2, temp[1] + 1, WHITE);
            plotRect(temp[0] - 1, temp[1] - 2, temp[0] + 1, temp[1] + 2, WHITE);
            drawPixel(temp[0], temp[1], WHITE);
        }
        else if (stars[i].z < 614)
        {
            plotRect(temp[0] - 1, temp[1], temp[0] + 2, temp[1] + 1, WHITE);
            plotRect(temp[0], temp[1] - 1, temp[0] + 1, temp[1] + 2, WHITE);
        }
        else if (stars[i].z < 819)
        {
            plotRect(temp[0], temp[1], temp[0] + 1, temp[1] + 1, WHITE);
        }
        else
        {
            drawPixel(temp[0], temp[1], WHITE);
        }
    }
}
