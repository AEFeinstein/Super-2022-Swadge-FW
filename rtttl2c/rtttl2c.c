/*============================================================================
 * Includes
 *==========================================================================*/

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "../firmware/user/buzzer.h"

/*============================================================================
 * Defines
 *==========================================================================*/

#define SILENCE_STR "SILENCE"
#define INTER_NOTE_SILENCE_MS 0

/*============================================================================
 * Enums
 *==========================================================================*/

typedef enum
{
    PARSING_NAME,
    PARSING_METADATA,
    PARSING_NOTES,
} rtttlState_t;

typedef enum
{
    DURATION,
    OCTAVE,
    BPM,
} rtttlMetadata_t;

/*============================================================================
 * Prototypes
 *==========================================================================*/

char*   parseNote (char* p, int defOctave, int defDuration, int bpm, FILE* outFileC);

/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * @brief Given an RTTTL string as an argument, print the C representation
 *
 * @param argc The number of arguments, must be 2
 * @param argv The arguments, first the program name, then the RTTTL string
 * @return int 0
 */
int main(int argc, char** argv)
{
    if(2 > argc)
    {
        printf("Please pass in an RTTTL string\n");
        return 0;
    }

    FILE* outFileC = fopen("songs.c", "w+");
    fprintf(outFileC, "#include <songs.h>\n\n");

    FILE* outFileH = fopen("songs.h", "w+");
    fprintf(outFileH, "#include <osapi.h>\n");
    fprintf(outFileH, "#include \"buzzer.h\"\n");
    fprintf(outFileH, "#include \"hpatimer.h\"\n");
    fprintf(outFileH, "#include \"user_main.h\"\n");

    char names[50][32] = {{0}};

    int argidx;
    for(argidx = 1; argidx < argc; argidx++)
    {
        // Open the file
        FILE* inFile = fopen(argv[argidx], "r");
        // Get the length
        fseek(inFile, 0L, SEEK_END);
        long sz = ftell(inFile);
        fseek(inFile, 0L, SEEK_SET);
        // Read the file
        char* ringtoneMem = (char*)malloc(sz);
        fread(ringtoneMem, sz, 1, inFile);
        // Close the file
        fclose(inFile);

        char name[32] = {0};
        int nameIdx = 0;
        int duration, octave, bpm;
        int numNotes = 0;

        rtttlState_t rtttlState = PARSING_NAME;
        rtttlMetadata_t metadata = DURATION;

        // Iterate through each character, parsing it out one at a time
        char* ringtone = ringtoneMem;
        for(char ch = *ringtone; ch != '\0'; ringtone++, ch = *ringtone)
        {
            switch(rtttlState)
            {
                case PARSING_NAME:
                {
                    if(':' == ch)
                    {
                        fprintf(outFileC, "const song_t %s RODATA_ATTR = {\n", name);
                        fprintf(outFileC, "    .notes = {\n");
                        strcat(names[argidx - 1], name);
                        rtttlState = PARSING_METADATA;
                    }
                    else
                    {
                        name[nameIdx++] = ch;
                    }
                    break;
                }
                case PARSING_METADATA:
                {
                    switch (ch)
                    {
                        case ':':
                        {
                            rtttlState = PARSING_NOTES;
                            break;
                        }
                        case 'D':
                        case 'd':
                        {
                            metadata = DURATION;
                            duration = 0;
                            break;
                        }
                        case 'O':
                        case 'o':
                        {
                            metadata = OCTAVE;
                            octave = 0;
                            break;
                        }
                        case 'B':
                        case 'b':
                        {
                            metadata = BPM;
                            bpm = 0;
                            break;
                        }
                        case '=':
                        {
                            break;
                        }
                        case '0':
                        case '1':
                        case '2':
                        case '3':
                        case '4':
                        case '5':
                        case '6':
                        case '7':
                        case '8':
                        case '9':
                        {
                            switch (metadata)
                            {
                                case OCTAVE:
                                {
                                    octave *= 10;
                                    octave += (ch - '0');
                                    break;
                                }
                                case DURATION:
                                {
                                    duration *= 10;
                                    duration += (ch - '0');
                                    break;
                                }
                                case BPM:
                                {
                                    bpm *= 10;
                                    bpm += (ch - '0');
                                    break;
                                }
                            }
                            break;
                        }
                        default:
                        {
                            break;
                        }
                    }
                    break;
                }
                case PARSING_NOTES:
                {
                    ringtone = parseNote(ringtone, octave, duration, bpm, outFileC);
                    if(INTER_NOTE_SILENCE_MS > 0)
                    {
                        numNotes += 2; // One for the note, one for silence
                    }
                    else
                    {
                        numNotes++;
                    }
                    if('\0' == *ringtone)
                    {
                        goto ALL_DONE;
                    }
                    break;
                }
            }
        }

ALL_DONE:
        fprintf(outFileC, "    },\n");
        fprintf(outFileC, "    .numNotes = %d,\n", numNotes);
        fprintf(outFileC, "    .shouldLoop = false\n");
        fprintf(outFileC, "};\n\n");
        free(ringtoneMem);
    }

    fprintf(outFileC, "const song_t * allSongs[] = {\n");
    for(int i = 0; i < argidx - 1; i++)
    {
        fprintf(outFileC, "\t&%s,\n", names[i]);
    }
    fprintf(outFileC, "};\n\n");

    fprintf(outFileH, "const song_t* ICACHE_FLASH_ATTR getRandSong(void);\n");
    fprintf(outFileC, "const song_t* ICACHE_FLASH_ATTR getRandSong(void)\n");
    fprintf(outFileC, "{\n");
    fprintf(outFileC, "    return allSongs[(os_random() %% (sizeof(allSongs) / sizeof(allSongs[0])))];\n");
    fprintf(outFileC, "}\n");

    fclose(outFileC);
    fclose(outFileH);
}

