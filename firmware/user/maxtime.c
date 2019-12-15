#include "maxtime.h"
#include <osapi.h>
#include <user_interface.h>
#include <stdlib.h>

// maxtime.c
//
// Created 28 September 2019 Author Richard Jones

void ICACHE_FLASH_ATTR maxTimeBegin( struct maxtime_t* mymaxtime )
{
    uint32_t time_now_us = system_get_time();
    mymaxtime->period_us = time_now_us - mymaxtime->start_us ;
    mymaxtime->start_us  = time_now_us;
}

void ICACHE_FLASH_ATTR maxTimeEnd  ( struct maxtime_t* mymaxtime )
{
    uint32_t time_now_us = system_get_time();
    if ( ( time_now_us - mymaxtime->start_us ) > mymaxtime->max_us  )
    {
        mymaxtime->max_us = time_now_us - mymaxtime->start_us;
        os_printf ( "%s: period=%6dus, max=%dus\n",
                    mymaxtime->name,
                    mymaxtime->period_us,
                    mymaxtime->max_us );
    }
}
