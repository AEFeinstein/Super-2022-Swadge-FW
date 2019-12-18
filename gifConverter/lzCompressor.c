// Scott Kuhl

// fts() traverses a directory hierarchy. It works on Linux, Mac OS X,
// and the BSDs.
//
// fts() is the recommended way to traverse directories on Mac OS X
// because fts() is considered "legacy".
//
// A POSIX standard function which provides similar functionality is
// demonstrated in ftw.c

#define _DEFAULT_SOURCE // required to make this program work on Linux
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // for getcwd()

#include <sys/errno.h> // lets us directly access errno

// includes recommended by "man fts":
#include <sys/types.h>
#include <sys/stat.h>
#include <fts.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>

#include "fastlz.h"

int main(void)
{
    /* An array of paths to traverse. Each path must be null
     * terminated and the list must end with a NULL pointer. */
    char* paths[] = { "bins", NULL };

    /* 2nd parameter: An options parameter. Must include either
       FTS_PHYSICAL or FTS_LOGICAL---they change how symbolic links
       are handled.
       The 2nd parameter can also include the FTS_NOCHDIR bit (with a
       bitwise OR) which causes fts to skip changing into other
       directories. I.e., fts will call chdir() to literally cause
       your program to behave as if it is running into another
       directory until it exits that directory. See "man fts" for more
       information.
       Last parameter is a comparator which you can optionally provide
       to change the traversal of the filesystem hierarchy.
    */
    FTS* ftsp = fts_open(paths, FTS_PHYSICAL, NULL);
    if(ftsp == NULL)
    {
        perror("fts_open");
        exit(EXIT_FAILURE);
    }

    int totalCompressedSize = 0;
    FILE* galleryImagesC = fopen("galleryImages.c", "w+");
    FILE* galleryImagesH = fopen("galleryImages.h", "w+");

    fprintf(galleryImagesH, "#ifndef _GALLERY_IMAGES_H_\n");
    fprintf(galleryImagesH, "#define _GALLERY_IMAGES_H_\n\n");
    fprintf(galleryImagesH, "#include <osapi.h>\n\n");

    fprintf(galleryImagesC, "#include \"galleryImages.h\"\n\n");

    while(1) // call fts_read() enough times to get each file
    {
        FTSENT* ent = fts_read(ftsp); // get next entry (could be file or directory).
        if(ent == NULL)
        {
            if(errno == 0)
            {
                break;    // No more items, bail out of while loop
            }
            else
            {
                // fts_read() had an error.
                perror("fts_read");
                exit(EXIT_FAILURE);
            }
        }

        // Given a "entry", determine if it is a file or directory
        if(ent->fts_info & FTS_F) // The entry is a file.
        {
            if(0 == memcmp(rindex(ent->fts_accpath, '.'), ".bin", 4))
            {
                char fPathMem[64] = {0};
                char* fPath = fPathMem;
                sprintf(fPath, "./%s", ent->fts_accpath);
                // printf("%s\n", fPath);
                FILE* binFile = fopen(fPath, "r");
                if (NULL != binFile)
                {
                    // Clip off the extension
                    fPath = rindex(fPath, '/') + 1;
                    *(rindex(fPath, '.')) = 0;


                    if(strstr(fPath, "_00") != NULL)
                    {
                        char name[64] = {0};
                        strcat(name, fPath);
                        name[strlen(name) - 3] = 0;

                        printf("    }\n");
                        printf("};\n");
                        printf("\n");
                        printf("const galImage_t %s =\n", name);
                        printf("{\n");
                        printf("    .nFrames = XXX,\n");
                        printf("    .continousPan = NONE,\n");
                        printf("    .frames = {\n");
                    }
                    printf("        {.data = gal_%s, .len = sizeof(gal_%s)},\n", fPath, fPath);

                    // printf("opened %s.bin\n", fPath);
                    fseek(binFile, 0, SEEK_END);
                    long fsize = ftell(binFile);
                    fseek(binFile, 0, SEEK_SET); /* same as rewind(f); */
                    // printf("  %s.bin is %ld bytes long\n", fPath, fsize);

                    uint8_t* bytes = malloc(fsize);
                    fread(bytes, sizeof(uint8_t), fsize, binFile);

                    fclose(binFile);

                    uint8_t* compressedBytes = (uint8_t*)malloc(fsize);
                    uint32_t compressedLen = fastlz_compress(bytes, fsize, compressedBytes);

                    // printf("  Compressed %ld bytes into %d bytes\n", fsize, compressedLen);
                    totalCompressedSize += compressedLen;

                    // Pad it out
                    uint32_t paddedLen = compressedLen;
                    // while(paddedLen % 4 != 0)
                    // {
                    //     paddedLen++;
                    // }

                    fprintf(galleryImagesH, "extern uint8_t gal_%s[%d] RODATA_ATTR;\n", fPath, paddedLen);
                    fprintf(galleryImagesC, "uint8_t gal_%s[%d] RODATA_ATTR = {", fPath, paddedLen);
                    for (uint32_t bIndex = 0; bIndex < paddedLen; bIndex++)
                    {
                        if (bIndex % 16 == 0)
                        {
                            fprintf(galleryImagesC, "\n    ");
                        }
                        if(bIndex < compressedLen)
                        {
                            fprintf(galleryImagesC, "0x%02x, ", compressedBytes[bIndex]);
                        }
                        else
                        {
                            fprintf(galleryImagesC, "0x%02x, ", 0);
                        }
                    }
                    fprintf(galleryImagesC, "};\n\n");

                    free(bytes);
                    free(compressedBytes);
                }
            }
        }
    }

    // printf("totalCompressedSize = %d\n", totalCompressedSize);

    fprintf(galleryImagesH, "\n#endif // _GALLERY_IMAGES_H_\n");
    fclose(galleryImagesC);
    fclose(galleryImagesH);

    // close fts and check for error closing.
    if(fts_close(ftsp) == -1)
    {
        perror("fts_close");
    }
    return 0;
}
