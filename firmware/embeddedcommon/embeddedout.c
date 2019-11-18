//Copyright 2015 <>< Charles Lohr under the ColorChord License.
#include <osapi.h>
#include "embeddedout.h"
#if DEBUGPRINT
    #include <stdio.h>
#endif

/*============================================================================
 * Prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR Sort(uint8_t orderType, uint16_t values[], uint16_t* map, uint8_t num);
void ICACHE_FLASH_ATTR AssignColorledOut(uint32_t color, int16_t ledpos, int16_t jshift, uint8_t repeats,
        uint8_t led_spacing_gap );

uint8_t ledOut[MAX_NUM_LIN_LEDS * 3];
uint16_t ledAmpOut[MAX_NUM_LIN_LEDS];
int16_t ledFreqOut[MAX_NUM_LIN_LEDS];
int16_t ledFreqOutOld[MAX_NUM_LIN_LEDS];

uint32_t flip_amount_prev = 0;
int diff_flip_amount_prev = 0;
int rot_dir = 1; // initial rotation direction 1
int move_on_peak = 0;
int16_t ColorCycle = 0;
#define DECREASING 2
#define INCREASING 1


void ICACHE_FLASH_ATTR Sort(uint8_t orderType, uint16_t values[], uint16_t* map, uint8_t num)
{
    //    bubble sort on a specified orderType to reorder sorted_note_map
    uint8_t holdmap;
    uint8_t change;
    int not_correct_order;
    int i, j;
    for( i = 0; i < num; i++ )
    {
        change = 0;
        for( j = 0; j < num - 1 - i; j++ )
        {
            switch(orderType)
            {
                case DECREASING :
                    not_correct_order = values[map[j]] < values[map[j + 1]];
                    break;
                default : // increasing
                    not_correct_order = values[map[j]] > values[map[j + 1]];
            }
            if ( not_correct_order )
            {
                change = 1;
                holdmap = map[j];
                map[j] = map[j + 1];
                map[j + 1] = holdmap;
            }
        }
        if (!change)
        {
            return;
        }
    }
}

// Routine to inject ledpos LED from USE_NUM_LIN_LEDS into the NUM_LIN_LEDS with shift, symmetry repeats and gaps
void ICACHE_FLASH_ATTR AssignColorledOut(uint32_t color, int16_t ledpos, int16_t jshift, uint8_t repeats,
        uint8_t led_spacing_gap )
{
    int16_t i, indled;
    uint16_t rmult = (NUM_LIN_LEDS << 8) / (repeats + 1);
    for (i = 0; i <= repeats; i++)
    {
        indled = jshift + ledpos * (1 + led_spacing_gap); // produce gaps
        indled += ((i * rmult) >> 8); // produce symmetry repeats
        //if( indled >= NUM_LIN_LEDS ) indled -= NUM_LIN_LEDS; // this ok if no gaps
        indled %= NUM_LIN_LEDS; // needed if putting in gaps as could exceed 2*NUM_LIN_LEDS
        ledOut[indled * 3 + 0] = ( color >> 0 ) & 0xff;
        ledOut[indled * 3 + 1] = ( color >> 8 ) & 0xff;
        ledOut[indled * 3 + 2] = ( color >> 16 ) & 0xff;
    }
}


void ICACHE_FLASH_ATTR UpdateLinearLEDs()
{
    //Source material:
    /*
        extern uint16_t fuzzed_bins[]; //[FIXBINS]  <- The Full DFT after IIR, Blur and Taper
        extern uint16_t max_bins[]; //[FIXBINS]  <- Max of bins after Full DFT after IIR, Blur and Taper
        extern uint16_t octave_bins[OCTAVES];
        extern uint32_t maxallbins;
        extern uint16_t folded_bins[]; //[FIXBPERO] <- The folded fourier output.
        extern int16_t  note_peak_freqs[];
        extern uint16_t note_peak_amps[];  //[MAXNOTES]
        extern uint16_t note_peak_amps2[];  //[MAXNOTES]  (Responds quicker)
        extern uint8_t  note_jumped_to[]; //[MAXNOTES] When a note combines into another one,
        extern int gFRAMECOUNT_MOD_SHIFT_INTERVAL; // modular count of calls to NewFrame() in user_main.c
        (from ccconfig.h or defaults defined in embeddedout.h
        COLORCHORD_SHIFT_INTERVAL; // controls speed of shifting if 0 no shift
        COLORCHORD_FLIP_ON_PEAK; //if non-zero determines flipping of shift direction or shifting at peaks , 0 no flip
        COLORCHORD_SHIFT_DISTANCE; //low order 6 bits is distance of shift, top 2 bits gives gap for inserting USE_NUM_LIN_LEDS in NUN_LIN_LEDS
        COLORCHORD_SORT_NOTES; // 0 no sort, 1 inc freq, 2 dec amps, 3 dec amps2
        COLORCHORD_LIN_WRAPAROUND; // 0 no adjusting, else current led display has minimum deviation to prev
    DONE in anticipation of refactoring
    1. Which notes to display? all, all non-zero amp, top N of non-zero, top with amp above min proportion of total
    2. What order to display?
    3. Max leds (from USE_NUM_LIN_LEDS leds) to use each displayed note?  equal amounts, proportional to note's amp,
      (thinking this get mapped to consecutive leds giving segments, but could disperse throughout)
    4. How to display the note in its interval: color by freq and (use fixed brightness, brightness prop to amp2; mask to fade out from one or both ends, length prop
    to amp2 or amp1.
    5. How to embed USE_NUM_LIN_LEDS leds into full ring of NUM_LIN_LEDS leds. Repeat, Move and/or flip via peaks of total amp or octave_bin values


    */

    //Notes are found above a minimum amplitude
    //Goal: On a linear array of size USE_NUM_LIN_LEDS Make splotches of light that are proportional to the amps strength of theses notes.
    //Color them according to note_peak_freq with brightness related to amps2
    //Put this linear array on a ring with NUM_LIN_LEDS and optionally rotate it with optionally direction changes on peak amps2

    int16_t i; // uint8_t i; caused instability especially for large no of LEDS
    int16_t j, l;
    uint32_t k;
    int16_t minimizingShift;
    uint32_t porpamps[MAXNOTES]; //number of LEDs for each corresponding note.
    uint16_t sorted_note_map[MAXNOTES]; //mapping from which note into the array of notes from the rest of the system.
    uint16_t snmapmap[MAXNOTES];
    uint8_t sorted_map_count = 0;
    uint32_t note_nerf_a = 0;
    uint32_t total_note_a = 0;
    uint32_t flip_amount = 0;
    int diff_flip_amount = 0;
    //TODO these next two should be different gui parameters
    uint8_t shift_dist = COLORCHORD_SHIFT_DISTANCE & 0x3f;
    uint8_t led_spacing_gap = COLORCHORD_SHIFT_DISTANCE >> 6;
    int16_t jshift; // int8_t jshift; caused instability especially for large no of LEDS



