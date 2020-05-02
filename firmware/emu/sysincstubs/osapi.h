#ifndef _OSAPI_H
#define _OSAPI_H

#include <c_types.h>


/* timer related */
typedef void os_timer_func_t(void *timer_arg);

typedef struct _os_timer_t {
    struct _os_timer_t *timer_next;
    void               *timer_handle;
    uint32             timer_expire;
    uint32             timer_period;
    os_timer_func_t    *timer_func;
    bool               timer_repeat_flag;
    void               *timer_arg;
} os_timer_t;

#define os_event_t ETSEvent


#endif

