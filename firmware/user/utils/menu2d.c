/*
 * menu2d.c
 *
 *  Created on: Jul 12, 2020
 *      Author: adam
 */

/*==============================================================================
 * Includes
 *============================================================================*/

#include <mem.h>
#include "menu2d.h"
#include "oled.h"
#include "font.h"
#include "bresenham.h"

#if defined(FEATURE_OLED)

/*==============================================================================
 * Defines
 *============================================================================*/

#define ROW_SPACING   3
#define ITEM_SPACING 10

#define BLANK_SPACE_Y  37
#define SELECTED_ROW_Y 46

/*==============================================================================
 * Prototypes
 *============================================================================*/

void linkNewNode(cLinkedNode_t** root, uint8_t len, linkedInfo_t info);
void drawRow(cLinkedNode_t* row, int16_t yPos, bool shouldDrawBox);

/*==============================================================================
 * Functions
 *============================================================================*/

/**
 * Allocate memory for and initialize a menu struct
 *
 * @param title  The title for this menu. This must be a pointer to static
 *               memory. New memory is NOT allocated for this string
 * @param cbFunc The callback function for when an item is selected
 * @return A pointer to malloc'd memory for this menu
 */
menu_t* ICACHE_FLASH_ATTR initMenu(const char* title, menuCb cbFunc)
{
    // Allocate the memory
    menu_t* menu = (menu_t*)os_malloc(sizeof(menu_t));

    // Initialize all values
    menu->title = title;
    menu->rows = NULL;
    menu->numRows = 0;
    menu->cbFunc = cbFunc;
    menu->yOffset = 0;

    // Return the pointer
    return menu;
}

/**
 * Free all memory allocated for a menu struct, including memory
 * for the rows and items
 *
 * @param menu The menu to to free
 */
void ICACHE_FLASH_ATTR deinitMenu(menu_t* menu)
{
    // For each row
    while(menu->numRows--)
    {
        // For each item in the row
        while(menu->rows->d.row.numItems--)
        {
            // Free the item, iterate to the next item
            cLinkedNode_t* next = menu->rows->d.row.items->next;
            os_free(menu->rows->d.row.items);
            menu->rows->d.row.items = next;
        }
        // Then free the row and iterate to the next
        cLinkedNode_t* next = menu->rows->next;
        os_free(menu->rows);
        menu->rows = next;
    }
    // Finally free the whole menu
    os_free(menu);
}

/**
 * Helper function to link a new node in the circular linked list. This can
 * either be a new row in the menu or a new entry in a row.
 *
 * @param root A pointer to a cLinkedNode_t pointer to link the new node to
 * @param len  The number of items in root
 * @param info The info to link in the list
 */
void ICACHE_FLASH_ATTR linkNewNode(cLinkedNode_t** root, uint8_t len, linkedInfo_t info)
{
    // If root points to a null pointer
    if(NULL == (*root))
    {
        // os_malloc a new node
        (*root) = (cLinkedNode_t*)os_malloc(sizeof(cLinkedNode_t));

        // Link it up
        (*root)->next = (*root);
        (*root)->prev = (*root);

        // And store the information
        (*root)->d = info;
    }
    else
    {
        // If root doesn't point to a null pointer, iterate to the end of the rows
        cLinkedNode_t* row = (*root);
        while(--len)
        {
            row = row->next;
        }

        // Save what will be the next and prev rows
        cLinkedNode_t* newNext = row->next;
        cLinkedNode_t* newPrev = row;

        // Allocate the new row, and move to it
        row->next = (cLinkedNode_t*)os_malloc(sizeof(cLinkedNode_t));
        row = row->next;

        // Link everything together
        row->prev = newPrev;
        row->next = newNext;
        newPrev->next = row;
        newNext->prev = row;

        // And store the information
        row->d = info;
    }
}

/**
 * Add a row to the menu
 *
 * @param menu The menu to add a row to
 */
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

/**
 * Add a new item to the row which was last added to the menu
 *
 * @param menu The menu to add an item to
 * @param name The name of this item. This must be a pointer to static memory.
 *             New memory is NOT allocated for this string. This pointer will
 *             be returned via the callback when the item is selected
 */