#if DEBUGPRINT
    printf( "Note Peak Freq: " );
    for( i = 0; i < MAXNOTES; i++ )
    {
        printf( " %5d /", note_peak_freqs[i]  );
    }
    printf( "\n" );
    printf( "Note Peak Amps: " );
    for( i = 0; i < MAXNOTES; i++ )
    {
        printf( " %5d /", note_peak_amps[i]  );
    }
    printf( "\n" );
    printf( "Note Peak Amp2: " );
    for( i = 0; i < MAXNOTES; i++ )
    {
        printf( " %5d /", note_peak_amps2[i]  );
    }
    printf( "\n" );
    printf( "Note jumped to: " );
    for( i = 0; i < MAXNOTES; i++ )
    {
        printf( " %5d /", note_jumped_to[i]  );
    }
    printf( "\n" );
#endif

    for( i = 0; i < MAXNOTES; i++ )
    {
        if( note_peak_freqs[i] < 0)
        {
            continue;
        }
        sorted_note_map[sorted_map_count] = i;
        sorted_map_count++;
        total_note_a += note_peak_amps[i];
    }
    Sort(DECREASING, note_peak_amps, sorted_note_map, sorted_map_count);

    // eliminate and reduces count of notes to keep top ones or those with amp
    //  too small relative to non-eliminated
    //  adjust total amplitude
    if (NERF_NOTE_PORP <= 100)
    {
        //All notes to be used will have amps1 >= floor of NERF_NOTE_PORP percent of total amplitudes
        // e.g. NERF_NOTE_PORP = 25 and total amp1 130003 all notes to use will have amp1 >= 32500
        note_nerf_a = total_note_a * NERF_NOTE_PORP / 100;
        j = sorted_map_count - 1;
        while (j >= 0)
        {
            uint16_t ist = note_peak_amps[sorted_note_map[j]];
            if( ist < note_nerf_a )
            {
                total_note_a -= ist;
                note_nerf_a = total_note_a * NERF_NOTE_PORP / 100;
                sorted_map_count--;
                j--;
                continue;
            }
            else
            {
                break;
            }
        }
    }
    else
    {
        //Use at most NERF_NOTE_PORP - 100 top notes
        while (sorted_map_count > NERF_NOTE_PORP - 100)
        {
            sorted_map_count--;
            total_note_a -= note_peak_amps[sorted_note_map[sorted_map_count]];
        }
    }


    // Options here of what to use for flipping (0 in bit 6) or shifting (1 in bit 6) on peaks
    // 1 in bit 5 means total amplitude of notes, 1 octave_bin[0], 2 octave_bin[1], 3 octave_bin[0]+ocatave_bin[1] etc.
    // only for 5 or fewer octaves possible BUG if OCTAVES is 8 or more
    flip_amount = 0;
    if (COLORCHORD_FLIP_ON_PEAK == 0)
    {
    }
    else if (COLORCHORD_FLIP_ON_PEAK & (1 << 5))
    {
        flip_amount = total_note_a;
    }
    else
    {
        for (j = 0; j < OCTAVES; j++)
        {
            if (COLORCHORD_FLIP_ON_PEAK & (1 << j))
            {
                flip_amount += octave_bins[j];
            }
        }
    }

    diff_flip_amount = flip_amount_prev - flip_amount; // used to check increasing or decreasing and find when flip

