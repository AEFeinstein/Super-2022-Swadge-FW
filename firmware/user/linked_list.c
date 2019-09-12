/*
*	linked_list.c
*
*	Created on: Sept 12, 2019
*		Author: Jonathan Moriarty
*/

#include <osapi.h>
#include <user_interface.h>
#include <stdlib.h>

#include "linked_list.h"

// Add to the end of the list.
void ICACHE_FLASH_ATTR push(list_t * list, void * val)
{
    node_t * newLast = malloc(sizeof(node_t));
    newLast->val = val;
    newLast->next = NULL;
    newLast->prev = list->last;
    
    if (list->length == 0)
    {
        list->first = newLast;
        list->last = newLast;
    }
    else
    {
        list->last->next = newLast;
        list->last = newLast;
    }
    list->length++;
}

// Remove from the end of the list.
void * ICACHE_FLASH_ATTR pop(list_t * list)
{
    void * retval = NULL;
    
    // If the list any nodes at all.
    if (list->last != NULL)
    {
        // If the list has only one node, clear the first pointer.
        if (list->length == 1) 
        {
            list->first = NULL;
        }

        // Get the last node val, then free it.
        retval = list->last->val;
        free(list->last);
        list->last = NULL;
        list->length--;
    }

    return retval;
}

// Add to the front of the list.
void ICACHE_FLASH_ATTR unshift(list_t * list, void * val)
{
    node_t * newFirst = malloc(sizeof(node_t));
    newFirst->val = val;
    newFirst->next = list->first;
    newFirst->prev = NULL;
    
    if (list->length == 0)
    {
        list->first = newFirst;
        list->last = newFirst;
    }
    else
    {
        list->first->prev = newFirst;
        list->first = newFirst;
    }
    list->length++;
}

// Remove from the front of the list.
void * ICACHE_FLASH_ATTR shift(list_t * list)
{
    void * retval = NULL;
    
    // If the list any nodes at all.
    if (list->first != NULL)
    {
        // If the list has only one node, clear the last pointer.
        if (list->length == 1) 
        {
            list->last = NULL;
        }

        // Get the last node val, then free it.
        retval = list->first->val;
        free(list->first);
        list->first = NULL;
        list->length--;
    }

    return retval;
}

//TODO: bool return to check if index is valid?
// Add at an index in the list.
void ICACHE_FLASH_ATTR add(list_t * list, void * val, int index)
{   
    // If the index we're trying to add to the start of the list.
    if (index == 0)
    {
         unshift(list, val);
    }
    // Else if the index we're trying to add to is before the end of the list.
    else if (index < list->length - 1) 
    {
        node_t * newNode = malloc(sizeof(node_t));
        newNode->val = val;
        newNode->next = NULL;
        newNode->prev = NULL;

        node_t * current = NULL;
        for (int i = 0; i < index; i++) 
        {
            current = current == NULL ? list->first : current->next;
        }
        
        // We need to adjust the newNode, and the nodes before and after it.
        // current is set to the node before it.

        current->next->prev = newNode;
        newNode->next = current->next;

        current->next = newNode;
        newNode->prev = current;

        list->length++;
    }
    // Else just add the node to the end of the list.
    else
    {
        push(list, val);
    }
}

// Remove at an index in the list.
void * ICACHE_FLASH_ATTR remove(list_t * list, int index)
{
    // If the index we're trying to remove from is the start of the list.
    if (index == 0)
    {
         return shift(list);
    }
    // Else if the index we're trying to remove from is before the end of the list.
    else if (index < list->length - 1) 
    {
        void * retval = NULL;
        
        node_t * current = NULL;
        for (int i = 0; i < index; i++)
        {
            current = current == NULL ? list->first : current->next;
        }
        
        // We need to free the removed node, and adjust the nodes before and after it.
        // current is set to the node before it.
        
        node_t * target = current->next;
        retval = target->val;

        current->next = target->next;
        current->next->prev = current;

        free(target);
        target = NULL;

        list->length--;
        return retval;
    }
    // Else just remove the node at the end of the list.
    else
    {
        return pop(list);
    }
}

// Remove all items from the list.
void ICACHE_FLASH_ATTR clear(list_t * list)
{
    while (list->first != NULL)
    {
        pop(list);
    }
}

