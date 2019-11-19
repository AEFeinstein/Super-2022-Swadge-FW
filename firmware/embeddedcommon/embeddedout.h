//Copyright 2015 <>< Charles Lohr under the ColorChord License.



#ifndef _EMBEDDEDOUT_H
    #define _EMBEDDEDOUT_H

    #include "embeddednf.h"

    extern int gFRAMECOUNT_MOD_SHIFT_INTERVAL;
    extern int gROTATIONSHIFT; //Amount of spinning of pattern around a LED ring

    //TODO fix
    // print debug info wont work on esp8266 need debug to go to usb there
    #ifndef DEBUGPRINT
        #define DEBUGPRINT 0
    #endif

    //Controls brightness
    #ifndef NOTE_FINAL_AMP
        #define NOTE_FINAL_AMP  150   //Number from 0...255
    #endif

    //Controls, basically, the minimum size of the splotches.
    #ifndef NERF_NOTE_PORP
        #define NERF_NOTE_PORP 15 //value from 0 to 255
    #endif

    // used to allocate arrays related to LEDs,
    // so can have up to this may leds without recompile
    // only need to lower if run out of space
#endif
#ifndef MAX_NUM_LIN_LEDS
    #define MAX_NUM_LIN_LEDS 256

    // this is now a parameter and is number of actual LEDs in attached ring.
    #ifndef NUM_LIN_LEDS
        #define NUM_LIN_LEDS 32
    #endif

    #ifndef USE_NUM_LIN_LEDS
        #define USE_NUM_LIN_LEDS NUM_LIN_LEDS
    #endif

    #ifndef COLORCHORD_SHIFT_INTERVAL
        #define COLORCHORD_SHIFT_INTERVAL 0 //how frame interval between shifts. if 0 no shift
    #endif

    #ifndef COLORCHORD_FLIP_ON_PEAK
        #define COLORCHORD_FLIP_ON_PEAK 0 //if non-zero will cause flipping shift on peaks
    #endif

    #ifndef COLORCHORD_SHIFT_DISTANCE
        #define COLORCHORD_SHIFT_DISTANCE 0 //distance of shift
    #endif

    #ifndef COLORCHORD_SORT_NOTES
        #define COLORCHORD_SORT_NOTES 0 // 0 no sort, 1 inc freq, 2 dec amps, 3 dec amps2
    #endif

    #ifndef COLORCHORD_LIN_WRAPAROUND
        #define COLORCHORD_LIN_WRAPAROUND 0// 0 no adjusting, else current led display has minimum deviation to prev
    #endif

    extern uint8_t ledOut[]; //[MAX_NUM_LIN_LEDS*3]
    extern uint8_t RootNoteOffset; //Set to define what the root note is.  0 = A.

    //For doing the nice linear strip LED updates
    //Also added rotation direction changing at peak total amp2 LEDs
    void ICACHE_FLASH_ATTR UpdateLinearLEDs(void);

    // Displays the DFT
    void ICACHE_FLASH_ATTR DFTInLights(void);

    //Pattern of lights independent of input signal
    void ICACHE_FLASH_ATTR PureRotatingLEDs(void);

    uint32_t ECCtoAdjustedHEX( int16_t note, uint8_t sat, uint8_t val );
    uint32_t ICACHE_FLASH_ATTR EHSVtoHEXhelper( uint8_t hue, uint8_t sat, uint8_t val, bool applyGamma );
    uint32_t EHSVtoHEX( uint8_t hue, uint8_t sat, uint8_t val ); //hue = 0..255 // TODO: TEST ME!!!
    uint8_t ICACHE_FLASH_ATTR GAMMA_CORRECT(uint8_t val);

#endif

