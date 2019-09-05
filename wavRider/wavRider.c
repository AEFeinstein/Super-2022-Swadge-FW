#include "wavRider.h"
#include "tinywav.h"
#include "../firmware/user/buzzer.h"

#define NUM_CHANNELS 1
#define SAMPLE_RATE 48000

#define CLOCK_NUMERATOR 5000000

const song_t testSong = {
    .numNotes = 9,
    .notes = {
        {.note = C_4, .timeMs = 250},
        {.note = D_4, .timeMs = 250},
        {.note = E_4, .timeMs = 250},
        {.note = F_4, .timeMs = 250},
        {.note = G_4, .timeMs = 250},
        {.note = F_4, .timeMs = 250},
        {.note = E_4, .timeMs = 250},
        {.note = D_4, .timeMs = 250},
        {.note = C_4, .timeMs = 250},
    }
};

bool isHigh(double period, double time)
{
    // printf("%s %f > %f\n", __func__, time, period);
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
    for(uint32_t noteIdx = 0; noteIdx < testSong.numNotes; noteIdx++) {
        printf("f: %d, t: %dms\n", testSong.notes[noteIdx].note, testSong.notes[noteIdx].timeMs);
        double tStart = timeS;
        double period = 1 / (CLOCK_NUMERATOR * (1 / (double)testSong.notes[noteIdx].note));
        double noteTimeS = testSong.notes[noteIdx].timeMs / (double)1000;
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