//Copyright 2015 <>< Charles Lohr under the ColorChord License.
#include <osapi.h>
#include "DFT32.h"
#include <string.h>

#ifndef CCEMBEDDED
    #include <stdlib.h>
    #include <stdio.h>
    #include <math.h>
    static float* goutbins;
#endif
#include <stdio.h>
uint16_t embeddedbins32[FIXBINS];

//NOTES to self:
//
// Let's say we want to try this on an AVR.
//  24 bins, 5 octaves = 120 bins.
// 20 MHz clock / 4.8k sps = 4096 IPS = 34 clocks per bin = :(
//  We can do two at the same time, this frees us up some

static uint8_t Sdonefirstrun;

//A table of precomputed sin() values.  Ranging -1500 to +1500
//If we increase this, it may cause overflows elsewhere in code.
const int16_t Ssinonlytable[256] =
{
    0,    36,    73,   110,   147,   183,   220,   256,
    292,   328,   364,   400,   435,   470,   505,   539,
    574,   607,   641,   674,   707,   739,   771,   802,
    833,   863,   893,   922,   951,   979,  1007,  1034,
    1060,  1086,  1111,  1135,  1159,  1182,  1204,  1226,
    1247,  1267,  1286,  1305,  1322,  1339,  1355,  1371,
    1385,  1399,  1412,  1424,  1435,  1445,  1455,  1463,
    1471,  1477,  1483,  1488,  1492,  1495,  1498,  1499,
    1500,  1499,  1498,  1495,  1492,  1488,  1483,  1477,
    1471,  1463,  1455,  1445,  1435,  1424,  1412,  1399,
    1385,  1371,  1356,  1339,  1322,  1305,  1286,  1267,
    1247,  1226,  1204,  1182,  1159,  1135,  1111,  1086,
    1060,  1034,  1007,   979,   951,   922,   893,   863,
    833,   802,   771,   739,   707,   674,   641,   607,
    574,   539,   505,   470,   435,   400,   364,   328,
    292,   256,   220,   183,   147,   110,    73,    36,
    0,   -36,   -73,  -110,  -146,  -183,  -219,  -256,
    -292,  -328,  -364,  -399,  -435,  -470,  -505,  -539,
    -573,  -607,  -641,  -674,  -706,  -739,  -771,  -802,
    -833,  -863,  -893,  -922,  -951,  -979, -1007, -1034,
    -1060, -1086, -1111, -1135, -1159, -1182, -1204, -1226,
    -1247, -1267, -1286, -1305, -1322, -1339, -1355, -1371,
    -1385, -1399, -1412, -1424, -1435, -1445, -1454, -1463,
    -1471, -1477, -1483, -1488, -1492, -1495, -1498, -1499,
    -1500, -1499, -1498, -1495, -1492, -1488, -1483, -1477,
    -1471, -1463, -1455, -1445, -1435, -1424, -1412, -1399,
    -1385, -1371, -1356, -1339, -1322, -1305, -1286, -1267,
    -1247, -1226, -1204, -1182, -1159, -1135, -1111, -1086,
    -1060, -1034, -1007,  -979,  -951,  -923,  -893,  -863,
    -833,  -802,  -771,  -739,  -707,  -674,  -641,  -608,
    -574,  -540,  -505,  -470,  -435,  -400,  -364,  -328,
    -292,  -256,  -220,  -183,  -147,  -110,   -73,   -37,
};


/** The above table was created using the following code:
#include <math.h>
#include <stdio.h>
#include <stdint.h>

int16_t Ssintable[256]; //Actually, just [sin].

int main()
{
    int i;
    for( i = 0; i < 256; i++ )
    {
        Ssintable[i] = (int16_t)((sinf( i / 256.0 * 6.283 ) * 1500.0));
    }

    printf( "const int16_t Ssinonlytable[256] = {" );
    for( i = 0; i < 256; i++ )
    {
        if( !(i & 0x7 ) )
        {
            printf( "\n\t" );
        }
        printf( "%6d," ,Ssintable[i] );
    }
    printf( "};\n" );
} */