#define ORDER_ORGINAL 0
#define ORDER_FREQ_INC 1
#define ORDER_AMP1_DEC 2
#define ORDER_AMP2_DEC 3

    switch (COLORCHORD_SORT_NOTES)
    {
        case ORDER_ORGINAL : // restore orginal note order
            for (i = 0; i < sorted_map_count; i++)
            {
                snmapmap[i] = i;
            }
            Sort(INCREASING, sorted_note_map, snmapmap, sorted_map_count);
            break;
        case ORDER_FREQ_INC :
            Sort(INCREASING, (uint16_t*)note_peak_freqs, sorted_note_map, sorted_map_count);
            break;
        case ORDER_AMP1_DEC: // already sorted like this
            //Sort(DECREASING, note_peak_amps, sorted_note_map, sorted_map_count);
            break;
        case ORDER_AMP2_DEC :
            Sort(DECREASING, note_peak_amps2, sorted_note_map, sorted_map_count);
            break;
        default:
            break;
    }


    //Make a copy of all of the variables into these local ones so we don't have to keep triple or double-dereferencing.
    uint16_t local_peak_amps[MAXNOTES];
    uint16_t local_peak_amps2[MAXNOTES];
    int16_t  local_peak_freq[MAXNOTES];
    // uint8_t  local_note_jumped_to[MAXNOTES];

    switch (COLORCHORD_SORT_NOTES)
    {
        case ORDER_ORGINAL :
            for( i = 0; i < sorted_map_count; i++ )
            {
                local_peak_amps[i] = note_peak_amps[sorted_note_map[snmapmap[i]]];
                local_peak_amps2[i] = note_peak_amps2[sorted_note_map[snmapmap[i]]];
                local_peak_freq[i] = note_peak_freqs[sorted_note_map[snmapmap[i]]];
                // local_note_jumped_to[i] = note_jumped_to[sorted_note_map[snmapmap[i]]];
            }
            break;
        case ORDER_FREQ_INC :
        case ORDER_AMP1_DEC :
        case ORDER_AMP2_DEC :
        default :
            for( i = 0; i < sorted_map_count; i++ )
            {
                local_peak_amps[i] = note_peak_amps[sorted_note_map[i]];
                local_peak_amps2[i] = note_peak_amps2[sorted_note_map[i]];
                local_peak_freq[i] = note_peak_freqs[sorted_note_map[i]];
                // local_note_jumped_to[i] = note_jumped_to[sorted_note_map[i]];
            }
    }

    // Zero all led's - maybe for very large number of led's this takes too much time and need to zero only onces that won't get reassigned below
    memset( ledOut, 0, sizeof( ledOut ) );

    if( total_note_a == 0 )
    {
        return;
    }

    // Assign number of LEDs for each note: proportional to amp vs equal fraction
    uint32_t porportional = (uint32_t)(USE_NUM_LIN_LEDS << 16) / ((uint32_t)total_note_a);
    uint16_t total_accounted_leds = 0;
    for( i = 0; i < sorted_map_count; i++ )
    {
        switch (COLORCHORD_OUTPUT_DRIVER & 0x01)
        {
            case 0 : // proportional to amps
                porpamps[i] = (local_peak_amps[i] * porportional) >> 16;
                break;
            default : // equal sized
                porpamps[i] = USE_NUM_LIN_LEDS / sorted_map_count;
        }
        total_accounted_leds += porpamps[i];
    }

    // porpamps[i] is the floor of correct proportion so adjoin some if needed
    int16_t total_unaccounted_leds = USE_NUM_LIN_LEDS - total_accounted_leds;
    for( i = 0; (i < sorted_map_count) && total_unaccounted_leds; i++ )
    {
        porpamps[i]++;
        total_unaccounted_leds--;
    }


