#ifndef _BUZZER_H_
#define _BUZZER_H_

#include <stdbool.h>
#include <stdint.h>

// These counts (essential period of notes) are (5,000,000 / (2 * frequency))
// Negative values n are used by mode_music.c to play the current selected note
//    adjusted by n + 1201 cent of an octave
typedef enum
{
    SILENCE = 0,
    C_0 = 152905,
    C_SHARP_0 = 144342,
    D_0 = 136240,
    D_SHARP_0 = 128535,
    E_0 = 121359,
    F_0 = 114521,
    F_SHARP_0 = 108131,
    G_0 = 102041,
    G_SHARP_0 = 96302,
    A_0 = 90909,
    A_SHARP_0 = 85793,
    B_0 = 80985,
    C_1 = 76453,
    C_SHARP_1 = 72150,
    D_1 = 68101,
    D_SHARP_1 = 64284,
    E_1 = 60680,
    F_1 = 57274,
    F_SHARP_1 = 54054,
    G_1 = 51020,
    G_SHARP_1 = 48160,
    A_1 = 45455,
    A_SHARP_1 = 42904,
    B_1 = 40492,
    C_2 = 38220,
    C_SHARP_2 = 36075,
    D_2 = 34051,
    D_SHARP_2 = 32142,
    E_2 = 30336,
    F_2 = 28634,
    F_SHARP_2 = 27027,
    G_2 = 25510,
    G_SHARP_2 = 24078,
    A_2 = 22727,
    A_SHARP_2 = 21452,
    B_2 = 20248,
    C_3 = 19112,
    C_SHARP_3 = 18039,
    D_3 = 17026,
    D_SHARP_3 = 16071,
    E_3 = 15169,
    F_3 = 14318,
    F_SHARP_3 = 13514,
    G_3 = 12755,
    G_SHARP_3 = 12039,
    A_3 = 11364,
    A_SHARP_3 = 10726,
    B_3 = 10124,
    C_4 = 9555,
    C_SHARP_4 = 9019,
    D_4 = 8513,
    D_SHARP_4 = 8035,
    E_4 = 7584,
    F_4 = 7159,
    F_SHARP_4 = 6757,
    G_4 = 6378,
    G_SHARP_4 = 6020,
    A_4 = 5682,
    A_SHARP_4 = 5363,
    B_4 = 5062,
    C_5 = 4778,
    C_SHARP_5 = 4510,
    D_5 = 4257,
    D_SHARP_5 = 4018,
    E_5 = 3792,
    F_5 = 3579,
    F_SHARP_5 = 3378,
    G_5 = 3189,
    G_SHARP_5 = 3010,
    A_5 = 2841,
    A_SHARP_5 = 2681,
    B_5 = 2531,
    C_6 = 2389,
    C_SHARP_6 = 2255,
    D_6 = 2128,
    D_SHARP_6 = 2009,
    E_6 = 1896,
    F_6 = 1790,
    F_SHARP_6 = 1689,
    G_6 = 1594,
    G_SHARP_6 = 1505,
    A_6 = 1420,
    A_SHARP_6 = 1341,
    B_6 = 1265,
    C_7 = 1194,
    C_SHARP_7 = 1127,
    D_7 = 1064,
    D_SHARP_7 = 1004,
    E_7 = 948,
    F_7 = 895,
    F_SHARP_7 = 845,
    G_7 = 797,
    G_SHARP_7 = 752,
    A_7 = 710,
    A_SHARP_7 = 670,
    B_7 = 633,
    C_8 = 597,
    C_SHARP_8 = 564,
    D_8 = 532,
    D_SHARP_8 = 502,
    E_8 = 474,
    F_8 = 447,
    F_SHARP_8 = 422,
    G_8 = 399,
    G_SHARP_8 = 376,
    A_8 = 355,
    A_SHARP_8 = 335,
    B_8 = 316,
} notePeriod_t;

typedef struct
{
    uint32_t timeMs;
    notePeriod_t note;
} musicalNote_t;

typedef struct
{
    uint32_t shouldLoop;
    uint32_t numNotes;
    musicalNote_t notes[];
} song_t;

#endif
