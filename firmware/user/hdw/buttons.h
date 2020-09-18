#ifndef _BUTTONS_H_
#define _BUTTONS_H_

/*============================================================================
 * Enums
 *==========================================================================*/

typedef enum
{
    LEFT_MASK   = 0x01,
    DOWN_MASK   = 0x02,
    RIGHT_MASK  = 0x04,
    UP_MASK     = 0x08,
    ACTION_MASK = 0x0F
} button_mask;

typedef enum
{
    LEFT   = 0,
    DOWN   = 1,
    RIGHT  = 2,
    UP     = 3,
    ACTION = 4
} button_num;

/*============================================================================
 * Function Prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR HandleButtonEventSynchronous(void);
void HandleButtonEventIRQ( uint8_t stat, int btn, int down );
void ICACHE_FLASH_ATTR enableDebounce(bool enable);

#endif /* _BUTTONS_H_ */