#if DEBUGPRINT
    printf( "note_nerf_a = %d,  total_note_a =  %d, porportional = %d, total_accounted_leds = %d \n", note_nerf_a,
            total_note_a, porportional,  total_accounted_leds );
    printf("snm: ");
    for( i = 0; i < sorted_map_count; i++ )
    {
        printf( "%d /", sorted_note_map[i]);
    }
    printf( "\n" );

    printf("npf: ");
    for( i = 0; i < sorted_map_count; i++ )
    {
        printf( "%d /", note_peak_freqs[sorted_note_map[i]]);
    }
    printf( "\n" );

    printf("npa: ");
    for( i = 0; i < sorted_map_count; i++ )
    {
        printf( "%d /", note_peak_amps[sorted_note_map[i]]);
    }
    printf( "\n" );

    printf("lpf: ");
    for( i = 0; i < sorted_map_count; i++ )
    {
        printf( "%d /", local_peak_freq[i]);
    }
    printf( "\n" );

    printf("lpa: ");
    for( i = 0; i < sorted_map_count; i++ )
    {
        printf( "%d /", local_peak_amps[i]);
    }
    printf( "\n" );

    printf("lpa2: ");
    for( i = 0; i < sorted_map_count; i++ )
    {
        printf( "%d /", local_peak_amps2[i]);
    }
    printf( "\n" );

    printf("porp: ");
    for( i = 0; i < sorted_map_count; i++ )
    {
        printf( "%d /", porpamps[i]);
    }
    printf( "\n" );

    printf("lnjt: ");
    for( i = 0; i < sorted_map_count; i++ )
    {
        printf( "%d /", local_note_jumped_to[i]);
    }
    printf( "\n" );
