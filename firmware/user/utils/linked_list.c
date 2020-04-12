/*
*   linked_list.c
*
*   Created on: Sept 12, 2019
*       Author: Jonathan Moriarty
*/

#include <osapi.h>
#include <user_interface.h>
#include <stdlib.h>

#include "linked_list.h"

// Add to the end of the list.
void ICACHE_FLASH_ATTR push(list_t* list, void* val)
{
    node_t* newLast = malloc(sizeof(node_t));
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
void* ICACHE_FLASH_ATTR pop(list_t* list)
{
    void* retval = NULL;

    // Get a direct pointer to the node we're removing.
    node_t* target = list->last;

    // If the list any nodes at all.
    if (target != NULL)
    {

        // Adjust the node before the removed node, then adjust the list's last pointer.
        if (target->prev != NULL)
        {
            target->prev->next = NULL;
            list->last = target->prev;
        }
        // If the list has only one node, clear the first and last pointers.
        else
        {
            list->first = NULL;
            list->last = NULL;
        }

        // Get the last node val, then free it and update length.
        retval = target->val;
        free(target);
        list->length--;
    }

    return retval;
}

// Add to the front of the list.
void ICACHE_FLASH_ATTR unshift(list_t* list, void* val)
{
    node_t* newFirst = malloc(sizeof(node_t));
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
void* ICACHE_FLASH_ATTR shift(list_t* list)
{
    void* retval = NULL;

    // Get a direct pointer to the node we're removing.
    node_t* target = list->first;

    // If the list any nodes at all.
    if (target != NULL)
    {

        // Adjust the node after the removed node, then adjust the list's first pointer.
        if (target->next != NULL)
        {
            target->next->prev = NULL;
            list->first = target->next;
        }
        // If the list has only one node, clear the first and last pointers.
        else
        {
            list->first = NULL;
            list->last = NULL;
        }

        // Get the first node val, then free it and update length.
        retval = target->val;
        free(target);
        list->length--;
    }

    return retval;
}

//TODO: bool return to check if index is valid?
// Add at an index in the list.
void ICACHE_FLASH_ATTR add(list_t* list, void* val, int index)
{
    // If the index we're trying to add to the start of the list.
    if (index == 0)
    {
        unshift(list, val);
    }
    // Else if the index we're trying to add to is before the end of the list.
    else if (index < list->length - 1)
    {
        node_t* newNode = malloc(sizeof(node_t));
        newNode->val = val;
        newNode->next = NULL;
        newNode->prev = NULL;

        node_t* current = NULL;
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
void* ICACHE_FLASH_ATTR removeIdx(list_t* list, int index)
{
    // If the list is null or empty, dont touch it
    if(NULL == list || list->length == 0)
    {
        return NULL;
    }
    // If the index we're trying to remove from is the start of the list.
    else if (index == 0)
    {
        return shift(list);
    }
    // Else if the index we're trying to remove from is before the end of the list.
    else if (index < list->length - 1)
    {
        void* retval = NULL;

        node_t* current = NULL;
        for (int i = 0; i < index; i++)
        {
            current = current == NULL ? list->first : current->next;
        }

        // We need to free the removed node, and adjust the nodes before and after it.
        // current is set to the node before it.

        node_t* target = current->next;
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

/**
 * Remove a specific entry from the linked list
 * This only removes the first instance of the entry if it is linked multiple
 * times
 * 
 * @param list  The list to remove an entry from
 * @param entry The entry to remove
 * @return The void* val associated with the removed entry
 */
void* ICACHE_FLASH_ATTR removeEntry(list_t* list, node_t* entry)
{
    // If the list is null or empty, dont touch it
    if(NULL == list || list->length == 0)
    {
        return NULL;
    }
    // If the entry we're trying to remove is the fist one, shift it
    else if(list->first == entry)
    {
        return shift(list);
    }
    // If the entry we're trying to remove is the last one, pop it
    else if(list->last == entry)
    {
        return pop(list);
    }
    // Otherwise it's somewhere in the middle, or doesn't exist
    else
    {
        // Start at list->first->next because we know the entry isn't at the head
        node_t* prev = list->first;
        node_t* curr = prev->next;
        // Iterate!
        while (curr != NULL)
        {
            // Found the node to remove
            if(entry == curr)
            {
                // Save this node's value
                void* retVal = curr->val;

                // Unlink and free the node
                prev->next = curr->next;
                curr->next->prev = prev;
                free(curr);
                list->length--;

                return retVal;
            }

            // Iterate to the next node
            curr = curr->next;
        }
    }
    // Nothing to be removed
    return NULL;
}

// Remove all items from the list.
void ICACHE_FLASH_ATTR clear(list_t* list)
{
    while (list->first != NULL)
    {
        pop(list);
    }
}

