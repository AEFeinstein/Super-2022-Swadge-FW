#ifndef _BUTTONS_H_
#define _BUTTONS_H_

/*============================================================================
 * Enums
 *==========================================================================*/

typedef enum
{
    LEFT   = 0,
    DOWN   = 1,
    RIGHT  = 2,
    UP     = 3,
    ACTION = 4
} button_num;

typedef enum
{
    LEFT_MASK   = 1 << LEFT,
    DOWN_MASK   = 1 << DOWN,
    RIGHT_MASK  = 1 << RIGHT,
    UP_MASK     = 1 << UP,
    ACTION_MASK = 1 << ACTION
} button_mask;



/*============================================================================
 * Function Prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR HandleButtonEventSynchronous(void);
void HandleButtonEventIRQ( uint8_t stat, int btn, int down );
void ICACHE_FLASH_ATTR enableDebounce(bool enable);

#endif /* _BUTTONS_H_ */