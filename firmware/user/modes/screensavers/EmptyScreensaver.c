/*
 * EmptyScreensaver.c
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
#include "Screensaver.h"
#include "EmptyScreensaver.h"

/*==============================================================================
 * Prototypes
 *============================================================================*/

void ICACHE_FLASH_ATTR initEmptyScreensaver(void);
void ICACHE_FLASH_ATTR updateEmptyScreensaver(void);
void ICACHE_FLASH_ATTR destroyEmptyScreensaver(void);

/*==============================================================================
 * Variables
 *============================================================================*/

screensaver ssEmptyScreensaver =
{
    .initScreensaver = initEmptyScreensaver,
    .updateScreensaver = updateEmptyScreensaver,
    .destroyScreensaver = destroyEmptyScreensaver,
};

/*==============================================================================
 * Functions
 *============================================================================*/

/**
 * @brief TODO
 *
 */
void ICACHE_FLASH_ATTR initEmptyScreensaver(void)
{
    clearDisplay();
}

/**
 * @brief TODO
 *
 */
void ICACHE_FLASH_ATTR destroyEmptyScreensaver(void)
{
    // Nothing to destroy
}

/**
 * @brief TODO
 *
 */
void ICACHE_FLASH_ATTR updateEmptyScreensaver(void)
{
    // Nothing to update
}