/**
 * @brief Parse a note from the string
 *
 * @param p           The string to parse a note from
 * @param defOctave   The default octave of this note
 * @param defDuration The default duration of this note
 * @param bpm         The beats per minute of the song
 * @return char*      A pointer to the string after the note
 */
char*   parseNote (char* p, int defOctave, int defDuration, int bpm, FILE* outFileC)
{
    int octave = defOctave;
    int octaveMod = 0;
    float duration = defDuration;
    char noteName[32] = {0};

    // Skip whitespace
    while (*p == ' ')
    {
        p++;
    }
    if (!*p)
    {
        return p;
    }

    // Parse duration
    if (*p >= '0' && *p <= '9')
    {
        duration = 0;
        while (*p >= '0' && *p <= '9')
        {
            duration = duration * 10 + (*p++ - '0');
        }
    }

    // Parse note
    if('A' <= *p && *p <= 'G')
    {
        noteName[0] = *p;
    }
    else if('a' <= *p && *p <= 'g')
    {
        noteName[0] = *p - 'a' + 'A';
    }
    else if('p' == *p || 'P' == *p)
    {
        strcat(noteName, SILENCE_STR);
    }
    else
    {
        return p;
    }
    p++;

    // Parse modifier
    if (*p == '#')
    {
        strcat(noteName, "_SHARP");
        p++;
    }
    if (*p == 'b')
    {
        // There are no flats, make it the lower note's sharp
        noteName[0]--;
        if(noteName[0] < 'A')
        {
            noteName[0] = 'G';
            octaveMod--;
        }
        strcat(noteName, "_SHARP");
        p++;
    }

    // Parse special duration
    while (*p == '.')
    {
        duration = 1 / ((1 / duration) + (1 / (duration * 2)));
        p++;
    }

    // Parse octave
    if (*p >= '0' && *p <= '9')
    {
        octave = (*p++ - '0');
    }

    // Append the octave
    if(0 != memcmp(noteName, SILENCE_STR, sizeof(SILENCE_STR)))
    {
        sprintf(&noteName[strlen(noteName)], "_%d", (octave + octaveMod));
    }

    // Parse special duration (again...)
    while (*p == '.')
    {
        duration = 1 / ((1 / duration) + (1 / (duration * 2)));
        p++;
    }

    // Skip delimiter
    while (*p == ' ')
    {
        p++;
    }

    // Print note
    int noteMs = round(240000 / (float)(bpm * duration)) - INTER_NOTE_SILENCE_MS;
    fprintf(outFileC, "        {.note = %s, .timeMs = %d},\n", noteName, noteMs);
    if(INTER_NOTE_SILENCE_MS > 0)
    {
        fprintf(outFileC, "        {.note = %s, .timeMs = %d},\n", SILENCE_STR, INTER_NOTE_SILENCE_MS);
    }
    return p;
}
