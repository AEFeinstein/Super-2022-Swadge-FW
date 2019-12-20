#ifndef _CCCONFIG_H
#define _CCCONFIG_H

#include <c_types.h>

// Set various debug prints all false
// used when testing with linux, will need modification to work with ESP8266
#define DFTHIST 0
#define DEBUGPRINT 0
#define FUZZHIST 0
#define SHOWNOTES 0
#define FOLDHIST 0
#define SHOWSAMP 0
#define CHECKOVERFLOW 0

// used by cc with gui to send nice info to o-scope
#define DFTSAMPLE 0

// progressive DFT is original handling highest octave
//    with specified sample rate and successive halving
//    for each subsequent octave
// if 0 does simple DFT on all bins at given sample rate
#define PROGRESSIVE_DFT 0

// will change behaviour slight if using progressive DFT
#define ADJUST_DFT_WITH_OCTAVE 0

//This is for using audio with mic with equal freq response
//#define USE_EQUALIZER

#define HPABUFFSIZE 512

#define CCEMBEDDED
#define DEFAULT_NUM_LEDS 16
#define DFREQ 60

#ifndef DFREQ
    #define DFREQ 60
#endif

#ifndef START_INACTIVE
    #define START_INACTIVE 0
#endif

#define bzero ets_bzero
#define delay_us ets_delay_us
#define get_cpu_frequency ets_get_cpu_frequency
#define install_putc1 ets_install_putc1
#define intr_lock ets_intr_lock
#define intr_unlock ets_intr_unlock
#define isr_attach ets_isr_attach
#define isr_mask ets_isr_mask
#define isr_unmask ets_isr_unmask
#define memcmp ets_memcmp
#define memcpy ets_memcpy
#define memmove ets_memmove
#define memset ets_memset
#define post ets_post
// #define printf ets_printf
#define putc ets_putc
#define run ets_run
#define set_idle_cb ets_set_idle_cb
#define snprintf ets_snprintf
#define sprintf ets_sprintf
#define strcat ets_strcat
#define strchr ets_strchr
#define strcmp ets_strcmp
#define strcpy ets_strcpy
#define strdup ets_strdup
#define strlen ets_strlen
#define strncmp ets_strncmp
#define strncpy ets_strncpy
#define strrchr ets_strrchr
#define strstr ets_strstr
#define task ets_task
#define timer_arm_new ets_timer_arm_new
#define timer_disarm ets_timer_disarm
#define timer_done ets_timer_done
#define timer_handler_isr ets_timer_handler_isr
#define timer_init ets_timer_init
#define timer_setfn ets_timer_setfn
#define update_cpu_frequency ets_update_cpu_frequency
#define vprintf ets_vprintf
#define vsnprintf ets_vsnprintf
#define vsprintf ets_vsprintf
#define write_char ets_write_char

#endif
