//Copyright 2015 <>< Charles Lohr under the ColorChord License.
#include <osapi.h>
#include "embeddednf.h"
#include <stdio.h>
#include <string.h>
#ifndef PRECOMPUTE_FREQUENCY_TABLE
    #include <math.h>
#endif

uint16_t folded_bins[FIXBPERO];
uint16_t fuzzed_bins[FIXBINS];
#ifdef USE_EQUALIZER
    uint16_t max_bins[FIXBINS];
    uint32_t maxallbins;
#endif
int16_t  note_peak_freqs[MAXNOTES];
uint16_t note_peak_amps[MAXNOTES];
uint16_t note_peak_amps2[MAXNOTES];
uint8_t  note_jumped_to[MAXNOTES];
uint16_t octave_bins[OCTAVES];


/*
#ifndef PRECOMPUTE_FREQUENCY_TABLE
static const float bf_table[24] = {
        1.000000, 1.029302, 1.059463, 1.090508, 1.122462, 1.155353,
        1.189207, 1.224054, 1.259921, 1.296840, 1.334840, 1.373954,
        1.414214, 1.455653, 1.498307, 1.542211, 1.587401, 1.633915,
        1.681793, 1.731073, 1.781797, 1.834008, 1.887749, 1.943064 };

/ * The above table was generated using the following code:

#include <stdio.h>
#include <math.h>

int main()
{
    int i;
    #define FIXBPERO 24
    printf( "const float bf_table[%d] = {", FIXBPERO );
    for( i = 0; i < FIXBPERO; i++ )
    {
        if( ( i % 6 ) == 0 )
            printf( "\n\t" );
        printf( "%f, ", pow( 2, (float)i / (float)FIXBPERO ) );
    }
    printf( "};\n" );
    return 0;
}
*/

//#endif

