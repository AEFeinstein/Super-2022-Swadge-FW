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

//#define INITIAL_AMP     CCS.gINITIAL_AMP
#define RMUXSHIFT       CCS.gRMUXSHIFT
#define ROOT_NOTE_OFFSET    CCS.gROOT_NOTE_OFFSET

#define NUM_SAMPLES_PER_FRAME 32
#define DONT_USE_CSS
#ifdef DONT_USE_CSS
    #define DFT_UPDATE      32
    #define DFTIIR 4
    #define FUZZ_IIR_BITS 1
#else
    #define DFTIIR          CCS.gDFTIIR
    #define DFT_UPDATE      CCS.gDFT_UPDATE
    #define FUZZ_IIR_BITS       CCS.gFUZZ_IIR_BITS
#endif

#ifdef USE_EQUALIZER
    #define EQUALIZER_SET       CCS.gEQUALIZER_SET
#else
    #define EQUALIZER_SET 0
#endif
//MAXNOTES cannot be changed dynamically.
#define MAXNOTES  12
#define FILTER_BLUR_PASSES  CCS.gFILTER_BLUR_PASSES
#define LOWER_CUTOFF        CCS.gLOWER_CUTOFF
#define SEMIBITSPERBIN      CCS.gSEMIBITSPERBIN
#define MAX_JUMP_DISTANCE   CCS.gMAX_JUMP_DISTANCE
#define MAX_COMBINE_DISTANCE CCS.gMAX_COMBINE_DISTANCE
#define AMP1_ATTACK_BITS    CCS.gAMP1_ATTACK_BITS
#define AMP1_DECAY_BITS     CCS.gAMP1_DECAY_BITS
#define AMP_1_MULT      CCS.gAMP_1_MULT
#define AMP2_ATTACK_BITS    CCS.gAMP2_ATTACK_BITS
#define AMP2_DECAY_BITS     CCS.gAMP2_DECAY_BITS
#define AMP_2_MULT      CCS.gAMP_2_MULT
#define MIN_AMP_FOR_NOTE    CCS.gMIN_AMP_FOR_NOTE
#define MINIMUM_AMP_FOR_NOTE_TO_DISAPPEAR CCS.gMINIMUM_AMP_FOR_NOTE_TO_DISAPPEAR
#define NOTE_FINAL_AMP      CCS.gNOTE_FINAL_AMP
#define NOTE_FINAL_SATURATION   CCS.gNOTE_FINAL_SATURATION
#define NERF_NOTE_PORP      CCS.gNERF_NOTE_PORP
#define USE_NUM_LIN_LEDS    CCS.gUSE_NUM_LIN_LEDS
#define SYMMETRY_REPEAT     CCS.gSYMMETRY_REPEAT
#define COLORCHORD_OUTPUT_DRIVER    CCS.gCOLORCHORD_OUTPUT_DRIVER
#define COLORCHORD_ACTIVE   CCS.gCOLORCHORD_ACTIVE
#define COLORCHORD_SHIFT_INTERVAL   CCS.gCOLORCHORD_SHIFT_INTERVAL
#define COLORCHORD_FLIP_ON_PEAK CCS.gCOLORCHORD_FLIP_ON_PEAK
#define COLORCHORD_SHIFT_DISTANCE   CCS.gCOLORCHORD_SHIFT_DISTANCE
#define COLORCHORD_SORT_NOTES   CCS.gCOLORCHORD_SORT_NOTES
#define COLORCHORD_LIN_WRAPAROUND   CCS.gCOLORCHORD_LIN_WRAPAROUND
#define CONFIG_NUMBER       CCS.gCONFIG_NUMBER
//#define NUM_LIN_LEDS      CCS.gNUM_LIN_LEDS

struct CCSettings
{
    uint8_t gSETTINGS_KEY;
    uint8_t gINITIAL_AMP;                       // /8 gives pre-amp muliplier of ADC output
    uint8_t gRMUXSHIFT;                     // 1  shift adjust in DFT32.c
    uint8_t gROOT_NOTE_OFFSET; //Set to define what the root note is.  0 = A.
    uint8_t gDFTIIR;                            //=6
    uint8_t gDFT_UPDATE;                        //=2 bins updated at this interval from HandleInt
    uint8_t gFUZZ_IIR_BITS;                     //=3
    uint8_t gFILTER_BLUR_PASSES;                //=5
    uint8_t gLOWER_CUTOFF;                      //=14 cut off at 256 times this
    uint8_t gSEMIBITSPERBIN;                    //=3
    uint8_t gMAX_JUMP_DISTANCE;                 //=42
    uint8_t gMAX_COMBINE_DISTANCE;              //=44
    uint8_t gAMP1_ATTACK_BITS;                  //=2
    uint8_t gAMP1_DECAY_BITS;                   //=2
    uint8_t gAMP_1_MULT;                        //=16 before amp1 used multiplied by this/16
    uint8_t gAMP2_ATTACK_BITS;                  //=4
    uint8_t gAMP2_DECAY_BITS;                   //=4
    uint8_t gAMP_2_MULT;                        //=16 before amp2 used multiplied by this/16
    uint8_t gMIN_AMP_FOR_NOTE;                  //=45
    uint8_t gMINIMUM_AMP_FOR_NOTE_TO_DISAPPEAR; //=1
    uint8_t gNOTE_FINAL_AMP;                    //=100
    uint8_t gNOTE_FINAL_SATURATION;             //=255
    uint8_t gNERF_NOTE_PORP;                    //=15
    uint8_t gUSE_NUM_LIN_LEDS;                  // = NUM_LIN_LEDS
    uint8_t gSYMMETRY_REPEAT;                   //=0
    uint8_t gCOLORCHORD_ACTIVE;         //=1
    uint8_t gCOLORCHORD_OUTPUT_DRIVER;
    uint8_t gCOLORCHORD_SHIFT_INTERVAL; // ==0 controls speed of shifting if 0 no shift
    uint8_t gCOLORCHORD_FLIP_ON_PEAK; //==0 if non-zero gives flipping at peaks of shift direction, 0 no flip
    uint8_t gCOLORCHORD_SHIFT_DISTANCE; //==0 distance of shift
    uint8_t gCOLORCHORD_SORT_NOTES; //==0  0 no sort, 1 inc freq, 2 dec amps, 3 dec amps2
    uint8_t gCOLORCHORD_LIN_WRAPAROUND; //==0  0 no adjusting, else current led display has minimum deviation to prev
    uint8_t gCONFIG_NUMBER; //=0 from 0 ..NUMBER_STORED_CONFIGURABLES-1
    uint8_t gNUM_LIN_LEDS; // set at compile but can be any value from 1 to 255
    uint8_t gEQUALIZER_SET;                     //=0 when one sweep over freq range to get mic responce
};

extern struct CCSettings CCS;

#endif
