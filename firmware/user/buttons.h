#ifndef _BUTTONS_H_
#define _BUTTONS_H_

/*============================================================================
 * Enums
 *==========================================================================*/

typedef enum
{
    UP    = 0x01,
    LEFT  = 0x02,
    RIGHT = 0x04,
    DOWN  = 0x08,
} button_mask;

/*============================================================================
 * Function Prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR HandleButtonEventSynchronous(void);
void HandleButtonEventIRQ( uint8_t stat, int btn, int down );
void ICACHE_FLASH_ATTR enableDebounce(bool enable);

#endif /* _BUTTONS_H_ */