uint16_t Sdatspace32A[FIXBINS * 2]; //(advances,places) full revolution is 256. 8bits integer part 8bit fractional
int32_t Sdatspace32B[FIXBINS * 2]; //(isses,icses)

//This is updated every time the DFT hits the octavecount, or 1 out of DFT_UPDATE*(1<<OCTAVES) times
//               which is DFT_UPDATE*(1<<(OCTAVES-1)) samples
int32_t Sdatspace32BOut[FIXBINS * 2]; //(isses,icses)

//Sdo_this_octave is a scheduling state for the running SIN/COS states for
//each bin.  We have to execute the highest octave every time, however, we can
//get away with updating the next octave down every-other-time, then the next
//one down yet, every-other-time from that one.  That way, no matter how many
//octaves we have, we only need to update FIXBPERO*2 DFT bins.
static uint8_t Sdo_this_octave[BINCYCLE];

static int32_t Saccum_octavebins[OCTAVES];
static uint8_t Swhichoctaveplace;
static uint8_t UpdateCount = 0;


//
uint16_t embeddedbins[FIXBINS];

//From: http://stackoverflow.com/questions/1100090/looking-for-an-efficient-integer-square-root-algorithm-for-arm-thumb2
//  for sqrt approx but also suggestion for quick norm approximation that would work in this DFT

#if APPROXNORM != 1
/**
 * \brief    Fast Square root algorithm, with rounding
 *
 * This does arithmetic rounding of the result. That is, if the real answer
 * would have a fractional part of 0.5 or greater, the result is rounded up to
 * the next integer.
 *      - SquareRootRounded(2) --> 1
 *      - SquareRootRounded(3) --> 2
 *      - SquareRootRounded(4) --> 2
 *      - SquareRootRounded(6) --> 2
 *      - SquareRootRounded(7) --> 3
 *      - SquareRootRounded(8) --> 3
 *      - SquareRootRounded(9) --> 3
 *
 * \param[in] a_nInput - unsigned integer for which to find the square root
 *
 * \return Integer square root of the input value.
 */
static uint16_t SquareRootRounded(uint32_t a_nInput)
{
    uint32_t op  = a_nInput;
    uint32_t res = 0;
    uint32_t one = 1uL << 30; // The second-to-top bit is set: use 1u << 14 for uint16_t type; use 1uL<<30 for uint32_t type


    // "one" starts at the highest power of four <= than the argument.
    while (one > op)
    {
        one >>= 2;
    }

    while (one != 0)
    {
        if (op >= res + one)
        {
            op = op - (res + one);
            res = res +  2 * one;
        }
        res >>= 1;
        one >>= 2;
    }

    /* Do arithmetic rounding to nearest integer */
    if (op > res)
    {
        res++;
    }

    return res;
}
#endif

