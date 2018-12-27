/*============================================================================
 * Includes
 *==========================================================================*/

#include "mem.h"
#include "c_types.h"
#include "user_interface.h"
#include "ssid.h"
#include <osapi.h>

/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * Copy a default SSID and password into the given pointers
 * WARNING: pointers must point to areas of sufficient memory
 *
 * @param ssid Copy the string "testnet" into this char*.
 * @param password Copy the string "testpass" into this char*
 */
//void ICACHE_FLASH_ATTR LoadSSIDAndPassword( char* ssid, char* password )
//{
//    char ssidStr[] = "testnet"; // No longer than 32 chars
//    char pswdStr[] = "testpass"; // No longer than 64 chars
//    ets_memcpy( ssid, ssidStr, ets_strlen(ssidStr) );
//    ets_memcpy( password, pswdStr, ets_strlen(pswdStr) );
//}
