#ifndef _BUTTONS_H_
#define _BUTTONS_H_

/*============================================================================
 * Enums
 *==========================================================================*/

typedef enum
{
    LEFT    = 0x01,
    DOWN    = 0x02,
    RIGHT   = 0x04,
    UP      = 0x08,
    ACTION  = 0x10,
} button_mask;

/*============================================================================
 * Function Prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR HandleButtonEventSynchronous(void);
void HandleButtonEventIRQ( uint8_t stat, int btn, int down );
void ICACHE_FLASH_ATTR enableDebounce(bool enable);

#endif /* _BUTTONS_H_ */