#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <strings.h>

#include "fastlz.h"

int main(int argc, char** argv)
{
    int totalCompressedSize = 0;
    FILE* galleryImagesC = fopen("galleryImages.c", "w+");
    FILE* galleryImagesH = fopen("galleryImages.h", "w+");

    fprintf(galleryImagesH, "#ifndef _GALLERY_IMAGES_H_\n");
    fprintf(galleryImagesH, "#define _GALLERY_IMAGES_H_\n\n");
    fprintf(galleryImagesH, "#include <osapi.h>\n\n");

    fprintf(galleryImagesC, "#include \"galleryImages.h\"\n\n");

    for (int fIndex = 1; fIndex < argc; fIndex++)
    {
        FILE* binFile = fopen(argv[fIndex], "r");
        if (NULL != binFile)
        {
            // Clip off the extension
            argv[fIndex] = rindex(argv[fIndex], '/') + 1;
            *(rindex(argv[fIndex], '.')) = 0;

            printf("opened %s.bin\n", argv[fIndex]);
            fseek(binFile, 0, SEEK_END);
            long fsize = ftell(binFile);
            fseek(binFile, 0, SEEK_SET); /* same as rewind(f); */
            printf("  %s.bin is %ld bytes long\n", argv[fIndex], fsize);

            uint8_t* bytes = malloc(fsize);
            fread(bytes, sizeof(uint8_t), fsize, binFile);

            fclose(binFile);

            uint8_t* compressedBytes = (uint8_t*)malloc(fsize);
            int compressedLen = fastlz_compress(bytes, fsize, compressedBytes);

            printf("  Compressed %ld bytes into %d bytes\n", fsize, compressedLen);
            totalCompressedSize += compressedLen;

            fprintf(galleryImagesH, "extern uint8_t gal_%s[%d] ICACHE_RODATA_ATTR;\n", argv[fIndex], compressedLen);
            fprintf(galleryImagesC, "uint8_t gal_%s[%d] ICACHE_RODATA_ATTR = {", argv[fIndex], compressedLen);
            for (int bIndex = 0; bIndex < compressedLen; bIndex++)
            {
                if (bIndex % 16 == 0)
                {
                    fprintf(galleryImagesC, "\n    ");
                }
                fprintf(galleryImagesC, "0x%02x, ", compressedBytes[bIndex]);
            }
            fprintf(galleryImagesC, "};\n\n");

            free(bytes);
            free(compressedBytes);
        }
    }
    printf("totalCompressedSize = %d\n", totalCompressedSize);

    fprintf(galleryImagesH, "\n#endif // _GALLERY_IMAGES_H_\n");
    fclose(galleryImagesC);
    fclose(galleryImagesH);
}