#endif



    //Assign the linear LEDs info for 0, 1, ..., USE_NUM_LIN_LEDS
    //Each note (above a minimum amplitude) produces an interval
    //Its color relates to the notes frequency, display brightness and interval size -
    //general idea: ledAmpOut[j] = AmpFun(local_peak_amps2[i]/2^16, k/porpamps[i]) *NOTE_FINAL_AMP;
    // AmpFun(relative amp, relative distance) returns relative brightness
    //0,1 AmpFun(a,d) = a gives orginal brightness proportional to a
    //2,3 AmpFun(a,d) = 1 if d<a else 0 length from start proportional to a
    //4,5 AmpFun(a,d) = 1 - d/a for d<a else 0  fade over length from start proportional to a
    //6,7 AmpFun(a,d) = 1 - (1-a)(1-2d) for d<1/2 else 1 - (1-a)(2d-1)  full at middle fade to a at ends
    //8,9 AmpFun(a,d) = a - a(N-1)/N(1-2d) for d<1/2 else a - a(N-1)/N(2d-1)  a at middle fade to a/N at ends
    //10,11 AmpFun(a,d) = 1 for (2d+a>=1) and (a+1>=2d) else 0 width of middle interval proportional to a full
    //12,13 AmpFun(a,d) = 1 for (2d+a>=1) and (a+1>=2d) else 0  width of middle interval proportional to a full then taper either side to 0

    j = 0;
    for( i = 0; i < sorted_map_count; i++ )
    {
        for (k = 0; k < porpamps[i]; k++, j++)
        {
            ledFreqOut[j] = local_peak_freq[i];
            switch (COLORCHORD_OUTPUT_DRIVER)
            {
                case 2 :
                case 3 :
                    ledAmpOut[j] = (k << 16) < ((uint32_t)local_peak_amps2[i] * porpamps[i]) ? NOTE_FINAL_AMP : 0;
                    break;
                case 4 :
                case 5 :
                    ledAmpOut[j] = (k << 16) < ((uint32_t)local_peak_amps2[i] * porpamps[i]) ?
                                   NOTE_FINAL_AMP - (uint32_t)NOTE_FINAL_AMP * (k << 16) / local_peak_amps2[i] / porpamps[i] : 0;
                    break;
                //TODO FIX 6,7,8,9 not fading nicely from middle like 4 does from end
                case 6 :
                case 7 :
                    ledAmpOut[j] = (k << 1) < porpamps[i] ?
                                   NOTE_FINAL_AMP - (((uint32_t)NOTE_FINAL_AMP * ((1 << 16) - local_peak_amps2[i]) * (porpamps[i] -
                                                      (k << 1)) / porpamps[i]) >> 16) :
                                   NOTE_FINAL_AMP - (((uint32_t)NOTE_FINAL_AMP * ((1 << 16) - local_peak_amps2[i]) * ((
                                                          k << 1) - porpamps[i]) / porpamps[i]) >> 16);
                    break;
                case 8 :
                case 9 :
                    ledAmpOut[j] = (k << 1) < porpamps[i] ?
                                   (uint32_t)local_peak_amps2[i]  - local_peak_amps2[i] * 3 / 4 * (porpamps[i] - (k << 1)) / porpamps[i] :
                                   (uint32_t)local_peak_amps2[i]  - local_peak_amps2[i] * 3 / 4 * ((k << 1) - porpamps[i]) / porpamps[i];
                    ledAmpOut[j] = ((uint32_t)NOTE_FINAL_AMP * ledAmpOut[j]) >> 16;
                    break;
                case 10 :
                case 11 :
                    ledAmpOut[j] = ( (k << 17) / porpamps[i]  + (uint32_t)local_peak_amps2[i] >= (1 << 16) ) &&
                                   (( uint32_t)local_peak_amps2[i] + (1 << 16) >= (k << 17) / porpamps[i]   ) ? NOTE_FINAL_AMP : 0;
                    break;
                case 12 :
                case 13 :
                    ledAmpOut[j] = ( (k << 17) / porpamps[i]  + (uint32_t)local_peak_amps2[i] >= (1 << 16) ) &&
                                   (( uint32_t)local_peak_amps2[i] + (1 << 16) >= (k << 17) / porpamps[i]   ) ? NOTE_FINAL_AMP :
                                   (k << 1) < porpamps[i] ? ((uint32_t)NOTE_FINAL_AMP * (k << 17) / porpamps[i] / ((1 << 16) - local_peak_amps2[i]) ) :
                                   ((uint32_t)NOTE_FINAL_AMP * (((porpamps[i] - k) << 17)) / porpamps[i] / ((1 << 16) - local_peak_amps2[i]) );
                    break;
                case 14 :
                case 15 :
                    ledAmpOut[j] = NOTE_FINAL_AMP;
                    break;
                default :
                    ledAmpOut[j] = ((uint32_t)local_peak_amps2[i] * NOTE_FINAL_AMP) >> 16; //(1)
            }

        }
    }




    //This part possibly run on an embedded system with small number of LEDs.
    if (COLORCHORD_LIN_WRAPAROUND )
    {
        //printf("NOTERANGE: %d ", NOTERANGE); //192
        // finds an index minimizingShift so that shifting the used leds will have the minimum deviation
        //    from the previous linear pattern
        uint16_t midx = 0;
        uint32_t mqty = 100000000;
        for( j = 0; j < USE_NUM_LIN_LEDS; j++ )
        {
            uint32_t dqty;
            uint16_t localj;
            dqty = 0;
            localj = j;
            for( l = 0; l < USE_NUM_LIN_LEDS; l++ )
            {
                //TODO  d might be better if used both freq and amp, now only using freq
                int32_t d = (int32_t)ledFreqOut[localj] - (int32_t)ledFreqOutOld[l];
                if( d < 0 )
                {
                    d *= -1;
                }
                if( d > (NOTERANGE >> 1) )
                {
                    d = NOTERANGE - d + 1;
                }
                dqty += ( d * d );
                localj++;
                if( localj >= USE_NUM_LIN_LEDS )
                {
                    localj = 0;
                }
            }
            if( dqty < mqty )
            {
                mqty = dqty;
                midx = j;
            }
        }
        minimizingShift = midx;
        //printf("spin: %d, min deviation: %d\n", minimizingShift, mqty);
    }
    else
    {
        minimizingShift = 0;
    }
    // if option change direction and set to move on max peaks of total amplitude
    if (COLORCHORD_FLIP_ON_PEAK )
    {
        if (diff_flip_amount_prev <= 0 && diff_flip_amount > 0)
        {
            rot_dir *= -1;
            move_on_peak = 1;
        }
        else
        {
            move_on_peak = 0;
        }
    }
    else
    {
        rot_dir = 1;
    }

    // want possible extra spin to relate to changes peak intensity
    // now every COLORCHORD_SHIFT_INTERVAL th frame
    if (COLORCHORD_SHIFT_INTERVAL != 0 )
    {
        if ( gFRAMECOUNT_MOD_SHIFT_INTERVAL == 0 )
        {
            if (COLORCHORD_FLIP_ON_PEAK & (1 << 6) ) // shift on peak
            {
                gROTATIONSHIFT += move_on_peak * shift_dist;
                //printf("tna %d dfap dfa %d %d rot_dir %d, j shift %d\n", total_note_a, diff_flip_amount_prev,  diff_flip_amount, rot_dir, j);
            }
            else     // shift
            {
                gROTATIONSHIFT += rot_dir * shift_dist;
            }
        }
    }
    else
    {
        gROTATIONSHIFT = 0; // reset
    }

    // compute shift to start rotating pattern around all the LEDs
    jshift = ( gROTATIONSHIFT ) % NUM_LIN_LEDS; // neg % pos is neg so fix
    if ( jshift < 0 )
    {
        jshift += NUM_LIN_LEDS;
    }