void ICACHE_FLASH_ATTR addItemToRow(menu_t* menu, const char* name)
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
    newItem.item.name = name;

    // Link the new item
    linkNewNode(&(row->d.row.items), row->d.row.numItems, newItem);
    row->d.row.numItems++;
}

/**
 * Draw a single row of the menu to the OLED
 *
 * @param row The row to draw
 * @param yPos The Y position of the row to draw
 * @param shouldDrawBox True if this is the selected row, false otherwise
 */
void ICACHE_FLASH_ATTR drawRow(cLinkedNode_t* row, int16_t yPos, bool shouldDrawBox)
{
    // Get a pointer to the items for this row
    cLinkedNode_t* items = row->d.row.items;

    // If there are multiple items
    if(row->d.row.numItems > 1)
    {
        // Get the X position for the selected item, centering it
        int16_t xPos = row->d.row.xOffset + ((OLED_WIDTH - textWidth((char*)items->d.item.name, IBM_VGA_8)) / 2);

        // Then work backwards to make sure the entire row is drawn
        while(xPos > 0)
        {
            // Iterate backwards
            items = items->prev;
            // Adjust the x pos
            xPos -= (textWidth((char*)items->d.item.name, IBM_VGA_8) + ITEM_SPACING);
        }

        // Then draw items until we're off the OLED
        bool drawnBox = false;
        while(xPos < OLED_WIDTH)
        {
            // Plot the text
            int16_t xPosS = xPos;
            xPos = plotText(
                       xPos, yPos,
                       (char*)items->d.item.name,
                       IBM_VGA_8, WHITE);

            // If this is the selected item, draw a box around it
            if(shouldDrawBox && !drawnBox && row->d.row.items == items)
            {
                plotRect(xPosS - 2, yPos - 2, xPos, yPos + FONT_HEIGHT_IBMVGA8 + 1, WHITE);
                drawnBox = true;
            }

            // Add some space between items
            xPos += ITEM_SPACING;

            // Iterate to the next item
            items = items->next;
        }

        // If the offset is nonzero, move it towards zero
        if(0 != row->d.row.xOffset)
        {
            if(row->d.row.xOffset < 1)
            {
                row->d.row.xOffset += 2;
            }
            else if(row->d.row.xOffset > 1)
            {
                row->d.row.xOffset -= 2;
            }
            else
            {
                row->d.row.xOffset = 0;
            }
        }
    }
    else
    {
        // If there's only one item, just plot it
        int16_t xPosS = (OLED_WIDTH - textWidth((char*)items->d.item.name, IBM_VGA_8)) / 2;
        int16_t xPosF = plotText(xPosS, yPos,
                                 (char*)items->d.item.name,
                                 IBM_VGA_8, WHITE);

        // If this is the selected item, draw a box around it
        if(shouldDrawBox)
        {
            plotRect(xPosS -  2, yPos - 2, xPosF, yPos + FONT_HEIGHT_IBMVGA8 + 1, WHITE);
        }
    }
}

/**
 * Draw the menu to the OLED. The menu has animations for smooth scrolling,
 * so it is recommended this function be called every 20ms
 *
 * @param menu The menu to draw
 */