void UpdateFreqs()
{
#ifndef PRECOMPUTE_FREQUENCY_TABLE
#define BUILD_BUG_ON(condition) ((void)sizeof(char[1 - 2*!!(condition)]))

    int i;
    uint16_t fbins[FIXBPERO];


    //Warning: This does floating point.  Avoid doing this frequently.  If you
    //absolutely cannot have floating point on your system, you may precompute
    //this and store it as a table.  It does preclude you from changing
    //BASE_FREQ in runtime.

    for( i = 0; i < FIXBPERO; i++ )
    {
        float frq = pow( 2, (float)i / (float)FIXBPERO) * BASE_FREQ;
        fbins[i] = ( 65536.0 ) / ( DFREQ ) * frq * (1 << (OCTAVES - 1)) + 0.5;
    }
#else
    //TODO this only for FIXBPERO = 24 or 12 could add for 6, 8, 16, 36,48
#define PCOMP( f )  (uint16_t)((65536.0)/(DFREQ) * (f * BASE_FREQ) * (1<<(OCTAVES-1)) + 0.5)
#if FIXBPERO == 24
    static const uint16_t fbins[FIXBPERO] =
    {
        PCOMP( 1.000000 ), PCOMP( 1.029302 ), PCOMP( 1.059463 ), PCOMP( 1.090508 ), PCOMP( 1.122462 ), PCOMP( 1.155353 ),
        PCOMP( 1.189207 ), PCOMP( 1.224054 ), PCOMP( 1.259921 ), PCOMP( 1.296840 ), PCOMP( 1.334840 ), PCOMP( 1.373954 ),
        PCOMP( 1.414214 ), PCOMP( 1.455653 ), PCOMP( 1.498307 ), PCOMP( 1.542211 ), PCOMP( 1.587401 ), PCOMP( 1.633915 ),
        PCOMP( 1.681793 ), PCOMP( 1.731073 ), PCOMP( 1.781797 ), PCOMP( 1.834008 ), PCOMP( 1.887749 ), PCOMP( 1.943064 )
    };
#elif FIXBPERO == 12
    static const uint16_t fbins[FIXBPERO] =
    {
        PCOMP( 1.000000 ), PCOMP( 1.059463 ), PCOMP( 1.122462 ),
        PCOMP( 1.189207 ), PCOMP( 1.259921 ), PCOMP( 1.334840 ),
        PCOMP( 1.414214 ), PCOMP( 1.498307 ), PCOMP( 1.587401 ),
        PCOMP( 1.681793 ), PCOMP( 1.781797 ), PCOMP( 1.887749 )
    };
#elif FIXBPERO == 36
    static const uint16_t fbins[FIXBPERO] =
    {
        PCOMP(  1.000000 ), PCOMP(  1.019441 ), PCOMP(  1.039259 ), PCOMP(  1.059463 ), PCOMP(  1.080060 ), PCOMP(  1.101057 ),
        PCOMP(  1.122462 ), PCOMP(  1.144283 ), PCOMP(  1.166529 ), PCOMP(  1.189207 ), PCOMP(  1.212326 ), PCOMP(  1.235894 ),
        PCOMP(  1.259921 ), PCOMP(  1.284415 ), PCOMP(  1.309385 ), PCOMP(  1.334840 ), PCOMP(  1.360790 ), PCOMP(  1.387245 ),
        PCOMP(  1.414214 ), PCOMP(  1.441707 ), PCOMP(  1.469734 ), PCOMP(  1.498307 ), PCOMP(  1.527435 ), PCOMP(  1.557129 ),
        PCOMP(  1.587401 ), PCOMP(  1.618261 ), PCOMP(  1.649721 ), PCOMP(  1.681793 ), PCOMP(  1.714488 ), PCOMP(  1.747819 ),
        PCOMP(  1.781797 ), PCOMP(  1.816437 ), PCOMP(  1.851749 ), PCOMP(  1.887749 ), PCOMP(  1.924448 ), PCOMP(  1.961860 )
    };
#elif FIXBPERO == 48
    static const uint16_t fbins[FIXBPERO] =
    {
        PCOMP(  1.000000 ), PCOMP(  1.014545 ), PCOMP(  1.029302 ), PCOMP(  1.044274 ), PCOMP(  1.059463 ), PCOMP(  1.074873 ),
        PCOMP(  1.090508 ), PCOMP(  1.106370 ), PCOMP(  1.122462 ), PCOMP(  1.138789 ), PCOMP(  1.155353 ), PCOMP(  1.172158 ),
        PCOMP(  1.189207 ), PCOMP(  1.206505 ), PCOMP(  1.224054 ), PCOMP(  1.241858 ), PCOMP(  1.259921 ), PCOMP(  1.278247 ),
        PCOMP(  1.296840 ), PCOMP(  1.315703 ), PCOMP(  1.334840 ), PCOMP(  1.354256 ), PCOMP(  1.373954 ), PCOMP(  1.393938 ),
        PCOMP(  1.414214 ), PCOMP(  1.434784 ), PCOMP(  1.455653 ), PCOMP(  1.476826 ), PCOMP(  1.498307 ), PCOMP(  1.520100 ),
        PCOMP(  1.542211 ), PCOMP(  1.564643 ), PCOMP(  1.587401 ), PCOMP(  1.610490 ), PCOMP(  1.633915 ), PCOMP(  1.657681 ),
        PCOMP(  1.681793 ), PCOMP(  1.706255 ), PCOMP(  1.731073 ), PCOMP(  1.756252 ), PCOMP(  1.781797 ), PCOMP(  1.807714 ),
        PCOMP(  1.834008 ), PCOMP(  1.860684 ), PCOMP(  1.887749 ), PCOMP(  1.915207 ), PCOMP(  1.943064 ), PCOMP(  1.971326 )
    };
#else
    BUILD_BUG_ON( 1 ); //forces compile error

#endif
#endif

#ifdef USE_32DFT
    UpdateBins32( fbins );
#else
    UpdateBinsForProgressiveIntegerSkippyInt( fbins );
#endif
#if DEBUGPRINT
    os_printf( "fbins: " );
    for( uint8_t i = 0; i < FIXBPERO; i++ )
    {
        os_printf( " %5d /", fbins[i]  );
    }
    os_printf( "\n" );
#endif
}

