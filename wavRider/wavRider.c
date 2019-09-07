#include "wavRider.h"
#include "tinywav.h"
#include "../firmware/user/buzzer.h"

#define NUM_CHANNELS 1
#define SAMPLE_RATE 48000

#define CLOCK_NUMERATOR 5000000

const song_t MetalGear = {
    .notes = {
        {.note = E_4, .timeMs = 479},
        {.note = SILENCE, .timeMs = 1},
        {.note = D_4, .timeMs = 479},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_4, .timeMs = 959},
        {.note = SILENCE, .timeMs = 1},
        {.note = D_4, .timeMs = 239},
        {.note = SILENCE, .timeMs = 1},
        {.note = E_4, .timeMs = 239},
        {.note = SILENCE, .timeMs = 1},
        {.note = A_3, .timeMs = 239},
        {.note = SILENCE, .timeMs = 1},
        {.note = E_4, .timeMs = 479},
        {.note = SILENCE, .timeMs = 1},
        {.note = D_4, .timeMs = 959},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_4, .timeMs = 239},
        {.note = SILENCE, .timeMs = 1},
        {.note = D_4, .timeMs = 239},
        {.note = SILENCE, .timeMs = 1},
        {.note = E_4, .timeMs = 719},
        {.note = SILENCE, .timeMs = 1},
        {.note = A_4, .timeMs = 239},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_4, .timeMs = 239},
        {.note = SILENCE, .timeMs = 1},
        {.note = E_4, .timeMs = 239},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_4, .timeMs = 479},
        {.note = SILENCE, .timeMs = 1},
        {.note = D_4, .timeMs = 959},
        {.note = SILENCE, .timeMs = 1},
        {.note = E_4, .timeMs = 239},
        {.note = SILENCE, .timeMs = 1},
        {.note = A_4, .timeMs = 239},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_5, .timeMs = 959},
        {.note = SILENCE, .timeMs = 1},
        {.note = B_4, .timeMs = 239},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_5, .timeMs = 239},
        {.note = SILENCE, .timeMs = 1},
        {.note = D_5, .timeMs = 239},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_5, .timeMs = 479},
        {.note = SILENCE, .timeMs = 1},
        {.note = A_4, .timeMs = 959},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_4, .timeMs = 239},
        {.note = SILENCE, .timeMs = 1},
        {.note = A_4, .timeMs = 239},
        {.note = SILENCE, .timeMs = 1},
        {.note = B_4, .timeMs = 719},
        {.note = SILENCE, .timeMs = 1},
        {.note = C_5, .timeMs = 239},
        {.note = SILENCE, .timeMs = 1},
        {.note = B_4, .timeMs = 479},
        {.note = SILENCE, .timeMs = 1},
        {.note = A_4, .timeMs = 239},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_4, .timeMs = 239},
        {.note = SILENCE, .timeMs = 1},
        {.note = A_4, .timeMs = 1919},
        {.note = SILENCE, .timeMs = 1},
        {.note = B_4, .timeMs = 479},
        {.note = SILENCE, .timeMs = 1},
        {.note = A_4, .timeMs = 479},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_4, .timeMs = 959},
        {.note = SILENCE, .timeMs = 1},
        {.note = A_4, .timeMs = 239},
        {.note = SILENCE, .timeMs = 1},
        {.note = B_4, .timeMs = 239},
        {.note = SILENCE, .timeMs = 1},
        {.note = E_4, .timeMs = 239},
        {.note = SILENCE, .timeMs = 1},
        {.note = B_4, .timeMs = 479},
        {.note = SILENCE, .timeMs = 1},
        {.note = A_4, .timeMs = 959},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_4, .timeMs = 239},
        {.note = SILENCE, .timeMs = 1},
        {.note = A_4, .timeMs = 239},
        {.note = SILENCE, .timeMs = 1},
        {.note = B_4, .timeMs = 719},
        {.note = SILENCE, .timeMs = 1},
        {.note = E_5, .timeMs = 239},
        {.note = SILENCE, .timeMs = 1},
        {.note = D_5, .timeMs = 239},
        {.note = SILENCE, .timeMs = 1},
        {.note = B_4, .timeMs = 239},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_4, .timeMs = 479},
        {.note = SILENCE, .timeMs = 1},
        {.note = A_4, .timeMs = 959},
        {.note = SILENCE, .timeMs = 1},
        {.note = B_4, .timeMs = 239},
        {.note = SILENCE, .timeMs = 1},
        {.note = E_5, .timeMs = 239},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_5, .timeMs = 959},
        {.note = SILENCE, .timeMs = 1},
        {.note = F_SHARP_5, .timeMs = 239},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_5, .timeMs = 239},
        {.note = SILENCE, .timeMs = 1},
        {.note = A_5, .timeMs = 239},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_5, .timeMs = 479},
        {.note = SILENCE, .timeMs = 1},
        {.note = E_5, .timeMs = 959},
        {.note = SILENCE, .timeMs = 1},
        {.note = D_5, .timeMs = 239},
        {.note = SILENCE, .timeMs = 1},
        {.note = E_5, .timeMs = 239},
        {.note = SILENCE, .timeMs = 1},
        {.note = F_SHARP_5, .timeMs = 719},
        {.note = SILENCE, .timeMs = 1},
        {.note = G_5, .timeMs = 239},
        {.note = SILENCE, .timeMs = 1},
        {.note = F_SHARP_5, .timeMs = 479},
        {.note = SILENCE, .timeMs = 1},
        {.note = E_5, .timeMs = 239},
        {.note = SILENCE, .timeMs = 1},
        {.note = D_5, .timeMs = 239},
        {.note = SILENCE, .timeMs = 1},
        {.note = E_5, .timeMs = 1919},
        {.note = SILENCE, .timeMs = 1},
    },
    .numNotes = 128,
    .shouldLoop = false
};