void ICACHE_FLASH_ATTR drawMenu(menu_t* menu)
{
    // First clear the OLED
    if(1 == menu->numRows)
    {
        fillDisplayArea(0, OLED_HEIGHT - FONT_HEIGHT_IBMVGA8 - 4, OLED_WIDTH, OLED_HEIGHT, BLACK);
    }
    else
    {
        clearDisplay();
    }

    // Start with the seleted row to be drawn
    int16_t yPos = menu->yOffset + SELECTED_ROW_Y;
    cLinkedNode_t* row = menu->rows;

    if(1 == menu->numRows)
    {
        yPos = OLED_HEIGHT - FONT_HEIGHT_IBMVGA8 - 2;
    }
    else
    {
        // Work backwards to draw all prior rows on the OLED
        while(yPos + FONT_HEIGHT_IBMVGA8 >= BLANK_SPACE_Y)
        {
            row = row->prev;
            yPos -= (FONT_HEIGHT_IBMVGA8 + ROW_SPACING);
        }
    }

    // Draw rows until you run out of space on the OLED
    while(yPos < OLED_HEIGHT)
    {
        // Draw the row
        drawRow(row, yPos,
                (row == menu->rows) && (menu->yOffset == 0) && (row->d.row.xOffset == 0));

        // Move to the next row
        row = row->next;
        yPos += FONT_HEIGHT_IBMVGA8 + ROW_SPACING;
    }

    if(1 != menu->numRows)
    {
        // If the offset is nonzero, move it towards zero
        if(menu->yOffset < 0)
        {
            menu->yOffset++;
        }
        if(menu->yOffset > 0)
        {
            menu->yOffset--;
        }

        // Clear the top 37 pixels of the OLED
        fillDisplayArea(0, 0, OLED_WIDTH, BLANK_SPACE_Y, BLACK);

        // Draw the title, centered
        int16_t titleOffset = (OLED_WIDTH - textWidth((char*)menu->title, RADIOSTARS)) / 2;
        plotText(titleOffset, 8, (char*)menu->title, RADIOSTARS, WHITE);
    }
}

/**
 * This is called to process button presses. When a button is pressed, the
 * selected row or item immediately changes. An offset is set in the X or Y
 * direction so the menu doesn't appear to move, then the menu is smoothly
 * animated to the final position
 *
 * @param menu The menu to process a button for
 * @param btn  The button that was pressed
 */
void ICACHE_FLASH_ATTR menuButton(menu_t* menu, int btn)
{
    // If the menu is in motion, ignore this button press
    if((menu->yOffset != 0) || (menu->rows->d.row.xOffset != 0))
    {
        return;
    }

    switch(btn)
    {
        case 3:
        {
            // Up pressed, move to the prior row and set a negative offset
            menu->rows = menu->rows->prev;
            menu->yOffset = -(FONT_HEIGHT_IBMVGA8 + ROW_SPACING);
            break;
        }
        case 1:
        {
            // Down pressed, move to the next row and set a positive offset
            menu->rows = menu->rows->next;
            menu->yOffset = (FONT_HEIGHT_IBMVGA8 + ROW_SPACING);
            break;
        }
        case 0:
        {
            // Left pressed, only change if there are multiple items in this row
            if(menu->rows->d.row.numItems > 1)
            {
                // To properly center the word, measure both old and new centered words
                uint8_t oldWordWidth = textWidth((char*)menu->rows->d.row.items->d.item.name, IBM_VGA_8);
                // Move to the previous item
                menu->rows->d.row.items = menu->rows->d.row.items->prev;
                uint8_t newWordWidth = textWidth((char*)menu->rows->d.row.items->d.item.name, IBM_VGA_8);
                // Set the offset to smootly animate from the old, centered word to the new centered word
                menu->rows->d.row.xOffset = -(newWordWidth + ITEM_SPACING + ((oldWordWidth - newWordWidth - 1) / 2));
            }
            break;
        }
        case 2:
        {
            // Right pressed, only change if there are multiple items in this row
            if(menu->rows->d.row.numItems > 1)
            {
                // To properly center the word, measure both old and new centered words
                uint8_t oldWordWidth = textWidth((char*)menu->rows->d.row.items->d.item.name, IBM_VGA_8);
                // Move to the next item
                menu->rows->d.row.items = menu->rows->d.row.items->next;
                uint8_t newWordWidth = textWidth((char*)menu->rows->d.row.items->d.item.name, IBM_VGA_8);
                // Set the offset to smootly animate from the old, centered word to the new centered word
                menu->rows->d.row.xOffset = oldWordWidth + ITEM_SPACING + ((newWordWidth - oldWordWidth - 1) / 2);
            }
            break;
        }
        case 4:
        {
            // Select pressed. Tell the host mode what item was selected
            if(NULL != menu->cbFunc)
            {
                menu->cbFunc(menu->rows->d.row.items->d.item.name);
            }
            break;
        }
        default:
        {
            break;
        }
    }
}

#endif