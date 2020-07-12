#include <mem.h>
#include "menu.h"
#include "oled.h"
#include "font.h"
#include "bresenham.h"

#define ROW_SPACING 3
#define ITEM_SPACING 9

void ICACHE_FLASH_ATTR linkNewNode(cLinkedNode_t** root, uint8_t len, linkedInfo_t info);

void ICACHE_FLASH_ATTR initMenu(menu_t** menu, char* title, menuCb cbFunc)
{
    *menu = (menu_t*)os_malloc(sizeof(menu_t));
    (*menu)->title = title;
    (*menu)->rows = NULL;
    (*menu)->numRows = 0;
    (*menu)->cbFunc = cbFunc;
    (*menu)->yOffset = 0;
}

void ICACHE_FLASH_ATTR deinitMenu(menu_t* menu)
{
    while(menu->numRows--)
    {
        while(menu->rows->d.row.numItems--)
        {
            cLinkedNode_t* next = menu->rows->d.row.items->next;
            os_free(menu->rows->d.row.items);
            menu->rows->d.row.items = next;
        }
        cLinkedNode_t* next = menu->rows->next;
        os_free(menu->rows);
        menu->rows = next;
    }
    os_free(menu);
}

void ICACHE_FLASH_ATTR linkNewNode(cLinkedNode_t** root, uint8_t len, linkedInfo_t info)
{
    if(NULL == (*root))
    {
        // If the root is null, simply os_malloc a new node
        (*root) = (cLinkedNode_t*)os_malloc(sizeof(cLinkedNode_t));
        (*root)->next = (*root);
        (*root)->prev = (*root);
        (*root)->d = info;
    }
    else
    {
        // Iterate to the end of the rows
        cLinkedNode_t* row = (*root);
        while(--len)
        {
            row = row->next;
        }

        // Save what will be the next and prev rows
        cLinkedNode_t* newNext = row->next;
        cLinkedNode_t* newPrev = row;

        // Allocate the new row
        row->next = (cLinkedNode_t*)os_malloc(sizeof(cLinkedNode_t));
        row = row->next;
        row->d = info;

        // Link everything together
        row->prev = newPrev;
        row->next = newNext;
        newPrev->next = row;
        newNext->prev = row;
    }
}

void ICACHE_FLASH_ATTR addRowToMenu(menu_t* menu)
{
    // Make a new row
    linkedInfo_t newRow;
    newRow.row.items = NULL;
    newRow.row.numItems = 0;
    newRow.row.xOffset = 0;

    // Link the new row
    linkNewNode(&(menu->rows), menu->numRows, newRow);
    menu->numRows++;
}

void ICACHE_FLASH_ATTR addItemToRow(menu_t* menu, char* name, int id)
{
    // Iterate to the end of the rows
    cLinkedNode_t* row = menu->rows;
    uint8_t len = menu->numRows;
    while(--len)
    {
        row = row->next;
    }

    // Make a new item
    linkedInfo_t newItem;
    newItem.item.id = id;
    newItem.item.name = name;

    // Link the new item
    linkNewNode(&(row->d.row.items), row->d.row.numItems, newItem);
    row->d.row.numItems++;
}

void ICACHE_FLASH_ATTR drawMenu(menu_t* menu)
{
    clearDisplay();
    bool drawnBox = false;

    // Start drawning just past the middle of the screen
    int16_t yPos = (OLED_HEIGHT / 2) + 1;

    cLinkedNode_t* row = menu->rows->prev;
    for(uint8_t r = 0; r < menu->numRows; r++)
    {
        cLinkedNode_t* items = row->d.row.items;
        if(row->d.row.numItems > 1)
        {
            int16_t xPos = 2 + row->d.row.xOffset;
            while(xPos < OLED_WIDTH)
            {
                int16_t oldX = xPos;
                xPos = plotText(
                           xPos,
                           menu->yOffset + yPos,
                           items->d.item.name,
                           IBM_VGA_8, WHITE);

                if(r == 1 && !drawnBox && menu->yOffset == 0 && row->d.row.xOffset == 0)
                {
                    plotRect(oldX - 2, yPos - 2, xPos, yPos + FONT_HEIGHT_IBMVGA8 + 1, WHITE);
                    drawnBox = true;
                }

                xPos += ITEM_SPACING;
                items = items->next;
            }

            if(row->d.row.xOffset < 0)
            {
                row->d.row.xOffset++;
            }
            if(row->d.row.xOffset > 0)
            {
                row->d.row.xOffset--;
            }
        }
        else
        {
            // If there's only one item, just plot it
            int16_t xPos = plotText(
                               2,
                               menu->yOffset + yPos,
                               items->d.item.name,
                               IBM_VGA_8, WHITE);

            if(r == 1 && !drawnBox && menu->yOffset == 0)
            {
                plotRect(0, yPos - 2, xPos, yPos + FONT_HEIGHT_IBMVGA8 + 1, WHITE);
                drawnBox = true;
            }
        }

        // Move to the next row
        row = row->next;
        yPos += FONT_HEIGHT_IBMVGA8 + ROW_SPACING;
    }

    if(menu->yOffset < 0)
    {
        menu->yOffset++;
    }
    if(menu->yOffset > 0)
    {
        menu->yOffset--;
    }
    fillDisplayArea(0, 0, OLED_WIDTH, 37, BLACK);
}

void ICACHE_FLASH_ATTR menuButton(menu_t* menu, int btn)
{
    if((menu->yOffset != 0) || (menu->rows->d.row.xOffset != 0))
    {
        return;
    }

    switch(btn)
    {
        case 0:
        {
            menu->rows = menu->rows->prev;
            menu->yOffset = -(FONT_HEIGHT_IBMVGA8 + ROW_SPACING);
            break;
        }
        case 1:
        {
            menu->rows = menu->rows->next;
            menu->yOffset = (FONT_HEIGHT_IBMVGA8 + ROW_SPACING);
            break;
        }
        case 2:
        {
            if(menu->rows->d.row.numItems > 0)
            {
                menu->rows->d.row.items = menu->rows->d.row.items->prev;
                uint8_t wordWidth = textWidth(menu->rows->d.row.items->d.item.name, IBM_VGA_8);
                menu->rows->d.row.xOffset = -(wordWidth + ITEM_SPACING + 2);
            }
            break;
        }
        case 3:
        {
            if(menu->rows->d.row.numItems > 0)
            {
                menu->rows->d.row.items = menu->rows->d.row.items->next;
                uint8_t wordWidth = textWidth(menu->rows->d.row.items->d.item.name, IBM_VGA_8);
                menu->rows->d.row.xOffset = wordWidth + ITEM_SPACING + 2;
            }
            break;
        }
        case 4:
        {
            if(NULL != menu->cbFunc)
            {
                menu->cbFunc(menu->rows->d.row.items->d.item.id);
            }
            break;
        }
    }
}
