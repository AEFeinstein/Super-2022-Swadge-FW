/*
 * menu2d.h
 *
 *  Created on: Jul 12, 2020
 *      Author: adam
 */

#ifndef _MENU_2D_H_
#define _MENU_2D_H_

/*==============================================================================
 * Includes
 *============================================================================*/

#include <osapi.h>
#include "user_config.h"

#if defined(FEATURE_OLED)

/*==============================================================================
 * Structs, Unions, and Typedefs
 *============================================================================*/

typedef void (*menuCb)(const char*);

struct cLinkedNode;

typedef struct
{
    struct cLinkedNode* items;
    uint8_t numItems;
    int8_t xOffset;
    uint32_t tAccumulatedUs;
} rowInfo_t;

typedef struct
{
    const char* name;
} itemInfo_t;

typedef union
{
    rowInfo_t row;
    itemInfo_t item;
} linkedInfo_t;

typedef struct cLinkedNode
{
    linkedInfo_t d;
    struct cLinkedNode* prev;
    struct cLinkedNode* next;
} cLinkedNode_t;

typedef struct
{
    const char* title;
    cLinkedNode_t* rows;
    uint8_t numRows;
    menuCb cbFunc;
    int8_t yOffset;
    uint32_t tLastCallUs;
    uint32_t tAccumulatedUs;
} menu_t;

/*==============================================================================
 * Prototypes
 *============================================================================*/

menu_t* initMenu(const char* title, menuCb cbFunc);
void deinitMenu(menu_t* menu);
void addRowToMenu(menu_t* menu);
linkedInfo_t* addItemToRow(menu_t* menu, const char* name);
void removeItemFromMenu(menu_t* menu, const char* name);
void drawMenu(menu_t* menu);
void menuButton(menu_t* menu, int btn);

#endif
#endif