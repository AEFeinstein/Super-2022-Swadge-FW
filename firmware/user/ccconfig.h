#ifndef _CCCONFIG_H
#define _CCCONFIG_H

#include <c_types.h>

#define HPABUFFSIZE 512

#define CCEMBEDDED
#define NUM_LIN_LEDS 8
#define DFREQ 16000

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

#define ROOT_NOTE_OFFSET	CCS.gROOT_NOTE_OFFSET
#define DFTIIR				CCS.gDFTIIR
#define FUZZ_IIR_BITS  		CCS.gFUZZ_IIR_BITS
#define MAXNOTES  12 //MAXNOTES cannot be changed dynamically.
#define FILTER_BLUR_PASSES	CCS.gFILTER_BLUR_PASSES
#define SEMIBITSPERBIN		CCS.gSEMIBITSPERBIN
#define MAX_JUMP_DISTANCE	CCS.gMAX_JUMP_DISTANCE
#define MAX_COMBINE_DISTANCE CCS.gMAX_COMBINE_DISTANCE
#define AMP_1_IIR_BITS		CCS.gAMP_1_IIR_BITS
#define AMP_2_IIR_BITS		CCS.gAMP_2_IIR_BITS
#define MIN_AMP_FOR_NOTE	CCS.gMIN_AMP_FOR_NOTE
#define MINIMUM_AMP_FOR_NOTE_TO_DISAPPEAR CCS.gMINIMUM_AMP_FOR_NOTE_TO_DISAPPEAR
#define NOTE_FINAL_AMP		CCS.gNOTE_FINAL_AMP
#define NERF_NOTE_PORP		CCS.gNERF_NOTE_PORP
#define USE_NUM_LIN_LEDS	CCS.gUSE_NUM_LIN_LEDS
#define COLORCHORD_OUTPUT_DRIVER	CCS.gCOLORCHORD_OUTPUT_DRIVER
#define COLORCHORD_ACTIVE	CCS.gCOLORCHORD_ACTIVE
#define INITIAL_AMP	CCS.gINITIAL_AMP

//We are not enabling these for the ESP8266 port.
#define LIN_WRAPAROUND 0 
#define SORT_NOTES 0

struct CCSettings
{
	uint8_t gSETTINGS_KEY;
	uint8_t gROOT_NOTE_OFFSET; //Set to define what the root note is.  0 = A.
	uint8_t gDFTIIR;                            //=6
	uint8_t gFUZZ_IIR_BITS;                     //=1
	uint8_t gFILTER_BLUR_PASSES;                //=2
	uint8_t gSEMIBITSPERBIN;                    //=3
	uint8_t gMAX_JUMP_DISTANCE;                 //=4
	uint8_t gMAX_COMBINE_DISTANCE;              //=7
	uint8_t gAMP_1_IIR_BITS;                    //=4
	uint8_t gAMP_2_IIR_BITS;                    //=2
	uint8_t gMIN_AMP_FOR_NOTE;                  //=80
	uint8_t gMINIMUM_AMP_FOR_NOTE_TO_DISAPPEAR; //=64
	uint8_t gNOTE_FINAL_AMP;                    //=12
	uint8_t gNERF_NOTE_PORP;                    //=15
	uint8_t gUSE_NUM_LIN_LEDS;                  // = NUM_LIN_LEDS
	uint8_t gCOLORCHORD_ACTIVE;
	uint8_t gCOLORCHORD_OUTPUT_DRIVER;
	uint8_t gINITIAL_AMP;
};

extern struct CCSettings CCS;

#endif
