#include "mem.h"
#include "c_types.h"
#include "user_interface.h"

void ICACHE_FLASH_ATTR LoadSSIDAndPassword( char * ssid, char * password )
{
	ets_memcpy( ssid, "testnet", 7 );
	ets_memcpy( password, "testpass", 8 );
}


