/*
 * MatrixRain.c
 *
 *  Created on: January 6, 2020
 *      Author: AEFeinstein
 */

/*==============================================================================
 * Includes
 *============================================================================*/

#include <osapi.h>
#include <user_interface.h>
#include <mem.h>

#include "oled.h"
#include "cndraw.h"
#include "font.h"
#include "Screensaver.h"
#include "MatrixRain.h"

/*==============================================================================
 * Defines
 *============================================================================*/

#define NUM_RAINDROPS 12
#define TAIL_SIZE     5

/*==============================================================================
 * Structs
 *============================================================================*/

typedef struct
{
    int16_t x;
    int16_t y;

    char ch;
    uint8_t chSwapCtr;
    const sprite_t* font;
    uint8_t fontHeight;

    char tail[TAIL_SIZE];
    uint8_t tailMoveCtr;
    int16_t tailY;

    uint32_t usPerPixel;
    uint32_t tmr;
} raindrop;

typedef struct
{
    raindrop rain[NUM_RAINDROPS];
} matrixRain;

/*==============================================================================
 * Prototypes
 *============================================================================*/

void ICACHE_FLASH_ATTR initMatrixRain(void);
void ICACHE_FLASH_ATTR updateMatrixRain(void);
void ICACHE_FLASH_ATTR destroyMatrixRain(void);

/*==============================================================================
 * Variables
 *============================================================================*/

screensaver ssMatrixRain =
{
    .initScreensaver = initMatrixRain,
    .updateScreensaver = updateMatrixRain,
    .destroyScreensaver = destroyMatrixRain,
};

matrixRain* mr;

/*==============================================================================
 * Prototypes
 *============================================================================*/

void ICACHE_FLASH_ATTR initRaindrop(raindrop* obj);
char ICACHE_FLASH_ATTR randChr(void);

/*==============================================================================
 * Functions
 *============================================================================*/

/**
 * @brief Initialize Matrix rain
 */
void ICACHE_FLASH_ATTR initMatrixRain(void)
{
    mr = os_malloc(sizeof(matrixRain));

    for(uint8_t i = 0; i < NUM_RAINDROPS; i++)
    {
        initRaindrop(&mr->rain[i]);
    }
}

/**
 * @brief Free the MatrixRain
 */
void ICACHE_FLASH_ATTR destroyMatrixRain(void)
{
    os_free(mr);
}

/**
 * @return A random, printable character (not whitespace, not lowercase)
 */
char ICACHE_FLASH_ATTR randChr(void)
{
    // There are four more chars after lowercase in ASCII
    char ch = '!' + (os_random() % ('e' - '!'));
    if(ch >= 'a')
    {
        ch += ('{' - 'a');
    }
    return ch;
}

/**
 * Initialize a raindrop off the OLED somewhere
 *
 * @param the raindrop to initialize
 */
void ICACHE_FLASH_ATTR initRaindrop(raindrop* obj)
{
    // Random font
    switch(os_random() % 3)
    {
        case 0:
        {
            obj->font = font_TomThumb;
            obj->fontHeight = FONT_HEIGHT_TOMTHUMB;
            break;
        }
        default:
        case 1:
        {
            obj->font = font_IbmVga8;
            obj->fontHeight = FONT_HEIGHT_IBMVGA8;
            break;
        }
        case 2:
        {
            obj->font = font_Radiostars;
            obj->fontHeight = FONT_HEIGHT_RADIOSTARS;
            break;
        }
    }

    // Random placement above the screen
    obj->x = os_random() % (OLED_WIDTH);
    obj->y = -(os_random() % (OLED_HEIGHT));

    // Random character
    obj->ch = randChr();
    obj->chSwapCtr = 0;

    // Random tail
    for(uint8_t i = 0; i < TAIL_SIZE; i++)
    {
        obj->tail[i] = randChr();
    }
    obj->tailMoveCtr = 0;
    obj->tailY = obj->y - (obj->fontHeight / 2);

    // Random speed
    obj->usPerPixel = 25000 + (os_random() % 50000);
    obj->tmr = 0;
}