void ICACHE_FLASH_ATTR InitColorChord()
{
    //TODO Is this all needed? Only non-zero needs initialzation

    int i;
    //Set up and initialize
    for( i = 0; i < MAXNOTES; i++ )
    {
        note_peak_freqs[i] = -1;
        //      note_peak_amps[i] = 0;
        //      note_peak_amps2[i] = 0;
    }

#ifdef USE_EQUALIZER
    //max_bins[] initialized in CustomStart() find maximum
    maxallbins = 1;
    for( i = 0; i < FIXBINS; i++ )
    {
        if (max_bins[i] > maxallbins)
        {
            maxallbins = max_bins[i];
        }
    }
#endif

    //  memset( folded_bins, 0, sizeof( folded_bins ) );
    //  memset( fuzzed_bins, 0, sizeof( fuzzed_bins ) );
    //  memset( max_bins, 0, sizeof( max_bins ) );

    //Step 1: Initialize the Integer DFT.
#ifdef USE_32DFT
    SetupDFTProgressive32();
#else
    SetupDFTProgressiveIntegerSkippy();
#endif

    //Step 2: Set up the frequency list.  You could do this multiple times
    //if you want to change the loadout of the frequencies.
    UpdateFreqs();



}

void ICACHE_FLASH_ATTR HandleFrameInfo()
{
    int i, j, k;
    uint8_t hitnotes[MAXNOTES];
    memset( hitnotes, 0, sizeof( hitnotes ) );
#ifdef USE_EQUALIZER
    static uint16_t equalize_count;
#endif

#ifdef USE_32DFT
    uint16_t* strens;
    UpdateOutputBins32();
    strens = embeddedbins32;
#else
    uint16_t* strens = embeddedbins;
#endif
#if DFTHIST
    for( i = 0; i < FIXBINS; i++ )
    {
        os_printf( "%5d ", strens[i]  );
    }
    os_printf( "\n" );
#endif
#if DEBUGPRINT
    //#if 1
    uint16_t stmin;
    stmin = 65535;
    uint16_t stmax = 0;
    for( i = 0; i < FIXBINS; i++ )
    {
        if(strens[i] > stmax)
        {
            stmax = strens[i];
        }
        if(strens[i] < stmin)
        {
            stmin = strens[i];
        }
    }
    os_printf( "strens min: %d, max: %d\n ", stmin, stmax );
    //printf( "65535/maxbin = %f\n", 65535.0 / stmax );
    //  printf( "strens min: %d, max: %d\n ", stmin, stmax );
    //  printf( "strens oct 1: " );
    //  for( i = 0; i < FIXBPERO; i++ ) printf( " %5d /", strens[i]  );
    //  printf( "\n" );
#endif


#ifdef  USE_EQUALIZER
    //Copy out the bins from the DFT to our fuzzed bins.
    for( i = 0; i < FIXBINS; i++ )
    {
        fuzzed_bins[i] -= (fuzzed_bins[i] >> FUZZ_IIR_BITS); // once above 1<<FUZZ_IIR_BITS sticks at 1 less
        // Clip out small bins
        if (EQUALIZER_SET == 0)
        {
            //Equalize fuzzed_bins when not computing new max_bins
            if (strens[i] > LOWER_CUTOFF * 256)
            {
                fuzzed_bins[i] += (maxallbins * strens[i] / max_bins[i]) >> FUZZ_IIR_BITS;
            }
        }
        else
        {
            if (strens[i] > LOWER_CUTOFF * 256)
            {
                fuzzed_bins[i] += (strens[i] >> FUZZ_IIR_BITS);
            }
        }
    }

    if (EQUALIZER_SET || (equalize_count > 0))
        // Compute maximum freq response over the next seconds by sweeping sound over range of freq
    {
        if (equalize_count == 0)
        {
            for ( i = 0; i < FIXBINS; i++ )
            {
                max_bins[i] = 1;
            }
            maxallbins = 1;
            equalize_count = EQUALIZER_SET * 256;
        }
        else
        {
            for ( i = 0; i < FIXBINS; i++ )
            {
                if (fuzzed_bins[i] > max_bins[i])
                {
                    max_bins[i] = fuzzed_bins[i];
                }
                if (max_bins[i] > maxallbins)
                {
                    maxallbins = max_bins[i];
                }
            }
            equalize_count--;
            EQUALIZER_SET = 1 + equalize_count / 256;
            if (equalize_count == 0)
            {
                EQUALIZER_SET = 0;
            }
        }
    }
#else
    //Copy out the bins from the DFT to our fuzzed bins.
    for( i = 0; i < FIXBINS; i++ )
    {
        fuzzed_bins[i] -= (fuzzed_bins[i] >> FUZZ_IIR_BITS); // once above 1<<FUZZ_IIR_BITS sticks at 1 less
        // Clip out small bins
        if (strens[i] > LOWER_CUTOFF * 256)
        {
            fuzzed_bins[i] += (strens[i] >> FUZZ_IIR_BITS);
        }
    }
#endif
    /*
        //Taper first octave
        for( i = 0; i < FIXBPERO; i++ )
        {
            uint32_t taperamt = (65536 / FIXBPERO) * i;
            fuzzed_bins[i] = (taperamt * fuzzed_bins[i]) >> 16;
        }

        //Taper last octave
        for( i = 0; i < FIXBPERO; i++ )
        {
            int newi = FIXBINS - i - 1;
            uint32_t taperamt = (65536 / FIXBPERO) * i;
            fuzzed_bins[newi] = (taperamt * fuzzed_bins[newi]) >> 16;
        }
    */

#if FUZZHIST
    printf( "\n" );
    for( i = 0; i < FIXBINS; i++ )
    {
        printf( "%5d ", fuzzed_bins[i]  );
    }
    printf( "\n" );
#endif

    //Fold the bins from fuzzedbins into one octave.
    //  and collect bins from each octave
    for( i = 0; i < FIXBPERO; i++ )
    {
        folded_bins[i] = 0;
    }
    for( i = 0; i < OCTAVES; i++ )
    {
        octave_bins[i] = 0;
    }
    k = 0;
    for( j = 0; j < OCTAVES; j++ )
    {
        for( i = 0; i < FIXBPERO; i++ )
        {
            folded_bins[i] += fuzzed_bins[k];
            octave_bins[j] += fuzzed_bins[k++];
        }
    }

    //Now, we must blur the folded bins to get a good result.
    //Sometimes you may notice every other bin being out-of
    //line, and this fixes that.  We may consider running this
    //more than once, but in my experience, once is enough.
    for( j = 0; j < FILTER_BLUR_PASSES; j++ )
    {
        //Extra scoping because this is a large on-stack buffer.
        uint16_t folded_out[FIXBPERO];
        uint8_t adjLeft = FIXBPERO - 1;
        uint8_t adjRight = 1;
        for( i = 0; i < FIXBPERO; i++ )
        {
            uint16_t lbin = folded_bins[adjLeft] >> 2;
            uint16_t rbin = folded_bins[adjRight] >> 2;
            uint16_t tbin = folded_bins[i] >> 1;
            folded_out[i] = lbin + rbin + tbin;

            //We do this funny dance to avoid a modulus operation.  On some
            //processors, a modulus operation is slow.  This is cheap.
            adjLeft++;
            if( adjLeft >= FIXBPERO )
            {
                adjLeft = 0;
            }
            adjRight++;
            if( adjRight >= FIXBPERO )
            {
                adjRight = 0;
            }
        }

        for( i = 0; i < FIXBPERO; i++ )
        {
            folded_bins[i] = folded_out[i];
        }
    }
#if DEBUGPRINT
    //printf("MIN_AMP_FOR_NOTE %5d \n", MIN_AMP_FOR_NOTE);
    //printf( "Folded Bin << %2d >>4: ", FUZZ_IIR_BITS );
    //for( i = 0; i < FIXBPERO; i++ ) printf( " %5d /", (folded_bins[i]<<FUZZ_IIR_BITS)>>4  );
    //printf( "\n" );
    printf( "Folded Bin         : ");
    for( i = 0; i < FIXBPERO; i++ )
    {
        printf( " %5d /", folded_bins[i]  );
    }
    printf( "\n" );
#endif


    //Next, we have to find the peaks, this is what "decompose" does in our
    //normal tool.  As a warning, it expects that the values in foolded_bins
    //TODO this may be exceeded. DFT FUZZED  can range 0..65535, so maybe folded even higher
    //do NOT exceed 32767.
    // freq of note from 0..NOTERANGE-1 after interpolation
    //     initially 0, 1<<SEMIBITSPERBIN, 2<<SEMIBITSPERBIN, (FIXBPERO-1)<<SEMIBITSPERBIN
    {
        uint8_t adjLeft = FIXBPERO - 1;
        uint8_t adjRight = 1;
        for( i = 0; i < FIXBPERO; i++ )
        {
            int32_t prev = folded_bins[adjLeft];
            int32_t next = folded_bins[adjRight];
            int32_t this = folded_bins[i];
            int16_t thisfreq = i << SEMIBITSPERBIN;
            int16_t offset;
            adjLeft++;
            if( adjLeft >= FIXBPERO )
            {
                adjLeft = 0;
            }
            adjRight++;
            if( adjRight >= FIXBPERO )
            {
                adjRight = 0;
            }
            //folded_bins seem to range from 0 to 2^12 to get amp1 in 0..256 scale
            // Need to adjust this so in range 0..255 so can be compared to MIN_AMP_FOR_NOTE
            // remove test for too small, as even if too small may jump to an existing note
            //          if( this < MIN_AMP_FOR_NOTE<<8) continue;
            if( prev > this || next > this )
            {
                continue;
            }
            if( prev == this && next == this )
            {
                continue;
            }

            //i is at a peak...
#if DEBUGPRINT
            printf("peak at i = %5d of  %5d \n", i, this);
#endif
            //TODO needs rewrite finding peak. Really want to handle jump up, stay roughly level, ... , jump down
            //     NOT just one either side of this

            // Corrected way to linear adjust. If prev=next offset should be zero.
            // When prev=this>next should offset left 1/2 (of 1<<SEMIBITSPERBIN) (toward prev)
            //   and linearly to offset 0 at prev=next
            // When prev<this=next should offset right by 1/2 (of 1<<SEMIBITSPERBIN) (toward next)
            // note code this replaces was not linear
            if( next < prev ) //Closer to prev.
            {
                offset = -( ((prev - next) << 15) / (this - next));
            }
            else
            {
                offset = (((next - prev) << 15) / (this - prev));
            }

            //Round multiply by 1<<SEMIBITSPERBIN and shift back to correct range and adjust thisfreq
            thisfreq += (offset + (1 << (15 - SEMIBITSPERBIN))) >> (16 - SEMIBITSPERBIN);

            //In the event we went 'below zero' need to wrap to the top.
            if( thisfreq < 0)
            {
                thisfreq += NOTERANGE;
            }
            //Okay, we have a peak, and a frequency. Now, we need to search
            //through the existing notes to see if we have any matches.
            //If we have a note that's close enough, we will try to pull it
            //closer to us and boost it.
            int8_t lowest_found_free_note = -1;
            int8_t closest_note_id = -1;
            int16_t closest_note_distance = 32767;

            for( j = 0; j < MAXNOTES; j++ )
            {
                int16_t nf = note_peak_freqs[j];

                if( nf < 0 )
                {
                    if( lowest_found_free_note == -1 )
                    {
                        lowest_found_free_note = j;
                    }
                    continue;
                }

                int16_t distance = thisfreq - nf;

                if( distance < 0 )
                {
                    distance = -distance;
                }

                //Make sure that if we've wrapped around the right side of the
                //array, we can detect it and loop it back.
                if( distance << 1 > NOTERANGE )
                {
                    distance = NOTERANGE - distance;
                }

                //TODO minor bug - if SEMIBITSPERBIN is lowered while music playing thus decreasing NOTERANGE
                //  distance can be computed as negative by above, thus the note will be marked as desired and
                //  absorb all other notes till it disappears

                //If we find a note closer to where we are than any of the
                //others, we can mark it as our desired note.
                if( distance < closest_note_distance )
                {
                    closest_note_id = j;
                    closest_note_distance = distance;
                }
            }

            int8_t marked_note = -1;
            uint8_t is_a_new_note = 1;
            //MAX_JUMP_DISTANCE is in range 0..255 while distance is in range 0..floor(NOTERANGE/2)
            // so compare distance/floor(NOTERANGE/2) to MAX_JUMP_DISTANCE/255
            if( closest_note_distance * 255 <= NOTERANGE / 2 * MAX_JUMP_DISTANCE )
            {
                //We found the note we need to augment.
                // do not change freq but keep the augmented notes freqence
                //              note_peak_freqs[closest_note_id] = thisfreq;
                marked_note = closest_note_id;
                is_a_new_note = 0;
            }

            //The note was not found.
            else if( lowest_found_free_note != -1 )
            {
                note_peak_freqs[lowest_found_free_note] = thisfreq;
                marked_note = lowest_found_free_note;
            }

            //If we found a note to attach to, we have to use the IIR to
            //increase the strength of the note, but we can't exactly snap
            //it to the new strength.
            if( marked_note != -1 )
            {
                hitnotes[marked_note] = 1;
                int32_t newpeakamp1 = (this * AMP_1_MULT) >> 4;
                if (newpeakamp1 > 65535)
                {
                    newpeakamp1 = 65535;
                }
                if (newpeakamp1 > note_peak_amps[marked_note])
                {
                    note_peak_amps[marked_note] -=
                        (note_peak_amps[marked_note] >> AMP1_ATTACK_BITS) -
                        (newpeakamp1 >> AMP1_ATTACK_BITS);
                }
                else
                {
                    note_peak_amps[marked_note] -=
                        (note_peak_amps[marked_note] >> AMP1_DECAY_BITS) -
                        (newpeakamp1 >> AMP1_DECAY_BITS);
                }

                int32_t newpeakamp2 = (this * AMP_2_MULT) >> 4;
                if (newpeakamp2 > 65535)
                {
                    newpeakamp2 = 65535;
                }
                if (newpeakamp2 > note_peak_amps2[marked_note])
                {
                    note_peak_amps2[marked_note] -=
                        (note_peak_amps2[marked_note] >> AMP2_ATTACK_BITS) -
                        (newpeakamp2 >> AMP2_ATTACK_BITS);
                }
                else
                {
                    note_peak_amps2[marked_note] -=
                        (note_peak_amps2[marked_note] >> AMP2_DECAY_BITS) -
                        (newpeakamp2 >> AMP2_DECAY_BITS);
                }
                if( is_a_new_note && (newpeakamp1 < MIN_AMP_FOR_NOTE << 8) ) //kill it
                {
                    note_peak_freqs[marked_note] = -1;
                    note_peak_amps[marked_note] = 0;
                    note_peak_amps2[marked_note] = 0;
                    note_jumped_to[marked_note] = 0;
                }


            }
        }
    }

#if 0
    for( i = 0; i < MAXNOTES; i++ )
    {
        if( note_peak_freqs[i] < 0 )
        {
            continue;
        }
        printf( "%d / ", note_peak_amps[i] );
    }
    printf( "\n" );
#endif

    //Now we need to handle combining notes.
    //TODO need major rethink here. As is highly depending on order. When two notes combine
    // They get an interpolated frequency which then might be 'close' to other notes etc.
    if (MAX_COMBINE_DISTANCE > 0)   // Only do this looping if potential to combine!
    {
        for( i = 0; i < MAXNOTES; i++ )
            for( j = 0; j < i; j++ )
            {
                //We'd be combining nf2 (j) into nf1 (i) if they're close enough.
                int16_t nf1 = note_peak_freqs[i];
                int16_t nf2 = note_peak_freqs[j];
                int16_t distance = nf1 - nf2;

                if( nf1 < 0 || nf2 < 0 )
                {
                    continue;
                }

                if( distance < 0 )
                {
                    distance = -distance;
                }

                //If it wraps around above the halfway point, then we're closer to it
                //on the other side.
                if( distance << 1 > NOTERANGE )
                {
                    distance = NOTERANGE - distance;
                }

                if( distance * 255 > NOTERANGE / 2 * MAX_COMBINE_DISTANCE )
                {
                    continue;
                }

                int into;
                int from;

                if( note_peak_amps[i] > note_peak_amps[j] )
                {
                    into = i;
                    from = j;
                }
                else
                {
                    into = j;
                    from = i;
                }

                //We need to combine the notes.  We need to move the new note freq
                //towards the stronger of the two notes.
                int16_t amp1 = note_peak_amps[into];
                int16_t amp2 = note_peak_amps[from];

                //0 to 32768 porportional to how much of amp1 we want.
                uint32_t porp = (amp1 << 15) / (amp1 + amp2);
                int16_t newnote = (nf1 * porp + nf2 * (32768 - porp)) >> 15;

                //When combining notes, we have to use the amplitudes of into which has strongest amps
                //trying to average or combine the power of the notes looks awful.
                note_peak_freqs[into] = newnote;
                note_peak_freqs[from] = -1;
                note_peak_amps[from] = 0;
                note_jumped_to[from] = into + 1; // using 0 to mean unassigned, so lable from 1,..., MAXNOTES
            } // end combine notes
    } // end if testing if should combine

    //For all of the notes that have not been hit, we have to allow them to
    //to decay.  We only do this for notes that have not found a peak.
    for( i = 0; i < MAXNOTES; i++ )
    {
        if( note_peak_freqs[i] < 0 || hitnotes[i] )
        {
            continue;
        }
        note_peak_amps[i] -= note_peak_amps[i] >> AMP1_DECAY_BITS;
        note_peak_amps2[i] -= note_peak_amps2[i] >> AMP2_DECAY_BITS;
    }

    for( i = 0; i < MAXNOTES; i++ )
    {
        //In the event a note is not strong enough anymore, it is to be
        //returned back into the great pool of unused notes.
        if( note_peak_amps[i] < MINIMUM_AMP_FOR_NOTE_TO_DISAPPEAR << 8 )
        {
            note_peak_freqs[i] = -1;
            note_peak_amps[i] = 0;
            note_peak_amps2[i] = 0;
            note_jumped_to[i] = 0;
        }
    }

    //We now have notes!!!
#if SHOWNOTES == 1
    int hadnotes = 0;
    for( i = 0; i < MAXNOTES; i++ )
    {
        if( note_peak_freqs[i] < 0 )
        {
            continue;
        }
        printf( "(%3d %4d %4d) ", note_peak_freqs[i], note_peak_amps[i], note_peak_amps2[i] );
        hadnotes = 1;
    }
    if (hadnotes)
    {
        printf( "\n");
    }
    else
    {
        printf("no notes\n");
    }
#endif

#if FOLDHIST == 1
    for( i = 0; i < FIXBPERO; i++ )
    {
        printf( "%4d ", folded_bins[i] );
    }
    printf( "\n" );
#endif


}


