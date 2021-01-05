/**
 * MatrixFireFast - A fire simulation for NeoPixel (and other?) matrix
 * displays on Arduino (or ESP8266) using FastLED.
 *
 * Author: Patrick Rigney (https://www.toggledbits.com/)
 * Copyright 2020 Patrick H. Rigney, All Rights Reserved.
 *
 * Github: https://github.com/toggledbits/MatrixFireFast
 * License information can be found at the above Github link.
 *
 * Please donate in support of my projects: https://www.toggledbits.com/donate
 *
 * For configuration information and processor selection, please see
 * the README file at the above Github link.
 */

#include <osapi.h>
#include <user_interface.h>
#include "MatrixFastFire.h"
#include "oled.h"

// #define VERSION 20275

/* MATRIX CONFIGURATION -- PLEASE SEE THE README (GITHUB LINK ABOVE) */
#define MAT_W 64 /* Size (columns) of entire matrix */
#define MAT_H 32 /* and rows */

/* Flare constants */
#define FLARE_ROWS    4 /* number of rows (from bottom) allowed to flare */
#define MAX_FLARE     8 /* max number of simultaneous flares */
#define FLARE_CHANCE 50 /* chance (%) of a new flare (if there's room) */
#define FLARE_DECAY  14 /* decay rate of flare radiation; 14 is good */
#define NCOLORS      24

/* Flare variables */
uint8_t pix[MAT_W][MAT_H];
uint8_t nflare = 0;
uint32_t flare[MAX_FLARE];
unsigned long t = 0;

/**
 * @brief TODO
 *
 * @param col
 * @param row
 * @return uint16_t
 */
uint16_t ICACHE_FLASH_ATTR pos( uint16_t col, uint16_t row )
{
    return row + (col * MAT_W);
}

/**
 * @brief TODO
 *
 * @param n
 * @return uint32_t
 */
uint32_t ICACHE_FLASH_ATTR isqrt(uint32_t n)
{
    if ( n < 2 )
    {
        return n;
    }
    uint32_t smallCandidate = isqrt(n >> 2) << 1;
    uint32_t largeCandidate = smallCandidate + 1;
    return (largeCandidate * largeCandidate > n) ? smallCandidate : largeCandidate;
}

/**
 * @brief Set pixels to intensity around flare
 *
 * @param x
 * @param y
 * @param z
 */
void ICACHE_FLASH_ATTR glow( int x, int y, int z )
{
    int b = z * 10 / FLARE_DECAY + 1;
    for ( int i = (y - b); i < (y + b); ++i )
    {
        for ( int j = (x - b); j < (x + b); ++j )
        {
            if ( i >= 0 && j >= 0 && i < MAT_H && j < MAT_W )
            {
                int d = ( FLARE_DECAY * isqrt((x - j) * (x - j) + (y - i) * (y - i)) + 5 ) / 10;
                uint8_t n = 0;
                if ( z > d )
                {
                    n = z - d;
                }
                if ( n > pix[j][i] )   // can only get brighter
                {
                    pix[j][i] = n;
                }
            }
        }
    }
}

/**
 * @brief TODO
 *
 */
void ICACHE_FLASH_ATTR newflare(void)
{
    if ( nflare < MAX_FLARE && (1 + (os_random() % 100)) <= FLARE_CHANCE )
    {
        int x = os_random() % MAT_W;
        int y = (MAT_H - 1) - (os_random() % FLARE_ROWS);
        int z = NCOLORS / 2;
        flare[nflare++] = (z << 16) | (y << 8) | (x & 0xff);
        glow( x, y, z );
    }
}

/** make_fire() animates the fire display. It should be called from the
 *  loop periodically (at least as often as is required to maintain the
 *  configured refresh rate). Better to call it too often than not enough.
 *  It will not refresh faster than the configured rate. But if you don't
 *  call it frequently enough, the refresh rate may be lower than
 *  configured.
 */
void ICACHE_FLASH_ATTR make_fire(void)
{
    // First, move all existing heat points up the display and fade
    for(int16_t y = 0; y < MAT_H - 1; y++)
    {
        for(int16_t x = 0; x < MAT_W; x++)
        {
            uint8_t n = 0;
            if(pix[x][y + 1] > 0)
            {
                n = pix[x][y + 1] - 1;
            }
            pix[x][y] = n;
        }
    }

    // Heat the bottom row
    for(int16_t x = 0; x < MAT_W; x++)
    {
        if(pix[x][MAT_H - 1] > 0)
        {
            pix[x][MAT_H - 1] = (NCOLORS / 2) + os_random() % (NCOLORS / 2);
        }
    }

    // flare
    for ( uint16_t i = 0; i < nflare; ++i )
    {
        int x = flare[i] & 0xff;
        int y = (flare[i] >> 8) & 0xff;
        int z = (flare[i] >> 16) & 0xff;
        glow( x, y, z );
        if ( z > 1 )
        {
            flare[i] = (flare[i] & 0xffff) | ((z - 1) << 16);
        }
        else
        {
            // This flare is out
            for ( uint16_t j = i + 1; j < nflare; ++j )
            {
                flare[j - 1] = flare[j];
            }
            --nflare;
        }
    }
    newflare();

    // Set and draw
    for(int16_t x = 0; x < OLED_WIDTH; x++)
    {
        for(int16_t y = 0; y < OLED_HEIGHT; y++)
        {
            if(pix[x / 2][y / 2] > 0/*NCOLORS / 4*/)
            {
                drawPixel(x, y, WHITE);
            }
            else
            {
                drawPixel(x, y, BLACK);
            }
        }
    }
}

/**
 * @brief TODO
 *
 */
void ICACHE_FLASH_ATTR mff_setup(void)
{
    for ( uint16_t i = 0; i < MAT_W; i++ )
    {
        for ( uint16_t j = 0; j < MAT_H; j++ )
        {
            if ( j == MAT_H - 1 )
            {
                pix[i][j] = NCOLORS - 1;
            }
            else
            {
                pix[i][j] = 0;
            }
        }
    }
}