/**
 * @brief Update and display the rain
 */
void ICACHE_FLASH_ATTR updateMatrixRain(void)
{
    static uint32_t tLast = 0;
    bool shouldDraw = false;
    if(0 == tLast)
    {
        tLast = system_get_time();
        // Initial draw
        shouldDraw = true;
    }
    else
    {
        // Track time
        uint32_t tNow = system_get_time();
        uint32_t tElapsed = (tNow - tLast);
        tLast = tNow;

        // Move some rain
        for(uint8_t i = 0; i < NUM_RAINDROPS; i++)
        {
            raindrop* obj = &(mr->rain[i]);

            obj->tmr += tElapsed;
            if(obj->tmr > obj->usPerPixel)
            {
                obj->tmr -= obj->usPerPixel;

                // Move main char smoothly
                obj->y++;

                // Randomize the first char every few pixels
                obj->chSwapCtr = (obj->chSwapCtr + 1) % (obj->fontHeight - 1);
                if(0 == obj->chSwapCtr)
                {
                    obj->ch = randChr();
                }

                // Move tail jumpily
                obj->tailMoveCtr = (obj->tailMoveCtr + 1) % ((obj->fontHeight + 1) / 2);
                if(0 == obj->tailMoveCtr)
                {
                    obj->tailY += ((obj->fontHeight + 1) / 2);
                }

                // Something changed, so draw it
                shouldDraw = true;
            }
        }
    }

    // If there's something to draw
    if(shouldDraw)
    {
        // Clear first
        clearDisplay();

        // For each raindrop
        for(uint8_t i = 0; i < NUM_RAINDROPS; i++)
        {
            raindrop* obj = &(mr->rain[i]);
            // If this raindrop starts in-bounds
            if(obj->y > -obj->fontHeight)
            {
                bool allOffscreen = true;

                // Align everything by finding the largest char width first
                uint8_t maxCharWidth = charWidth(obj->ch, obj->font);
                for(uint8_t j = 0; j < TAIL_SIZE; j++)
                {
                    if(charWidth(obj->tail[j], obj->font) > maxCharWidth)
                    {
                        maxCharWidth = charWidth(obj->tail[j], obj->font);
                    }
                }

                // Plot the tail
                for(uint8_t j = 0; j < TAIL_SIZE; j++)
                {
                    // Check if it's on the screen
                    int16_t charY = obj->tailY - (j * (obj->fontHeight + 1));
                    if(charY < OLED_HEIGHT && charY > -obj->fontHeight)
                    {
                        // Note something is on-screen
                        allOffscreen = false;

                        // Plot the char
                        int16_t width = charWidth(obj->tail[j], obj->font);
                        int16_t charX = obj->x + (maxCharWidth - width) / 2;
                        int16_t charY = obj->tailY - (j * (obj->fontHeight + 1));
                        plotChar(charX,
                                 charY,
                                 obj->tail[j],
                                 obj->font,
                                 WHITE_F_TRANSPARENT_B);

                        // Shade the character to make it feel like its fading
                        shadeDisplayArea(charX, charY, charX + width, charY + obj->fontHeight, j);
                    }
                }

                // Plot the main char if it's on screen
                if(obj->y < OLED_HEIGHT)
                {
                    // Note something is on-screen
                    allOffscreen = false;

                    // Plot the char
                    int16_t xOffset = (maxCharWidth - charWidth(obj->ch, obj->font)) / 2;
                    plotChar(obj->x + xOffset,
                             obj->y,
                             obj->ch,
                             obj->font,
                             WHITE_F_TRANSPARENT_B);
                }

                // If this is entirely off-screen, initialize it to fall again
                if(true == allOffscreen)
                {
                    initRaindrop(obj);
                }
            }
        }
    }
}