#if DEBUGPRINT
    printf("rot_dir %d, gROTATIONSHIFT %d, jshift %d, gFRAMECOUNT_MOD_SHIFT_INTERVAL %d\n", rot_dir, gROTATIONSHIFT, jshift,
           gFRAMECOUNT_MOD_SHIFT_INTERVAL);
    printf("NOTE_FINAL_AMP = %d\n", NOTE_FINAL_AMP);
    printf("leds: ");
#endif

    // put linear pattern of USE_NUM_LIN_LEDS on earlier cleared ring NUM_LIN_LEDs
    for( l = 0; l < USE_NUM_LIN_LEDS; l++, minimizingShift++ )
    {
        //lefFreqOutOld and adjusting minimizingShift needed only if wraparound
        if ( COLORCHORD_LIN_WRAPAROUND )
        {
            if( minimizingShift >= USE_NUM_LIN_LEDS )
            {
                minimizingShift = 0;
            }
            ledFreqOutOld[l] = ledFreqOut[minimizingShift];
        }
        uint16_t amp = ledAmpOut[minimizingShift];
#if DEBUGPRINT
        printf("%d:%d/", ledFreqOut[minimizingShift], amp);
#endif
        if( amp > NOTE_FINAL_AMP )
        {
            amp = NOTE_FINAL_AMP;
        }
        uint32_t color = ECCtoAdjustedHEX( ledFreqOut[minimizingShift], NOTE_FINAL_SATURATION, amp );

        AssignColorledOut(color, l, jshift, SYMMETRY_REPEAT, led_spacing_gap );
    }



#if DEBUGPRINT
    printf( "\n" );
    printf("bytes: ");
    for( i = 0; i < USE_NUM_LIN_LEDS; i++ )
    {
        printf( "%02x%02x%02x-", ledOut[i * 3 + 0], ledOut[i * 3 + 1], ledOut[i * 3 + 2]);
    }
    printf( "\n\n" );
#endif
    flip_amount_prev = flip_amount;
    diff_flip_amount_prev = diff_flip_amount;
} //end UpdateLinearLEDs()

void ICACHE_FLASH_ATTR DFTInLights()
{
    // Display DFT, Fuzzed, Octaves or Folded histogram by mapping to USE_NUM_LIN_LEDS
    int16_t i;
    int16_t fbind;
    int16_t freq;
    uint16_t amp;
    uint16_t*   bins;
    uint8_t nbins;
    uint8_t led_spacing_gap = COLORCHORD_SHIFT_DISTANCE >> 6;
    switch( NERF_NOTE_PORP )
    {
        case 1:
#ifdef USE_32DFT
            bins = embeddedbins32;
#else
            bins = embeddedbins;
#endif
            nbins = FIXBINS;
            break;
        case 2:
            bins = fuzzed_bins;
            nbins = FIXBINS;
            break;
        case 3:
            bins = octave_bins;
            nbins = OCTAVES;
            break;
        default:
            bins = folded_bins;
            nbins = FIXBPERO;
    };

    memset( ledOut, 0, sizeof( ledOut ) );
    for( i = 0; i < USE_NUM_LIN_LEDS; i++ )
    {
        //      fbind = i*(nbins-1)/(USE_NUM_LIN_LEDS-1); // exact tranformation but then need check divide by zero
        fbind = i * nbins / USE_NUM_LIN_LEDS; // this is good enough and still will not exceed nbins-1

        //      assign colors (0, 1, ... NOTERANGE-1 )
        //      brightness is value of bins.
        amp = bins[fbind];
        amp = (((uint32_t)(amp)) * NOTE_FINAL_AMP * AMP_1_MULT) >> MAX_AMP2_LOG2; // for PC 14;
        if( amp > NOTE_FINAL_AMP )
        {
            amp = NOTE_FINAL_AMP;
        }
        //freq = fbind*(NOTERANGE-1)/(nbins - 1);
        freq = (fbind % FIXBPERO) * (1 << SEMIBITSPERBIN);

        /*
        //      each leds color depends on value in bin. If want green lowest to yellow hightest use ROOT_NOTE_OFFSET = 110
                freq = bins[fbind];
                freq = (((int32_t)(freq))*NOTE_FINAL_AMP)>>MAX_AMP2_LOG2; // for PC 14;
                if( freq > NOTERANGE ) freq = NOTERANGE;
                freq = NOTERANGE - freq;
                amp = NOTE_FINAL_AMP;
        */
        uint32_t color = ECCtoAdjustedHEX( freq, NOTE_FINAL_SATURATION, amp );
        AssignColorledOut(color, i, 0x00, SYMMETRY_REPEAT, led_spacing_gap );
    }
} // end DFTInLights()

