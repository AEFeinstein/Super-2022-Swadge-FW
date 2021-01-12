/*
 * Starfield.c
 *
 *  Created on: January 6, 2020
 *      Author: AEFeinstein, nathansmith11170
 *
 * Images from
 * https://learn.adafruit.com/animated-flying-toaster-oled-jewelry/code
 */

/*==============================================================================
 * Includes
 *============================================================================*/

#include <osapi.h>
#include <user_interface.h>
#include <mem.h>

#include "oled.h"
#include "assets.h"
#include "Screensaver.h"
#include "Toaster.h"

/*==============================================================================
 * Defines
 *============================================================================*/

#define NUM_OBJECTS 8

/*==============================================================================
 * Structs
 *============================================================================*/

typedef struct
{
    int16_t x;
    int16_t y;
    bool isToaster;
    uint16_t velocity;
    uint8_t frame;
    uint32_t tAccumUs;
    uint32_t frameMs;
} flyingObj;

typedef struct
{
    pngSequenceHandle toaster;
    pngHandle toast;
    flyingObj objects[NUM_OBJECTS];
} toasterDat;

/*==============================================================================
 * Prototypes
 *============================================================================*/

void ICACHE_FLASH_ATTR initObject(flyingObj* obj, bool veryOffScreen);

void ICACHE_FLASH_ATTR flyToasters(void);
void ICACHE_FLASH_ATTR destroyToaster(void);
void ICACHE_FLASH_ATTR initToaster(void);

/*==============================================================================
 * Variables
 *============================================================================*/

toasterDat* toaster;

screensaver ssToaster =
{
    .initScreensaver = initToaster,
    .updateScreensaver = flyToasters,
    .destroyScreensaver = destroyToaster,
};

/*==============================================================================
 * Functions
 *============================================================================*/

/**
 * @brief Initialize Toaster
 */
void ICACHE_FLASH_ATTR initToaster(void)
{
    toaster = os_malloc(sizeof(toasterDat));
    allocPngSequence(&(toaster->toaster), 4,
                     "toaster0.png",
                     "toaster1.png",
                     "toaster2.png",
                     "toaster1.png");
    allocPngAsset("toast.png", &(toaster->toast));

    for(uint8_t i = 0; i < NUM_OBJECTS; i++)
    {
        initObject(&toaster->objects[i], true);
    }
}

/**
 * @brief Free the Toaster
 */
void ICACHE_FLASH_ATTR destroyToaster(void)
{
    freePngSequence(&(toaster->toaster));
    freePngAsset(&(toaster->toast));
    os_free(toaster);
}

/**
 * @brief TODO
 *
 * @param obj
 */
void ICACHE_FLASH_ATTR initObject(flyingObj* obj, bool veryOffScreen)
{
    if(os_random() % 3 == 0)
    {
        obj->x = OLED_WIDTH;
        obj->y = (os_random() % OLED_HEIGHT) - toaster->toaster.handles->height;
        if(veryOffScreen)
        {
            obj->x += (os_random() % OLED_WIDTH);
        }
    }
    else
    {
        obj->x = (os_random() % OLED_WIDTH);
        obj->y = -toaster->toaster.handles->height;
        if(veryOffScreen)
        {
            obj->y -= (os_random() % OLED_HEIGHT) - toaster->toaster.handles->height;
        }
    }

    if(os_random() % 5 == 0)
    {
        obj->isToaster = false;
    }
    else
    {
        obj->isToaster = true;
    }
    obj->tAccumUs = 0;
    obj->frame = 0;
    obj->frameMs = 10000 + (os_random() % 100000);
}

/**
 * @brief Update and display the toasters
 */
void ICACHE_FLASH_ATTR flyToasters(void)
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
        uint32_t tNow = system_get_time();
        uint32_t tElapsed = (tNow - tLast);
        tLast = tNow;

        // Move some toasters
        for(uint8_t i = 0; i < NUM_OBJECTS; i++)
        {
            flyingObj* obj = &(toaster->objects[i]);

            // If this toaster's timer dinged
            obj->tAccumUs += tElapsed;
            if(obj->tAccumUs > obj->frameMs)
            {
                obj->tAccumUs -= obj->frameMs;

                // Move the object
                obj->x -= 2;
                obj->y++;

                // Every few frames, flap some wings
                if(obj->y % 3 == 0)
                {
                    obj->frame = (obj->frame + 1) % toaster->toaster.count;
                }

                // If it's off screen
                if(obj->y > OLED_HEIGHT || obj->x < -toaster->toaster.handles->width)
                {
                    // Respawn
                    initObject(obj, false);
                }

                // Something changed, so draw
                shouldDraw = true;
            }
        }
    }

    if(shouldDraw)
    {
        clearDisplay();

        for(uint8_t i = 0; i < NUM_OBJECTS; i++)
        {
            flyingObj* obj = &(toaster->objects[i]);
            if(obj->isToaster)
            {
                // Draw a toaster
                drawPngSequence(&(toaster->toaster), obj->x, obj->y, false, false, 0, obj->frame);
            }
            else
            {
                // Draw a toast
                drawPng(&(toaster->toast), obj->x, obj->y, false, false, 0);
            }
        }
    }
}