void ICACHE_FLASH_ATTR UpdateOutputBins32()
{
    int i;
    int32_t* ipt = &Sdatspace32BOut[0];
#if SHOWSAMP
    os_printf("--> Update Output Bins \n");
#endif
    // empirical log_2 of multiplier to adjust
    //        static const uint16_t adjrmuxbits[17] = {
    //      6, 5, 4, 4, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    //       static const uint16_t adjstrens[17] = {
    //      642, 394, 248, 141, 76, 40, 23, 15, 11, 10, 9, 8, 8, 8, 8, 8, 8};
    //        static const uint16_t adjstrens[17] = {
    //      1.25, 1.54, 1.94, 1.1, 1.19, 1.25, 1.44, 1.88, 1.38, 1.25, 1.13, 1, 1, 1, 1, 1, 1};
    for( i = 0; i < FIXBINS; i++ )
    {
#if APPROXNORM == 1
        int32_t isps = *(ipt++); //can keep 32 bits as no need to square
        int32_t ispc = *(ipt++);
#else
        int16_t isps = *(ipt++) >> 16; //might loose some precsion with this
        int16_t ispc = *(ipt++) >> 16;
#endif
        //If we are running DFT32 on regular ColorChord, then we will need to
        //also update goutbins[]... But if we're on embedded systems, we only
        //update embeddedbins32.
#ifndef CCEMBEDDED
        uint32_t mux = ( (isps) * (isps)) + ((ispc) * (ispc));
        goutbins[i] = sqrtf( (float)mux );
        //reasonable (but arbitrary attenuation)
        //goutbins[i] /= (78<<DFTIIR)*(1<<octave); is the same as
        goutbins[i] /= (78 << DFTIIR) * (1 << (i / FIXBPERO));
#endif

#if APPROXNORM == 1
        isps = isps < 0 ? -isps : isps;
        ispc = ispc < 0 ? -ispc : ispc;
        uint32_t rmux = isps > ispc ? isps + (ispc >> 1) : ispc + (isps >> 1);
#else
        uint32_t rmux = ( (isps) * (isps)) + ((ispc) * (ispc));
        rmux = SquareRootRounded( rmux );
        rmux = rmux << 16;
#endif
        //adjrmuxbits is empirical adjust embeddedbins32 via a logistic data so between 0 and 65536
#if ADJUST_DFT_WITH_OCTAVE == 1 && PROGRESSIVE_DFT == 1
        //uint8_t rmuxshift = RMUXSHIFT - adjrmuxbits[DFTIIR] + octave;
        //uint8_t rmuxshift = RMUXSHIFT + DFTIIR + octave; same as
        uint8_t rmuxshift = RMUXSHIFT + DFTIIR + (i / FIXBPERO);
#else
        //No adjustment using octave may be too noisy in high octaves
        //   also get jumps at octave boundarys giving false peaks
        //uint8_t rmuxshift = RMUXSHIFTSTART - adjrmuxbits[DFTIIR];
        uint8_t rmuxshift = RMUXSHIFT + DFTIIR;
#endif
        rmux = rmux << 2; // make a bit bigger
        rmux = rmux >> rmuxshift; // *adjstrens[DFTIIR] // if had floating point could refine further
        rmux = rmux / DFT_UPDATE;

#if CHECKOVERFLOW
        if (rmux > 65535)
        {
            rmux = 65535;
            os_printf("Clipping to prevent overflow embeddedbins\n" );
        }
#endif
        embeddedbins32[i] = rmux;
    }
}

#if PROGRESSIVE_DFT
static void ICACHE_FLASH_ATTR HandleInt( int16_t sample )
{
    int i;
    uint16_t adv;
    uint8_t localipl;
    int16_t filteredsample;

    uint8_t oct = Sdo_this_octave[Swhichoctaveplace];
    Swhichoctaveplace ++;
    Swhichoctaveplace &= BINCYCLE - 1;

    for( i = 0; i < OCTAVES; i++ )
    {
        Saccum_octavebins[i] += sample;
    }

    if( oct > 128 )
    {
        //Special: This is when we can update everything.
        //This gets run once out of every DFT_UPDATE*(1<<OCTAVES) times.
        // which is half as many samples
        //It handles updating part of the DFT.
        //It should happen at the very first call to HandleInit
        int32_t* bins = &Sdatspace32B[0];
        int32_t* binsOut = &Sdatspace32BOut[0];
        UpdateCount++;
        if (UpdateCount >= DFT_UPDATE)
        {
            UpdateCount = 0;
#if SHOWSAMP
            printf(" update binsOut and lower bins\noct %d: ", SHOWSAMP - 1 );
#endif
            for( i = 0; i < FIXBINS; i++ )
            {
                //First for the SIN then the COS.
                int32_t val = *(bins);
                *(binsOut++) = val;
                *(bins++) -= val >> DFTIIR;

                val = *(bins);
                *(binsOut++) = val;
                *(bins++) -= val >> DFTIIR;
            }
#if SHOWSAMP
        }
        else
        {
            printf("\noct %d: ", SHOWSAMP - 1);
#endif
        }
        return;
    }

    // process a filtered sample for one of the octaves
    uint16_t* dsA = &Sdatspace32A[oct * FIXBPERO * 2];
    int32_t* dsB = &Sdatspace32B[oct * FIXBPERO * 2];

    filteredsample = Saccum_octavebins[oct] >> (OCTAVES - oct);
    Saccum_octavebins[oct] = 0;
#if SHOWSAMP
    if (oct == SHOWSAMP - 1)
    {
        printf("%d ", filteredsample);
    }
    //printf("%d (%d) ", filteredsample, oct);
#endif
    for( i = 0; i < FIXBPERO; i++ )
    {
        adv = *(dsA++);
        localipl = *(dsA) >> 8;
        *(dsA++) += adv;
        *(dsB) += (Ssinonlytable[localipl] * filteredsample);
#if CHECKOVERFLOW
        if ((*(dsB) >> 16) > 65535)
        {
            os_printf("Overflow potential\n" );
        }
#endif
        dsB++;
        //Get the cosine (1/4 wavelength out-of-phase with sin)
        localipl += 64;
        *(dsB++) += (Ssinonlytable[localipl] * filteredsample);
    }
}