// pure pattern only brightness related to sound bass amplitude
void ICACHE_FLASH_ATTR PureRotatingLEDs()
{
    int16_t i;
    int16_t jshift; // int8_t jshift; caused instability especially for large no of LEDs
    int32_t led_arc_len;
    int16_t freq;
    uint32_t amp;
    //TODO These next two should be differnet gui parameters
    uint8_t shift_dist = COLORCHORD_SHIFT_DISTANCE & 0x3f;
    uint8_t led_spacing_gap = COLORCHORD_SHIFT_DISTANCE >> 6;

    freq = ColorCycle;
    //  uint32_t color = ECCtoAdjustedHEX( freq, NOTE_FINAL_SATURATION, NOTE_FINAL_AMP );

    // can have led_arc_len a fixed size or proportional to amp2
    amp = (octave_bins[0] + octave_bins[1]) * AMP_1_MULT / 16; // bass;
    amp = (((uint32_t)(amp)) * NOTE_FINAL_AMP) >> MAX_AMP2_LOG2; // for PC 14;
    if( amp > NOTE_FINAL_AMP )
    {
        amp = NOTE_FINAL_AMP;
    }
    led_arc_len = USE_NUM_LIN_LEDS;

    // now every COLORCHORD_SHIFT_INTERVAL th frame
    if (COLORCHORD_SHIFT_INTERVAL != 0 )
    {
        if ( gFRAMECOUNT_MOD_SHIFT_INTERVAL == 0 )
        {
            gROTATIONSHIFT += rot_dir * shift_dist;
            //printf("tna %d dfap dfa %d %d rot_dir %d, j shift %d\n",total_note_a, diff_flip_amount_prev,  diff_flip_amount, rot_dir, j);
            ColorCycle++;
            if (ColorCycle >= NOTERANGE)
            {
                ColorCycle = 0;
            }
            if (ColorCycle == 0 && COLORCHORD_FLIP_ON_PEAK)
            {
                rot_dir *= -1;
            }
        }
    }
    else
    {
        ColorCycle = 0;
        gROTATIONSHIFT = 0; // reset
    }

    jshift = ( gROTATIONSHIFT - led_arc_len / 2 ) % NUM_LIN_LEDS; // neg % pos is neg so fix
    if ( jshift < 0 )
    {
        jshift += NUM_LIN_LEDS;
    }
    memset( ledOut, 0, sizeof( ledOut ) );
    for( i = 0; i < led_arc_len; i++, jshift++ )
    {
        //      uint32_t color = ECCtoAdjustedHEX( (freq + i*NOTERANGE*NERF_NOTE_PORP/led_arc_len/100)%NOTERANGE, NOTE_FINAL_SATURATION, NOTE_FINAL_AMP );
        uint32_t color = ECCtoAdjustedHEX( (freq + i * NOTERANGE * NERF_NOTE_PORP / led_arc_len / 100) % NOTERANGE,
                                           NOTE_FINAL_SATURATION, amp );
        AssignColorledOut(color, jshift, 0x00, SYMMETRY_REPEAT, led_spacing_gap );
    }
} // end PureRotatingLEDs()


