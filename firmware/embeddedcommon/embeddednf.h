//Copyright 2015 <>< Charles Lohr under the ColorChord License.

#ifndef _EMBEDDEDNF_H
#define _EMBEDDEDNF_H

#include "ccconfig.h"

#ifndef MAX_AMP2_LOG2
    #define MAX_AMP2_LOG2 16
#endif


//Use a 32-bit DFT.  It won't work for AVRs, but for any 32-bit systems where
//they can multiply quickly, this is the bees knees.
#define USE_32DFT

#ifndef DFREQ
    #define DFREQ     60
#endif

//You may make this a float. If PRECOMPUTE_FREQUENCY_TABLE is defined, then
//it will create the table at compile time, and the float will never be used
//runtime.
#define BASE_FREQ 0.33 // 20 bpm

//The higher the number the slackier your FFT will be come.
#ifndef FUZZ_IIR_BITS
    #define FUZZ_IIR_BITS  1
#endif

//Notes are the individually identifiable notes we receive from the sound.
//We track up to this many at one time.  Just because a note may appear to
//vaporize in one frame doesn't mean it is annihilated immediately.
#ifndef MAXNOTES
    #define MAXNOTES  12
#endif

//We take the raw signal off of the
#ifndef FILTER_BLUR_PASSES
    #define FILTER_BLUR_PASSES 2
#endif

//Determines bit shifts for where notes lie.  We represent notes with an
//uint8_t.  We have to define all of the possible locations on the note line
//in this. note_frequency = 0..((1<<SEMIBITSPERBIN)*FIXBPERO-1)
#ifndef SEMIBITSPERBIN
    #define SEMIBITSPERBIN 3
#endif

#define NOTERANGE ((1<<SEMIBITSPERBIN)*FIXBPERO)


//If there is detected note this far away from an established note, we will
//then consider this new note the same one as last time, and move the
//established note.  This is also used when combining notes.  It is this
//distance times two.
#ifndef MAX_JUMP_DISTANCE
    #define MAX_JUMP_DISTANCE 4
#endif

#ifndef MAX_COMBINE_DISTANCE
    #define MAX_COMBINE_DISTANCE 7
#endif

//These control how quickly the IIR for the note strengths respond for
// both attack when increasing and decay when decreasing.
// AMP1 is the (slow) response to determine size of splotches
// AMP2 is the (quick) response to set the brightness of the notes.

#ifndef AMP1_ATTACK_BITS
    #define AMP1_ATTACK_BITS 4
#endif

#ifndef AMP1_DECAY_BITS
    #define AMP1_DECAY_BITS 4
#endif

#ifndef AMP2_ATTACK_BITS
    #define AMP2_ATTACK_BITS 2
#endif

#ifndef AMP2_DECAY_BITS
    #define AMP2_DECAY_BITS 2
#endif

// the amps are scaled by mult/16
#ifndef AMP_1_MULT
    #define AMP_1_MULT 16
#endif

#ifndef AMP_2_MULT
    #define AMP_2_MULT 16
#endif

//This is the amplitude, coming from folded_bins.  If the value is below this
//it is considered a non-note.
#ifndef MIN_AMP_FOR_NOTE
    #define MIN_AMP_FOR_NOTE 80
#endif

//If the strength of a note falls below this, the note will disappear, and be
//recycled back into the unused list of notes.
#ifndef MINIMUM_AMP_FOR_NOTE_TO_DISAPPEAR
    #define MINIMUM_AMP_FOR_NOTE_TO_DISAPPEAR 64
#endif

//This prevents compilation of any floating-point code, but it does come with
//an added restriction: Both DFREQ and BASE_FREQ must be #defined to be
//constants.
//Currently only works for FIXBPERO 12, 24 and 36
#define PRECOMPUTE_FREQUENCY_TABLE

#include "DFT32.h"

extern uint16_t fuzzed_bins[]; //[FIXBINS]  <- The Full DFT after IIR, Blur and Taper

#ifdef USE_EQUALIZER
    extern uint16_t max_bins[]; //[FIXBINS]  <- Max of bins after Full DFT after IIR, Blur and Taper
    extern uint32_t maxallbins;
#endif

extern uint16_t folded_bins[]; //[FIXBPERO] <- The folded fourier output.

//frequency of note; Note if it is <0, then  it is not set. It is
//generally a value from
extern int16_t  note_peak_freqs[]; //[MAXNOTES]
extern uint16_t note_peak_amps[];  //[MAXNOTES]
extern uint16_t note_peak_amps2[]; //[MAXNOTES]  (Responds quicker)
extern uint8_t  note_jumped_to[];  //[MAXNOTES] When a note combines into another one,
extern uint16_t octave_bins[OCTAVES];

//this records where it went.  I.e. if your note just disappeared, check this flag.

void ICACHE_FLASH_ATTR  UpdateFreqs(void);      //Not user-useful on most systems.
void ICACHE_FLASH_ATTR  HandleFrameInfo(void);  //Not user-useful on most systems



//Call this when starting.
void ICACHE_FLASH_ATTR  InitColorChord(void);


#endif