int ICACHE_FLASH_ATTR SetupDFTProgressive32()
{
    int i;
    int j;

    Sdonefirstrun = 1;
    Sdo_this_octave[0] = 0xff;
    for( i = 0; i < BINCYCLE - 1; i++ )
    {
        // Sdo_this_octave =
        // 255 4 3 4 2 4 3 4 1 4 3 4 2 4 3 4 0 4 3 4 2 4 3 4 1 4 3 4 2 4 3 4 is case for 5 octaves.
        // Initial state is special one, then at step i do octave = Sdo_this_octave with averaged samples from last update of that octave
        //search for "first" zero

        for( j = 0; j <= OCTAVES; j++ )
        {
            if( ((1 << j) & i) == 0 )
            {
                break;
            }
        }
        if( j > OCTAVES )
        {
#ifndef CCEMBEDDED
            fprintf( stderr, "Error: algorithm fault.\n" );
            exit( -1 );
#endif
            return -1;
        }
        Sdo_this_octave[i + 1] = OCTAVES - j - 1;
    }
    return 0;
}

#else
// Here is simple DFT on all bins
static void ICACHE_FLASH_ATTR HandleInt( int16_t sample )
{
    int i;
    uint16_t adv;
    uint8_t localipl;
    int32_t val;
    uint16_t* dsA;
    int32_t* bins;
    int32_t* binsOut;

    // process sample for all bins
    //  and every DFT_UPDATE times update binsOut and attenuate bins via DFTIIR
    UpdateCount++;
    dsA = &Sdatspace32A[0];
    bins = &Sdatspace32B[0];


    if (UpdateCount >= DFT_UPDATE)
    {
        binsOut = &Sdatspace32BOut[0];
    }
#if SHOWSAMP
    os_printf("%d ", sample);
#endif
    for( i = 0; i < FIXBINS; i++ )
    {
        adv = *(dsA++);
        localipl = *(dsA) >> 8;
        *(dsA++) += adv;
        val = *(bins);
        if (UpdateCount >= DFT_UPDATE)
        {
            *(binsOut++) = val;
            val -= val >> DFTIIR;
        }
#if CHECKOVERFLOW
        *(bins) = val + (Ssinonlytable[localipl] * sample);
        if ((*(bins) >> 16) > 65535)
        {
            //fprintf( stderr, "Overflow potential\n" );
            os_printf("Overflow potential\n" );
        }
        bins++;
#else
        *(bins++) = val + (Ssinonlytable[localipl] * sample);
#endif
        //Get the cosine (1/4 wavelength out-of-phase with sin)
        localipl += 64;
        val = *(bins);
        if (UpdateCount >= DFT_UPDATE)
        {
            *(binsOut++) = val;
            val -= val >> DFTIIR;
        }
        *(bins++) = val + (Ssinonlytable[localipl] * sample);
    }
    if (UpdateCount >= DFT_UPDATE)
    {
        UpdateCount = 0;
    }
}