uint32_t ICACHE_FLASH_ATTR ECCtoAdjustedHEX( int16_t note, uint8_t sat, uint8_t val )
{
    uint8_t hue = 0;
    note = (note + ROOT_NOTE_OFFSET * NOTERANGE / 120) % NOTERANGE;
    uint32_t renote = ((uint32_t)note * 65535) / (NOTERANGE - 1);
#define rn0  0
#define hn0  298
#define rn1  21845
#define hn1  255
#define rn2  43690
#define hn2  170
// #define rn3  65535
// #define hn3  43
    if( renote < rn1 )
    {
        hue = hn0 - (renote - rn0) * (43) / (21845);
    }
    else if( renote < rn2 )
    {
        hue = hn1 - (renote - rn1) * (85) / (21845);
    }
    else
    {
        hue = hn2 - (renote - rn2) * (127) / (21846);
    }
    return EHSVtoHEX( hue, sat, val );
}


uint32_t ICACHE_FLASH_ATTR EHSVtoHEX( uint8_t hue, uint8_t sat, uint8_t val )
{
#define SIXTH1 43
#define SIXTH2 85
#define SIXTH3 128
#define SIXTH4 171
#define SIXTH5 213
    // using gamma = 2.2
    static const uint8_t gamma_correction_table[256] =
    {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2,
        3, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6,
        6, 7, 7, 7, 8, 8, 8, 9, 9, 9, 10, 10, 11, 11, 11, 12,
        12, 13, 13, 13, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18, 19, 19,
        20, 20, 21, 22, 22, 23, 23, 24, 25, 25, 26, 26, 27, 28, 28, 29,
        30, 30, 31, 32, 33, 33, 34, 35, 35, 36, 37, 38, 39, 39, 40, 41,
        42, 43, 43, 44, 45, 46, 47, 48, 49, 49, 50, 51, 52, 53, 54, 55,
        56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71,
        73, 74, 75, 76, 77, 78, 79, 81, 82, 83, 84, 85, 87, 88, 89, 90,
        91, 93, 94, 95, 97, 98, 99, 100, 102, 103, 105, 106, 107, 109, 110, 111,
        113, 114, 116, 117, 119, 120, 121, 123, 124, 126, 127, 129, 130, 132, 133, 135,
        137, 138, 140, 141, 143, 145, 146, 148, 149, 151, 153, 154, 156, 158, 159, 161,
        163, 165, 166, 168, 170, 172, 173, 175, 177, 179, 181, 182, 184, 186, 188, 190,
        192, 194, 196, 197, 199, 201, 203, 205, 207, 209, 211, 213, 215, 217, 219, 221,
        223, 225, 227, 229, 231, 234, 236, 238, 240, 242, 244, 246, 248, 251, 253, 255
    };
    uint16_t or = 0, og = 0, ob = 0;

    // move in rainbow order RYGCBM as hue from 0 to 255

    if( hue < SIXTH1 ) //Ok: Red->Yellow
    {
        or = 255;
        og = (hue * 255) / (SIXTH1);
    }
    else if( hue < SIXTH2 ) //Ok: Yellow->Green
    {
        og = 255;
        or = 255 - (hue - SIXTH1) * 255 / SIXTH1;
    }
    else if( hue < SIXTH3 )  //Ok: Green->Cyan
    {
        og = 255;
        ob = (hue - SIXTH2) * 255 / (SIXTH1);
    }
    else if( hue < SIXTH4 ) //Ok: Cyan->Blue
    {
        ob = 255;
        og = 255 - (hue - SIXTH3) * 255 / SIXTH1;
    }
    else if( hue < SIXTH5 ) //Ok: Blue->Magenta
    {
        ob = 255;
        or = (hue - SIXTH4) * 255 / SIXTH1;
    }
    else //Magenta->Red
    {
        or = 255;
        ob = 255 - (hue - SIXTH5) * 255 / SIXTH1;
    }

    uint16_t rv = val;
    if( rv > 128 )
    {
        rv++;
    }
    uint16_t rs = sat;
    if( rs > 128 )
    {
        rs++;
    }

    //or, og, ob range from 0...255 now.
    //Apply saturation giving OR..OB == 0..65025
    or = or * rs + 255 * (256 - rs);
    og = og * rs + 255 * (256 - rs);
    ob = ob * rs + 255 * (256 - rs);
    or >>= 8;
    og >>= 8;
    ob >>= 8;
    //back to or, og, ob range 0...255 now.
    //Need to apply saturation and value.
    or = ( or * val) >> 8;
    og = (og * val) >> 8;
    ob = (ob * val) >> 8;
    //  printf( "  hue = %d r=%d g=%d b=%d rs=%d rv=%d\n", hue, or, og, ob, rs, rv );

    or = gamma_correction_table[ or ];
    og = gamma_correction_table[og];
    ob = gamma_correction_table[ob];
    //  return or | (og<<8) | ((uint32_t)ob<<16);
    return og | ( or << 8) | ((uint32_t)ob << 16); //new
}

