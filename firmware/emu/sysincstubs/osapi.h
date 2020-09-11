#ifndef _OSAPI_H
#define _OSAPI_H

#include <c_types.h>
#include <ets_sys.h>

/* timer related */
typedef void os_timer_func_t(void *timer_arg);

typedef ETSTimer os_timer_t;

#define os_event_t ETSEvent


#ifdef USE_US_TIMER
#define os_timer_arm_us(a, b, c) ets_timer_arm_new(a, b, c, 0)
#endif
#define os_timer_arm(a, b, c) ets_timer_arm_new(a, b, c, 1)
#define os_timer_disarm ets_timer_disarm
#define os_timer_done ets_timer_done
#define os_timer_handler_isr ets_timer_handler_isr
#define os_timer_init ets_timer_init
#define os_timer_setfn ets_timer_setfn

int os_printf(const char *format, ...);
int os_sprintf(char*, const char *format, ...);

#endif