bool isHigh(double period, double time)
{
    if(0 == period) {
        return false;
    }
    // printf("%s %f > %f\n", __func__, time, period);
    int numExtraPeriods = time / period;
    time -= (period * numExtraPeriods);

    while(time > period) {
        // printf("%s %f > %f\n", __func__, time, period);
        time -= period;
    }
    if(time > period / 2) {
        return true;
    }
    return false;
}

int main(int argc __attribute__((unused)), char ** argv __attribute__((unused))) {
    printf("Writing song\n");

    const song_t * toTest = &MetalGear;
    TinyWav tw;
    tinywav_open_write(&tw,
                       NUM_CHANNELS,
                       SAMPLE_RATE,
                       TW_INT16, // the output samples will be 32-bit floats. TW_INT16 is also supported
                       TW_INLINE,  // the samples will be presented inlined in a single buffer.
                       // Other options include TW_INTERLEAVED and TW_SPLIT
                       "./output.wav" // the output path
                      );

    double timeS = 0;
    for(uint32_t noteIdx = 0; noteIdx < toTest->numNotes; noteIdx++) {
        printf("f: %d, t: %dms\n", toTest->notes[noteIdx].note, toTest->notes[noteIdx].timeMs);
        double tStart = timeS;
        double period = 1 / (CLOCK_NUMERATOR * (1 / (double)toTest->notes[noteIdx].note));
        double noteTimeS = toTest->notes[noteIdx].timeMs / (double)1000;
        while(timeS - tStart < noteTimeS) {
            // printf("%f < %f\n", timeS - tStart, noteTimeS);
            timeS += (1/(double)SAMPLE_RATE);
            float samp;

            if(isHigh(period, timeS)) {
                samp = 1;
            } else {
                samp = -1;
            }
            tinywav_write_f(&tw, &samp, 1);
        }
    }

    tinywav_close_write(&tw);
}