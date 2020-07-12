#ifndef _MENU_H_
#define _MENU_H_

#include <osapi.h>

struct cLinkedNode;

typedef struct
{
    struct cLinkedNode* items;
    uint8_t numItems;
    int8_t xOffset;
} rowInfo_t;

typedef struct
{
    char* name;
    uint8_t id;
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

typedef void (*menuCb)(int);

typedef struct
{
    char* title;
    cLinkedNode_t* rows;
    uint8_t numRows;
    menuCb cbFunc;
    int8_t yOffset;
} menu_t;

void initMenu(menu_t** menu, char* title, menuCb cbFunc);
void deinitMenu(menu_t* menu);
void addRowToMenu(menu_t* menu);
void addItemToRow(menu_t* menu, char* name, int id);
void drawMenu(menu_t* menu);
void menuButton(menu_t* menu, int btn);

#endif