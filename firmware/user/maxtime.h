#ifndef _MAXTIME_H
#define _MAXTIME_H
#include <osapi.h>
#include <ets_sys.h>
// maxtime.h
//
// Created 28 September 2019 Author Richard Jones
//
// A utility to measure and print to os_printf() the maximum duration in
// microseconds spent in monitored functions.
// Many functions may be monitored simultaneously declaring multiple
// instances of the data structure maxtime_t.
//
// eg myFunc()
// {
//      static maxtime_t myfunctime = {"myfuncname"};
//      maxTimeStart( &myfunctime );
//      ....
//      maxTimeEnd( &myfunctime );
// }
//
struct maxtime_t
{
    char* name ;
    uint32_t start_us;
    uint32_t period_us;
    uint32_t max_us;
} ;

void maxTimeBegin( struct maxtime_t* mymaxtime );
void maxTimeEnd  ( struct maxtime_t* mymaxtime );

#endif