int ICACHE_FLASH_ATTR SetupDFTProgressive32()
{
    return 0;
}

#endif

#if PROGRESSIVE_DFT
void ICACHE_FLASH_ATTR UpdateBins32( const uint16_t* frequencies )
{
    int i;
    int imod = 0;
    for( i = 0; i < FIXBINS; i++, imod++ )
    {
        if (imod >= FIXBPERO)
        {
            imod = 0;
        }
        uint16_t freq = frequencies[imod];
        Sdatspace32A[i * 2] = freq;
    }
}
#else
void ICACHE_FLASH_ATTR UpdateBins32( const uint16_t* frequencies )
{
    int i;
    int imod = 0;
    int oct = 0;
    for( i = 0; i < FIXBINS; i++, imod++ )
    {
        if (imod >= FIXBPERO)
        {
            imod = 0;
            oct += 1;
        }
        uint16_t freq = frequencies[imod];
        Sdatspace32A[i * 2] = freq >> (OCTAVES - 1 - oct);
    }
}
#endif


void ICACHE_FLASH_ATTR PushSample32( int16_t dat )
{
#if DFTSAMPLE
#include <math.h>;
    //Sends out blocks of incoming data starting at an
    //zerocrossing that is non-decreasing - so plotting will look nice
    // to be used with embeddedlinux when testing
    static int cnt = 0;
    static olddat;
    static zerocrossing;
    zerocrossing = (abs(dat) < 100) ? 1 : 0 ;
    if ((cnt == 0 && zerocrossing && (dat >= olddat)) || cnt > 0)
    {
        printf("%d ", dat);
        cnt++;
        if (cnt > 128)
        {
            printf("\n");
            cnt = 0;
        }
    }
    olddat = dat;
#endif
    HandleInt( dat );
#if PROGRESSIVE_DFT
    HandleInt( dat );
#endif
}


#ifndef CCEMBEDDED

void ICACHE_FLASH_ATTR UpdateBinsForDFT32( const float* frequencies )
{
    int i;
    for( i = 0; i < FIXBINS; i++ )
    {
        float freq = frequencies[(i % FIXBPERO) + (FIXBPERO * (OCTAVES - 1))];
        Sdatspace32A[i * 2] = (65536.0 / freq); // / oneoveroctave;
    }
}

#endif


#ifndef CCEMBEDDED

void ICACHE_FLASH_ATTR DoDFTProgressive32( float* outbins, float* frequencies, int bins, const float* databuffer,
        int place_in_data_buffer, int size_of_data_buffer, float q, float speedup )
{
    static float backupbins[FIXBINS];
    int i;
    static int last_place;

    memset( outbins, 0, bins * sizeof( float ) );
    goutbins = outbins;

    memcpy( outbins, backupbins, FIXBINS * 4 );

    if( FIXBINS != bins )
    {
        fprintf( stderr, "Error: Bins was reconfigured.  skippy requires a constant number of bins (%d != %d).\n", FIXBINS,
                 bins );
        return;
    }

    if( !Sdonefirstrun )
    {
        SetupDFTProgressive32();
        Sdonefirstrun = 1;
    }

    UpdateBinsForDFT32( frequencies );

    for( i = last_place; i != place_in_data_buffer; i = (i + 1) % size_of_data_buffer )
    {
        int16_t ifr1 = (int16_t)( ((databuffer[i]) ) * 4095 );
        HandleInt( ifr1 );
        HandleInt( ifr1 );
    }

    UpdateOutputBins32();

    last_place = place_in_data_buffer;

    memcpy( backupbins, outbins, FIXBINS * 4 );
}

#endif




